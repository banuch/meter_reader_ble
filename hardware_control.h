/*
 * hardware_control.h - Hardware control interface (ESP32 Core 3.x Compatible)
 * 
 * FIXED FOR: ESP32 Arduino Core 3.2.0+ compatibility
 * CHANGES: Updated PWM setup from ledcSetup() to ledcAttach()
 */

#ifndef HARDWARE_CONTROL_H
#define HARDWARE_CONTROL_H

#include <Arduino.h>
#include "config.h"

class HardwareControl {
private:
  bool ledState;
  unsigned long lastBeepTime;
  
  void configurePWM();
  void configureGPIO();
  
public:
  HardwareControl();
  
  // Initialization
  void init();
  void startupSequence();
  
  // LED controls
  void ledOn();
  void ledOff();
  void ledToggle();
  void ledBlink(int count, int delayMs = 100);
  
  // Buzzer controls
  void beep();
  void doubleBeep();
  void multiBeep(int count);
  void longBeep(int durationMs = 500);
  
  // External power and switch
  void enableExternalPower();
  void disableExternalPower();
  bool isExternalSwitchPressed();
  bool isExternalSwitchHeld(unsigned long durationMs);
  
  // IRDA control
  void enableIRDA();
  void disableIRDA();
  
  // Utility functions
  void delayWithYield(unsigned long ms);
  void resetWatchdog();
  
  // Status getters
  bool isLedOn() const { return ledState; }
  unsigned long getLastBeepTime() const { return lastBeepTime; }
};

// Implementation
HardwareControl::HardwareControl() : ledState(false), lastBeepTime(0) {
}

void HardwareControl::init() {
  Serial.println("Initializing hardware control...");
  
  configureGPIO();
  configurePWM();
  
  // Set initial states
  enableExternalPower();
  enableIRDA();
  
  Serial.println("Hardware control initialized successfully");
}

void HardwareControl::configureGPIO() {
  // Configure output pins
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_BUZZER1, OUTPUT);
  pinMode(PIN_IRDA_EN, OUTPUT);
  pinMode(PIN_EXT_PW, OUTPUT);
  
  // Configure input pins
  pinMode(PIN_EXT_SW, INPUT);
  
  // Initialize output states
  digitalWrite(PIN_LED, LOW);
  digitalWrite(PIN_BUZZER, LOW);
  digitalWrite(PIN_BUZZER1, LOW);
  digitalWrite(PIN_IRDA_EN, HIGH);
  digitalWrite(PIN_EXT_PW, HIGH);
  
  Serial.println("GPIO pins configured");
}

void HardwareControl::configurePWM() {
  // Configure PWM for IRDA modulation at 38kHz
  // ESP32 Core 3.x compatible version
  WRITE_PERI_REG(0x3FF6E020, READ_PERI_REG(0x3FF6E020) | (1 << 16) | (1 << 10));
  WRITE_PERI_REG(0x3FF6E020, READ_PERI_REG(0x3FF6E020) | (1 << 16) | (1 << 9));
  
  // FIXED: Updated for ESP32 Core 3.x - use ledcAttach instead of ledcSetup + ledcAttachPin
  if (!ledcAttach(PIN_LED_PWM, PWM_FREQ, PWM_RESOLUTION)) {
    Serial.println("ERROR: Failed to attach LEDC to pin");
  } else {
    ledcWrite(PIN_LED_PWM, PWM_DUTY_CYCLE);
    Serial.println("PWM configured for IRDA modulation");
  }
}

void HardwareControl::startupSequence() {
  Serial.println("Performing startup sequence...");
  
  // LED and buzzer startup indication
  ledOn();
  doubleBeep();
  ledOff();
  
  delayWithYield(100);
  
  Serial.println("Startup sequence completed");
}

// LED controls
void HardwareControl::ledOn() {
  digitalWrite(PIN_LED, HIGH);
  ledState = true;
}

void HardwareControl::ledOff() {
  digitalWrite(PIN_LED, LOW);
  ledState = false;
}

void HardwareControl::ledToggle() {
  if (ledState) {
    ledOff();
  } else {
    ledOn();
  }
}

void HardwareControl::ledBlink(int count, int delayMs) {
  for (int i = 0; i < count; i++) {
    ledOn();
    delayWithYield(delayMs);
    ledOff();
    if (i < count - 1) {
      delayWithYield(delayMs);
    }
  }
}

// Buzzer controls
void HardwareControl::beep() {
  unsigned long currentTime = millis();
  
  // Prevent rapid consecutive beeping
  if (currentTime - lastBeepTime < 50) {
    return;
  }
  
  // Generate 38kHz modulated beep
  for (int i = 0; i < 500; i++) {
    digitalWrite(PIN_BUZZER, HIGH);
    delayMicroseconds(175);
    digitalWrite(PIN_BUZZER, LOW);
    delayMicroseconds(175);
  }
  
  lastBeepTime = currentTime;
}

void HardwareControl::doubleBeep() {
  beep();
  delayWithYield(100);
  beep();
}

void HardwareControl::multiBeep(int count) {
  for (int i = 0; i < count; i++) {
    beep();
    if (i < count - 1) {
      delayWithYield(300);
    }
  }
}

void HardwareControl::longBeep(int durationMs) {
  unsigned long startTime = millis();
  
  while (millis() - startTime < durationMs) {
    digitalWrite(PIN_BUZZER, HIGH);
    delayMicroseconds(175);
    digitalWrite(PIN_BUZZER, LOW);
    delayMicroseconds(175);
  }
}

// External power and switch
void HardwareControl::enableExternalPower() {
  digitalWrite(PIN_EXT_PW, HIGH);
}

void HardwareControl::disableExternalPower() {
  digitalWrite(PIN_EXT_PW, LOW);
}

bool HardwareControl::isExternalSwitchPressed() {
  return digitalRead(PIN_EXT_SW) == LOW;
}

bool HardwareControl::isExternalSwitchHeld(unsigned long durationMs) {
  if (!isExternalSwitchPressed()) {
    return false;
  }
  
  unsigned long startTime = millis();
  
  while (millis() - startTime < durationMs) {
    if (!isExternalSwitchPressed()) {
      return false;
    }
    delayWithYield(10);
  }
  
  return true;
}

// IRDA control
void HardwareControl::enableIRDA() {
  digitalWrite(PIN_IRDA_EN, HIGH);
}

void HardwareControl::disableIRDA() {
  digitalWrite(PIN_IRDA_EN, LOW);
}

// Utility functions
void HardwareControl::delayWithYield(unsigned long ms) {
  unsigned long start = millis();
  
  while (millis() - start < ms) {
    yield(); // Allow other tasks to run
    delay(1);
  }
}

void HardwareControl::resetWatchdog() {
  // ESP32 watchdog reset if needed
  yield();
}

#endif // HARDWARE_CONTROL_H