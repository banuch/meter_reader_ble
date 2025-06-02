/*
 * communication.h - Communication management (ESP32 Core 3.x Compatible)
 * 
 * FIXED FOR: ESP32 Arduino Core 3.2.0+ compatibility
 * CHANGES: Replaced cleanAPlist() with WiFi.disconnect()
 */

#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include <Arduino.h>
#include <BluetoothSerial.h>
#include <HardwareSerial.h>
#include <WiFiMulti.h>
#include "config.h"

class CommunicationManager {
private:
  BluetoothSerial* bluetoothSerial;
  HardwareSerial* irdaSerial;
  HardwareSerial* irSerial;
  WiFiMulti* wifiMulti;
  
  String commandBuffer;
  unsigned long lastCommandTime;
  
public:
  CommunicationManager();
  ~CommunicationManager();
  
  // Initialization
  void init(const String& bluetoothName);
  
  // Bluetooth operations
  String readBluetoothCommand();
  void println(const String& message);
  void print(const String& message);
  void printChar(char c);
  bool isBluetoothConnected();
  
  // Serial communication setup
  void setupIRDASerial(int baudRate);
  void setupIRSerial(int baudRate);
  
  // WiFi operations
  bool connectWiFi(const String& ssid, const String& password);
  void disconnectWiFi();
  bool isWiFiConnected();
  String getWiFiIP();
  
  // Data output helpers
  void printConfig(const ConfigManager& config);
  void printBatteryStatus(int batteryLevel);
  void printDataReceived(const String& meterType);
  void printRawData(const String& data);
  void printSystemStatus();
  
  // Hardware serial access
  HardwareSerial* getIRDASerial() { return irdaSerial; }
  HardwareSerial* getIRSerial() { return irSerial; }
  
  // Utility
  void flush();
  size_t available();
  void clearBuffers();
};

// Implementation
CommunicationManager::CommunicationManager() 
  : bluetoothSerial(nullptr), irdaSerial(nullptr), irSerial(nullptr), 
    wifiMulti(nullptr), lastCommandTime(0) {
}

CommunicationManager::~CommunicationManager() {
  delete bluetoothSerial;
  delete wifiMulti;
}

void CommunicationManager::init(const String& bluetoothName) {
  Serial.println("Initializing communication manager...");
  
  // Initialize Bluetooth
  bluetoothSerial = new BluetoothSerial();
  
  #if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
    Serial.println("ERROR: Bluetooth is not enabled!");
    return;
  #endif
  
  if (!bluetoothSerial->begin(bluetoothName)) {
    Serial.println("ERROR: Bluetooth initialization failed!");
    return;
  }
  
  bluetoothSerial->setTimeout(BT_TIMEOUT);
  Serial.println("Bluetooth initialized: " + bluetoothName);
  
  // Initialize hardware serials
  irdaSerial = &Serial2;
  irSerial = &Serial1;
  
  // Setup serial ports with default settings
  setupIRDASerial(BAUD_RATE_9600);
  setupIRSerial(BAUD_RATE_2400);
  
  // Initialize WiFi
  wifiMulti = new WiFiMulti();
  
  Serial.println("Communication manager initialized successfully");
}

String CommunicationManager::readBluetoothCommand() {
  String command = "";
  unsigned long startTime = millis();
  
  // Read all available characters within timeout
  while (bluetoothSerial->available() && (millis() - startTime < 100)) {
    char c = bluetoothSerial->read();
    
    // Filter out newlines and carriage returns
    if (c != '\n' && c != '\r' && isPrintable(c)) {
      command += c;
    }
    
    delay(1); // Small delay for buffer filling
  }
  
  // Record command reception time and echo back
  if (command.length() > 0) {
    lastCommandTime = millis();
    bluetoothSerial->println("CMD: " + command);
    Serial.println("Bluetooth command received: " + command);
  }
  
  return command;
}

void CommunicationManager::println(const String& message) {
  bluetoothSerial->println(message);
  Serial.println(message);
}

void CommunicationManager::print(const String& message) {
  bluetoothSerial->print(message);
  Serial.print(message);
}

void CommunicationManager::printChar(char c) {
  bluetoothSerial->print(c);
  Serial.print(c);
}

bool CommunicationManager::isBluetoothConnected() {
  return bluetoothSerial->hasClient();
}

void CommunicationManager::setupIRDASerial(int baudRate) {
  irdaSerial->begin(baudRate, SERIAL_8N1, PIN_RXD2, PIN_TXD2);
  irdaSerial->setTimeout(IRDA_TIMEOUT);
  
  // Configure GPIO registers for IRDA
  WRITE_PERI_REG(0x3FF6E020, READ_PERI_REG(0x3FF6E020) | (1 << 16) | (1 << 10));
  WRITE_PERI_REG(0x3FF6E020, READ_PERI_REG(0x3FF6E020) | (1 << 16) | (1 << 9));
  
  delay(50);
  irdaSerial->flush();
  
  Serial.println("IRDA Serial configured: " + String(baudRate) + " baud");
}

void CommunicationManager::setupIRSerial(int baudRate) {
  irSerial->begin(baudRate, SERIAL_8N1, PIN_RXD1, PIN_TXD1);
  irSerial->setTimeout(IRDA_TIMEOUT);
  
  delay(50);
  irSerial->flush();
  
  Serial.println("IR Serial configured: " + String(baudRate) + " baud");
}

bool CommunicationManager::connectWiFi(const String& ssid, const String& password) {
  Serial.println("Connecting to WiFi: " + ssid);
  
  // FIXED: Clear any existing AP configurations - ESP32 Core 3.x compatible
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  
  // Add the configured network
  wifiMulti->addAP(ssid.c_str(), password.c_str());
  
  // Attempt to connect with timeout
  int attempts = 0;
  const int maxAttempts = 20; // 10 seconds timeout
  
  while (wifiMulti->run() != WL_CONNECTED && attempts < maxAttempts) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (wifiMulti->run() == WL_CONNECTED) {
    Serial.println("\nWiFi connected successfully");
    Serial.println("SSID: " + ssid);
    Serial.println("IP address: " + WiFi.localIP().toString());
    return true;
  } else {
    Serial.println("\nWiFi connection failed after " + String(attempts) + " attempts");
    return false;
  }
}

void CommunicationManager::disconnectWiFi() {
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  Serial.println("WiFi disconnected");
}

bool CommunicationManager::isWiFiConnected() {
  return WiFi.status() == WL_CONNECTED;
}

String CommunicationManager::getWiFiIP() {
  if (isWiFiConnected()) {
    return WiFi.localIP().toString();
  }
  return "Not connected";
}

void CommunicationManager::printConfig(const ConfigManager& config) {
  println("=== System Configuration ===");
  println("Bluetooth Name: " + config.getBluetoothName());
  println("WiFi SSID: " + config.getSSID());
  println("Server IP: " + config.getIPAddress());
  println("Server Port: " + config.getPort());
  println("Password: [PROTECTED]");
  println("Firmware: " + String(FIRMWARE_VERSION));
  println("===========================");
}

void CommunicationManager::printBatteryStatus(int batteryLevel) {
  println("BATTERY CHARGE: " + String(batteryLevel) + " %");
  println("VERSION: " + String(FIRMWARE_VERSION));
}

void CommunicationManager::printDataReceived(const String& meterType) {
  println("DATA RECEIVED: " + meterType + ".");
  printChar(254); // End-of-transmission marker
}

void CommunicationManager::printRawData(const String& data) {
  println("=== RAW DATA ===");
  println(data);
  println("===============");
}

void CommunicationManager::printSystemStatus() {
  println("=== System Status ===");
  println("Bluetooth: " + String(isBluetoothConnected() ? "Connected" : "Disconnected"));
  println("WiFi: " + String(isWiFiConnected() ? "Connected" : "Disconnected"));
  if (isWiFiConnected()) {
    println("WiFi IP: " + getWiFiIP());
  }
  println("Free Heap: " + String(ESP.getFreeHeap()) + " bytes");
  println("Uptime: " + String(millis() / 1000) + " seconds");
  println("====================");
}

void CommunicationManager::flush() {
  bluetoothSerial->flush();
  irdaSerial->flush();
  irSerial->flush();
  Serial.flush();
}

size_t CommunicationManager::available() {
  return bluetoothSerial->available();
}

void CommunicationManager::clearBuffers() {
  // Clear all serial buffers
  while (bluetoothSerial->available()) {
    bluetoothSerial->read();
  }
  while (irdaSerial->available()) {
    irdaSerial->read();
  }
  while (irSerial->available()) {
    irSerial->read();
  }
  
  flush();
}

#endif // COMMUNICATION_H