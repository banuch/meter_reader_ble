/*
 * main.cpp - Main application file for Modular Energy Meter Reader (ESP32 Core 3.x Compatible)
 * 
 * FIXED FOR: ESP32 Arduino Core 3.2.0+ compatibility
 * CHANGES: Replaced String.contains() with indexOf() for compatibility
 * 
 * IMPORTANT: When using in Arduino IDE, rename this file to "meter_reader.ino"
 */

#include "config.h"
#include "hardware_control.h"
#include "communication.h"
#include "meter_reader.h"
#include "data_parser.h"
#include "power_management.h"
#include "ota_manager.h"

// Global instances
ConfigManager config;
HardwareControl hardware;
CommunicationManager comm;
MeterReader meterReader;
DataParser parser;
PowerManager powerMgr;
OTAManager otaManager;

void setup() {
  Serial.begin(115200);
  Serial.println("Starting Energy Meter Reader V13.MODULAR...");
  
  // Initialize configuration first
  config.init();
  config.loadAll();
  
  // Initialize hardware
  hardware.init();
  
  // Initialize communication with loaded Bluetooth name
  comm.init(config.getBluetoothName());
  
  // Connect modules (dependency injection)
  meterReader.setCommunicationManager(&comm);
  meterReader.setHardwareControl(&hardware);
  parser.setCommunicationManager(&comm);
  powerMgr.setHardwareControl(&hardware);
  otaManager.setCommunicationManager(&comm);
  
  // Initialize remaining modules
  meterReader.init();
  powerMgr.init();
  
  // Print current configuration
  comm.printConfig(config);
  
  // Perform startup sequence
  hardware.startupSequence();
  meterReader.initializeIRDA();
  
  Serial.println("System initialized successfully");
  Serial.println("Ready to accept commands via Bluetooth");
}

void loop() {
  // Handle incoming Bluetooth commands
  String command = comm.readBluetoothCommand();
  if (command.length() > 0) {
    handleCommand(command);
  }
  
  // Update power management and check for sleep conditions
  powerMgr.update();
  if (powerMgr.shouldSleep()) {
    powerMgr.enterDeepSleep();
  }
  
  delay(1); // Small delay for system stability
}

void handleCommand(const String& command) {
  // Visual and audio feedback
  hardware.ledOn();
  hardware.beep();
  
  // Route commands to appropriate handlers
  if (command.startsWith("update_")) {
    handleConfigCommand(command);
  }
  else if (command.startsWith("#IRDA") || command.startsWith("#IRIR")) {
    handleMeterCommand(command);
  }
  else if (command == "#BATTV*") {
    handleBatteryCommand();
  }
  else if (command == "#VER*") {
    handleVersionCommand();
  }
  else if (command == "get_config") {
    comm.printConfig(config);
  }
  else if (command == "   ") {
    comm.println("PT-OK");
  }
  else {
    comm.println("Unknown command: " + command);
  }
  
  // End feedback and reset sleep timer
  hardware.ledOff();
  hardware.doubleBeep();
  powerMgr.resetSleepTimer();
}

void handleConfigCommand(const String& command) {
  if (command.startsWith("update_bname")) {
    config.updateBluetoothName(command.substring(14));
  }
  else if (command.startsWith("update_ssid")) {
    config.updateSSID(command.substring(13));
  }
  else if (command.startsWith("update_password")) {
    config.updatePassword(command.substring(17));
  }
  else if (command.startsWith("update_ipaddress")) {
    config.updateIPAddress(command.substring(18));
  }
  else if (command.startsWith("update_port")) {
    config.updatePort(command.substring(13));
  }
  else if (command.startsWith("update_firmware")) {
    otaManager.performUpdate(config);
  }
  else {
    comm.println("Unknown config command");
  }
}

void handleMeterCommand(const String& command) {
  MeterType meterType = parseMeterCommand(command);
  if (meterType == METER_TYPE_UNKNOWN) {
    comm.println("Unknown meter command");
    return;
  }
  
  MeterData data;
  bool success = meterReader.readMeter(meterType, data);
  
  if (success) {
    if (shouldParseData(command)) {
      parser.parseAndPrint(data, meterType);
    } else {
      comm.println(data.rawData);
    }
    
    comm.printBatteryStatus(powerMgr.getBatteryLevel());
    comm.printDataReceived(getMeterTypeString(meterType));
  } else {
    comm.println("Error: Failed to read meter data");
  }
}

void handleBatteryCommand() {
  int batteryLevel = powerMgr.getBatteryLevel();
  comm.printBatteryStatus(batteryLevel);
}

void handleVersionCommand() {
  comm.println(FIRMWARE_VERSION);
}

MeterType parseMeterCommand(const String& command) {
  if (command == "#IRDA1*") return IRDA_1PH_RAW;
  if (command == "#IRDA1P*") return IRDA_1PH_PARSED;
  if (command == "#IRDA3*") return IRDA_3PH_RAW;
  if (command == "#IRDA3P*") return IRDA_3PH_PARSED;
  if (command == "#IRDA3P14HP*") return IRDA_3PH_14HP;
  if (command == "#IRDA3P13HP*") return IRDA_3PH_13HP;
  if (command == "#IRDA3SR*") return IRDA_3PH_SOLAR_RAW;
  if (command == "#IRDA3SP*") return IRDA_3PH_SOLAR_PARSED;
  if (command == "#IRIR1*") return IR_1PH_RAW;
  if (command == "#IRIR1P*") return IR_1PH_PARSED;
  if (command == "#IRIR3*") return IR_3PH_RAW;
  if (command == "#IRIR3P*") return IR_3PH_PARSED;
  
  return METER_TYPE_UNKNOWN;
}

// FIXED: Replace String.contains() with indexOf() for ESP32 Core 3.x compatibility
bool shouldParseData(const String& command) {
  return (command.indexOf("P") != -1) && (command.indexOf("RAW") == -1);
}

String getMeterTypeString(MeterType type) {
  switch (type) {
    case IRDA_1PH_RAW: return "IRDA-1Ph-RAW";
    case IRDA_1PH_PARSED: return "IRDA-1Ph-PARSED";
    case IRDA_3PH_RAW: return "IRDA-3Ph-RAW";
    case IRDA_3PH_PARSED: return "IRDA-3Ph-PARSED";
    case IRDA_3PH_14HP: return "IRDA-3Ph-14HP-PARSED";
    case IRDA_3PH_13HP: return "IRDA-3Ph-13HP-PARSED";
    case IRDA_3PH_SOLAR_RAW: return "IRDA-3Ph-SOLAR-RAW";
    case IRDA_3PH_SOLAR_PARSED: return "IRDA-3Ph-SOLAR-PARSED";
    case IR_1PH_RAW: return "IR-1Ph-RAW";
    case IR_1PH_PARSED: return "IR-1Ph-PARSED";
    case IR_3PH_RAW: return "IR-3Ph-RAW";
    case IR_3PH_PARSED: return "IR-3Ph-PARSED";
    default: return "UNKNOWN";
  }
}