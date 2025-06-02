/*
 * config.cpp - Configuration management implementation
 * 
 * This file implements the ConfigManager class that handles
 * persistent storage and validation of system configuration.
 */

#include "config.h"

ConfigManager::ConfigManager() {
  // Initialize with default values
  config.bluetoothName = DEFAULT_BLE_NAME;
  config.ssid = DEFAULT_SSID;
  config.password = DEFAULT_PASSWORD;
  config.ipAddress = DEFAULT_IP;
  config.port = DEFAULT_PORT;
}

ConfigManager::~ConfigManager() {
  preferences.end();
}

void ConfigManager::init() {
  preferences.begin("credentials", false);
  Serial.println("ConfigManager initialized");
}

void ConfigManager::loadAll() {
  // Load configuration from preferences with fallback to defaults
  config.bluetoothName = preferences.getString("blename", DEFAULT_BLE_NAME);
  config.ssid = preferences.getString("ssid", DEFAULT_SSID);
  config.password = preferences.getString("password", DEFAULT_PASSWORD);
  config.ipAddress = preferences.getString("ipaddress", DEFAULT_IP);
  config.port = preferences.getString("port", DEFAULT_PORT);
  
  // Validate loaded data and use defaults if invalid
  if (!isValidBluetoothName(config.bluetoothName)) {
    config.bluetoothName = DEFAULT_BLE_NAME;
  }
  if (!isValidSSID(config.ssid)) {
    config.ssid = DEFAULT_SSID;
  }
  if (config.password.length() == 0) {
    config.password = DEFAULT_PASSWORD;
  }
  if (!isValidIP(config.ipAddress)) {
    config.ipAddress = DEFAULT_IP;
  }
  if (!isValidPort(config.port)) {
    config.port = DEFAULT_PORT;
  }
  
  Serial.println("Configuration loaded successfully");
}

void ConfigManager::saveAll() {
  preferences.putString("blename", config.bluetoothName);
  preferences.putString("ssid", config.ssid);
  preferences.putString("password", config.password);
  preferences.putString("ipaddress", config.ipAddress);
  preferences.putString("port", config.port);
  
  Serial.println("Configuration saved to flash memory");
}

void ConfigManager::updateBluetoothName(const String& name) {
  if (isValidBluetoothName(name)) {
    config.bluetoothName = name;
    preferences.putString("blename", name);
    Serial.println("Bluetooth name updated: " + name);
  } else {
    Serial.println("Invalid Bluetooth name: " + name);
  }
}

void ConfigManager::updateSSID(const String& ssid) {
  if (isValidSSID(ssid)) {
    config.ssid = ssid;
    preferences.putString("ssid", ssid);
    Serial.println("SSID updated: " + ssid);
  } else {
    Serial.println("Invalid SSID: " + ssid);
  }
}

void ConfigManager::updatePassword(const String& password) {
  if (password.length() > 0) {
    config.password = password;
    preferences.putString("password", password);
    Serial.println("Password updated successfully");
  } else {
    Serial.println("Invalid password (empty)");
  }
}

void ConfigManager::updateIPAddress(const String& ip) {
  if (isValidIP(ip)) {
    config.ipAddress = ip;
    preferences.putString("ipaddress", ip);
    Serial.println("IP Address updated: " + ip);
  } else {
    Serial.println("Invalid IP address: " + ip);
  }
}

void ConfigManager::updatePort(const String& port) {
  if (isValidPort(port)) {
    config.port = port;
    preferences.putString("port", port);
    Serial.println("Port updated: " + port);
  } else {
    Serial.println("Invalid port: " + port);
  }
}

void ConfigManager::printConfig() const {
  Serial.println("=== Current Configuration ===");
  Serial.println("Bluetooth Name: " + config.bluetoothName);
  Serial.println("SSID: " + config.ssid);
  Serial.println("IP Address: " + config.ipAddress);
  Serial.println("Port: " + config.port);
  Serial.println("Password: [HIDDEN]");
  Serial.println("=============================");
}

void ConfigManager::resetToDefaults() {
  config.bluetoothName = DEFAULT_BLE_NAME;
  config.ssid = DEFAULT_SSID;
  config.password = DEFAULT_PASSWORD;
  config.ipAddress = DEFAULT_IP;
  config.port = DEFAULT_PORT;
  
  saveAll();
  Serial.println("Configuration reset to factory defaults");
}

// Validation helper functions
bool ConfigManager::isValidBluetoothName(const String& name) const {
  return (name.length() > 0 && name.length() < MAX_BT_NAME_LENGTH);
}

bool ConfigManager::isValidSSID(const String& ssid) const {
  return (ssid.length() > 0 && ssid.length() <= 32);
}

bool ConfigManager::isValidIP(const String& ip) const {
  // Basic IP validation - check for proper format
  if (ip.length() == 0) return false;
  
  int dotCount = 0;
  for (int i = 0; i < ip.length(); i++) {
    char c = ip[i];
    if (c == '.') {
      dotCount++;
    } else if (!isDigit(c)) {
      return false;
    }
  }
  
  return (dotCount == 3);
}

bool ConfigManager::isValidPort(const String& port) const {
  if (port.length() == 0) return false;
  
  int portNum = port.toInt();
  return (portNum > 0 && portNum <= 65535);
}