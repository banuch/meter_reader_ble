/*
 * power_management.h - Power management and sleep functionality (ESP32 Core 3.x Compatible)
 * 
 * FIXED FOR: ESP32 Arduino Core 3.2.0+ compatibility
 * CHANGES: Removed duplicate isBatteryCharging() function declaration
 */

#ifndef POWER_MANAGEMENT_H
#define POWER_MANAGEMENT_H

#include <Arduino.h>
#include <esp_sleep.h>
#include "config.h"
#include "hardware_control.h"

// Power states
enum PowerState {
  POWER_ACTIVE,
  POWER_IDLE,
  POWER_PREPARING_SLEEP,
  POWER_DEEP_SLEEP
};

enum WakeupReason {
  WAKEUP_EXTERNAL_BUTTON,
  WAKEUP_TIMER,
  WAKEUP_RESET,
  WAKEUP_UNKNOWN
};

// Battery information
struct BatteryInfo {
  int levelPercent;
  float voltageV;
  int rawADC;
  bool isCharging;
  bool isLow;
  unsigned long lastUpdateTime;
  
  // Constructor
  BatteryInfo() : levelPercent(0), voltageV(0.0), rawADC(0), 
                  isCharging(false), isLow(false), lastUpdateTime(0) {}
};

class PowerManager {
private:
  HardwareControl* hardware;
  
  // Sleep management
  unsigned long sleepTimer;
  unsigned long lastActivityTime;
  unsigned long sleepTimeoutMs;
  PowerState currentState;
  
  // Button handling
  unsigned long buttonPressStartTime;
  bool buttonCurrentlyPressed;
  bool buttonLongPressDetected;
  
  // Battery monitoring
  BatteryInfo battery;
  unsigned long batteryUpdateInterval;
  static const int BATTERY_SAMPLES = 50;
  
  // Wake-up handling
  WakeupReason lastWakeupReason;
  
  // Private methods
  void updateBatteryLevel();
  int calculateBatteryPercentage(int adcValue);
  float calculateBatteryVoltage(int adcValue);
  bool isBatteryChargingInternal(); // FIXED: Renamed to avoid duplicate declaration
  
  // Sleep detection
  bool checkManualSleepTrigger();
  bool checkAutoSleepCondition();
  bool checkLowBatteryCondition();
  
  // Button handling
  void updateButtonState();
  bool isButtonPressed();
  
  // Power state management
  void setState(PowerState newState);
  void logStateChange(PowerState oldState, PowerState newState);
  
  // Wake-up analysis
  WakeupReason determineWakeupReason();
  void logWakeupReason(WakeupReason reason);
  
public:
  PowerManager();
  
  // Initialization
  void init();
  void setHardwareControl(HardwareControl* hwCtrl) { hardware = hwCtrl; }
  
  // Main update function
  void update();
  
  // Sleep management
  bool shouldSleep();
  void enterDeepSleep();
  void prepareSleep();
  void resetSleepTimer();
  void extendSleepTimer(unsigned long additionalMs);
  
  // Activity tracking
  void recordActivity();
  unsigned long getTimeSinceLastActivity();
  unsigned long getSleepTimeRemaining();
  
  // Battery management
  int getBatteryLevel();
  float getBatteryVoltage();
  bool isBatteryLow();
  bool isBatteryCharging(); // FIXED: Single declaration only
  BatteryInfo getBatteryInfo();
  void forceBatteryUpdate();
  
  // Configuration
  void setSleepTimeout(unsigned long timeoutMs);
  unsigned long getSleepTimeout() const { return sleepTimeoutMs; }
  void setBatteryUpdateInterval(unsigned long intervalMs);
  
  // Status getters
  PowerState getCurrentState() const { return currentState; }
  WakeupReason getLastWakeupReason() const { return lastWakeupReason; }
  bool isButtonLongPressed() const { return buttonLongPressDetected; }
  
  // Diagnostics
  void printPowerStatus();
  void printBatteryStatus();
  void printSleepDiagnostics();
};

// Implementation
PowerManager::PowerManager() 
  : hardware(nullptr), sleepTimer(0), lastActivityTime(0), 
    sleepTimeoutMs(SLEEP_TIMEOUT_MS), currentState(POWER_ACTIVE),
    buttonPressStartTime(0), buttonCurrentlyPressed(false), 
    buttonLongPressDetected(false), batteryUpdateInterval(30000),
    lastWakeupReason(WAKEUP_UNKNOWN) {
}

void PowerManager::init() {
  Serial.println("Initializing power manager...");
  
  // Configure wake-up source for deep sleep
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_33, 0); // Wake on low signal (button press)
  
  // Initialize timers
  lastActivityTime = millis();
  sleepTimer = 0;
  
  // Determine wake-up reason
  lastWakeupReason = determineWakeupReason();
  logWakeupReason(lastWakeupReason);
  
  // Initialize battery monitoring
  battery.lastUpdateTime = 0;
  updateBatteryLevel();
  
  // Set initial state
  setState(POWER_ACTIVE);
  
  Serial.println("Power manager initialized successfully");
  Serial.println("Sleep timeout: " + String(sleepTimeoutMs / 1000) + " seconds");
  Serial.println("Initial battery level: " + String(battery.levelPercent) + "%");
}

void PowerManager::update() {
  // Update battery level periodically
  if (millis() - battery.lastUpdateTime > batteryUpdateInterval) {
    updateBatteryLevel();
  }
  
  // Update button state
  updateButtonState();
  
  // Update sleep timer
  sleepTimer = millis() - lastActivityTime;
  
  // Update power state based on conditions
  PowerState oldState = currentState;
  
  if (checkManualSleepTrigger() || checkLowBatteryCondition()) {
    setState(POWER_PREPARING_SLEEP);
  } else if (checkAutoSleepCondition()) {
    setState(POWER_IDLE);
    if (sleepTimer > sleepTimeoutMs) {
      setState(POWER_PREPARING_SLEEP);
    }
  } else {
    setState(POWER_ACTIVE);
  }
  
  // Log state changes
  if (oldState != currentState) {
    logStateChange(oldState, currentState);
  }
}

bool PowerManager::shouldSleep() {
  return (currentState == POWER_PREPARING_SLEEP);
}

bool PowerManager::checkManualSleepTrigger() {
  if (!hardware) return false;
  
  // Check for long button press (manual sleep trigger)
  if (buttonLongPressDetected) {
    Serial.println("Manual sleep triggered by long button press");
    return true;
  }
  
  return false;
}

bool PowerManager::checkAutoSleepCondition() {
  return (sleepTimer > sleepTimeoutMs * 0.8); // Start preparing at 80% of timeout
}

bool PowerManager::checkLowBatteryCondition() {
  return battery.isLow && (battery.levelPercent < 10);
}

void PowerManager::prepareSleep() {
  Serial.println("Preparing for deep sleep...");
  
  setState(POWER_PREPARING_SLEEP);
  
  if (hardware) {
    // Visual and audio indication of sleep
    hardware->multiBeep(5);
    hardware->ledBlink(3, 200);
    
    // Disable external power to save energy
    hardware->disableExternalPower();
    
    // Small delay to ensure power state is stable
    hardware->delayWithYield(100);
  }
}

void PowerManager::enterDeepSleep() {
  prepareSleep();
  
  Serial.println("Entering deep sleep mode...");
  Serial.println("Device will wake on button press");
  Serial.flush(); // Ensure message is sent before sleep
  
  // Enter deep sleep
  esp_deep_sleep_start();
  
  // This point should never be reached, but just in case:
  Serial.println("ERROR: Failed to enter deep sleep!");
  delay(1000);
  
  if (hardware) {
    hardware->multiBeep(4);
    hardware->enableExternalPower(); // Re-enable power
  }
}

void PowerManager::resetSleepTimer() {
  lastActivityTime = millis();
  sleepTimer = 0;
  
  if (currentState == POWER_IDLE || currentState == POWER_PREPARING_SLEEP) {
    setState(POWER_ACTIVE);
  }
}

void PowerManager::extendSleepTimer(unsigned long additionalMs) {
  lastActivityTime += additionalMs;
}

void PowerManager::recordActivity() {
  resetSleepTimer();
}

unsigned long PowerManager::getTimeSinceLastActivity() {
  return millis() - lastActivityTime;
}

unsigned long PowerManager::getSleepTimeRemaining() {
  unsigned long elapsed = getTimeSinceLastActivity();
  if (elapsed >= sleepTimeoutMs) {
    return 0;
  }
  return sleepTimeoutMs - elapsed;
}

// Battery management
void PowerManager::updateBatteryLevel() {
  int totalReading = 0;
  
  // Take multiple samples for accuracy
  for (int i = 0; i < BATTERY_SAMPLES; i++) {
    totalReading += analogRead(PIN_BATTERY);
    delay(2); // Small delay between readings
  }
  
  int averageReading = totalReading / BATTERY_SAMPLES;
  
  // Update battery information
  battery.rawADC = averageReading;
  battery.levelPercent = calculateBatteryPercentage(averageReading);
  battery.voltageV = calculateBatteryVoltage(averageReading);
  battery.isCharging = isBatteryChargingInternal();
  battery.isLow = (battery.levelPercent < 20);
  battery.lastUpdateTime = millis();
  
  // Ensure battery level is within valid range
  if (battery.levelPercent < 0) battery.levelPercent = 0;
  if (battery.levelPercent > 100) battery.levelPercent = 100;
  
  // Log battery status changes
  static int lastLoggedLevel = -1;
  if (abs(battery.levelPercent - lastLoggedLevel) >= 5) {
    Serial.println("Battery level: " + String(battery.levelPercent) + "% (" + String(battery.voltageV, 2) + "V)");
    lastLoggedLevel = battery.levelPercent;
  }
}

int PowerManager::calculateBatteryPercentage(int adcValue) {
  // Convert ADC reading to percentage
  const int minADC = 2000;  // Corresponds to ~3.0V (empty)
  const int maxADC = 3400;  // Corresponds to ~4.2V (full)
  
  if (adcValue <= minADC) return 0;
  if (adcValue >= maxADC) return 100;
  
  // Linear mapping (could be improved with proper battery discharge curve)
  return map(adcValue, minADC, maxADC, 0, 100);
}

float PowerManager::calculateBatteryVoltage(int adcValue) {
  // Convert ADC reading to voltage
  float voltage = (adcValue * 3.3f) / 4095.0f;
  
  // If using voltage divider, multiply by the ratio
  voltage *= 2.0f; // Adjust this multiplier based on your circuit
  
  return voltage;
}

bool PowerManager::isBatteryChargingInternal() {
  // This would require additional hardware to detect charging
  // For now, return false. Implement based on your charging circuit
  return false;
}

int PowerManager::getBatteryLevel() {
  return battery.levelPercent;
}

float PowerManager::getBatteryVoltage() {
  return battery.voltageV;
}

bool PowerManager::isBatteryLow() {
  return battery.isLow;
}

bool PowerManager::isBatteryCharging() {
  return battery.isCharging;
}

BatteryInfo PowerManager::getBatteryInfo() {
  return battery;
}

void PowerManager::forceBatteryUpdate() {
  updateBatteryLevel();
}

// Button handling
void PowerManager::updateButtonState() {
  if (!hardware) return;
  
  bool currentlyPressed = hardware->isExternalSwitchPressed();
  
  if (currentlyPressed && !buttonCurrentlyPressed) {
    // Button just pressed
    buttonCurrentlyPressed = true;
    buttonPressStartTime = millis();
    buttonLongPressDetected = false;
  } else if (!currentlyPressed && buttonCurrentlyPressed) {
    // Button released
    buttonCurrentlyPressed = false;
    buttonLongPressDetected = false;
  } else if (currentlyPressed && buttonCurrentlyPressed) {
    // Button held down - check for long press
    unsigned long pressDuration = millis() - buttonPressStartTime;
    if (pressDuration > BUTTON_DEBOUNCE_MS && !buttonLongPressDetected) {
      buttonLongPressDetected = true;
      Serial.println("Long button press detected (" + String(pressDuration) + "ms)");
    }
  }
}

bool PowerManager::isButtonPressed() {
  return buttonCurrentlyPressed;
}

// Power state management
void PowerManager::setState(PowerState newState) {
  if (currentState != newState) {
    currentState = newState;
  }
}

void PowerManager::logStateChange(PowerState oldState, PowerState newState) {
  String oldStateStr, newStateStr;
  
  switch (oldState) {
    case POWER_ACTIVE: oldStateStr = "ACTIVE"; break;
    case POWER_IDLE: oldStateStr = "IDLE"; break;
    case POWER_PREPARING_SLEEP: oldStateStr = "PREPARING_SLEEP"; break;
    case POWER_DEEP_SLEEP: oldStateStr = "DEEP_SLEEP"; break;
  }
  
  switch (newState) {
    case POWER_ACTIVE: newStateStr = "ACTIVE"; break;
    case POWER_IDLE: newStateStr = "IDLE"; break;
    case POWER_PREPARING_SLEEP: newStateStr = "PREPARING_SLEEP"; break;
    case POWER_DEEP_SLEEP: newStateStr = "DEEP_SLEEP"; break;
  }
  
  Serial.println("Power state: " + oldStateStr + " -> " + newStateStr);
}

// Wake-up analysis
WakeupReason PowerManager::determineWakeupReason() {
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  
  switch (wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0:
      return WAKEUP_EXTERNAL_BUTTON;
    case ESP_SLEEP_WAKEUP_TIMER:
      return WAKEUP_TIMER;
    case ESP_SLEEP_WAKEUP_UNDEFINED:
    default:
      return WAKEUP_RESET;
  }
}

void PowerManager::logWakeupReason(WakeupReason reason) {
  String reasonStr;
  
  switch (reason) {
    case WAKEUP_EXTERNAL_BUTTON:
      reasonStr = "External Button";
      break;
    case WAKEUP_TIMER:
      reasonStr = "Timer";
      break;
    case WAKEUP_RESET:
      reasonStr = "Reset/Power-on";
      break;
    default:
      reasonStr = "Unknown";
      break;
  }
  
  Serial.println("Wake-up reason: " + reasonStr);
}

// Configuration
void PowerManager::setSleepTimeout(unsigned long timeoutMs) {
  sleepTimeoutMs = timeoutMs;
  Serial.println("Sleep timeout set to: " + String(timeoutMs / 1000) + " seconds");
}

void PowerManager::setBatteryUpdateInterval(unsigned long intervalMs) {
  batteryUpdateInterval = intervalMs;
  Serial.println("Battery update interval set to: " + String(intervalMs / 1000) + " seconds");
}

// Diagnostics
void PowerManager::printPowerStatus() {
  Serial.println("=== Power Status ===");
  Serial.println("State: " + String((int)currentState));
  Serial.println("Time since activity: " + String(getTimeSinceLastActivity() / 1000) + "s");
  Serial.println("Sleep time remaining: " + String(getSleepTimeRemaining() / 1000) + "s");
  Serial.println("Button pressed: " + String(buttonCurrentlyPressed ? "YES" : "NO"));
  Serial.println("Free heap: " + String(ESP.getFreeHeap()) + " bytes");
  Serial.println("===================");
}

void PowerManager::printBatteryStatus() {
  Serial.println("=== Battery Status ===");
  Serial.println("Level: " + String(battery.levelPercent) + "%");
  Serial.println("Voltage: " + String(battery.voltageV, 2) + "V");
  Serial.println("Raw ADC: " + String(battery.rawADC));
  Serial.println("Is Low: " + String(battery.isLow ? "YES" : "NO"));
  Serial.println("Is Charging: " + String(battery.isCharging ? "YES" : "NO"));
  Serial.println("Last Update: " + String((millis() - battery.lastUpdateTime) / 1000) + "s ago");
  Serial.println("=====================");
}

void PowerManager::printSleepDiagnostics() {
  Serial.println("=== Sleep Diagnostics ===");
  Serial.println("Sleep timeout: " + String(sleepTimeoutMs / 1000) + "s");
  Serial.println("Auto sleep condition: " + String(checkAutoSleepCondition() ? "TRUE" : "FALSE"));
  Serial.println("Manual sleep trigger: " + String(checkManualSleepTrigger() ? "TRUE" : "FALSE"));
  Serial.println("Low battery condition: " + String(checkLowBatteryCondition() ? "TRUE" : "FALSE"));
  Serial.println("Should sleep: " + String(shouldSleep() ? "YES" : "NO"));
  Serial.println("=========================");
}

#endif // POWER_MANAGEMENT_H