/*
 * meter_reader.h - Meter reading functionality
 * 
 * This file contains the MeterReader class that implements
 * various meter communication protocols (IRDA and IR).
 */

#ifndef METER_READER_H
#define METER_READER_H

#include <Arduino.h>
#include "config.h"
#include "communication.h"
#include "hardware_control.h"

// ========================= PROTOCOL MESSAGE DEFINITIONS =========================

class ProtocolMessages {
public:
  // IRDA 3-Phase protocol messages
  static const uint8_t IRDA_3PH_MSG1[11];
  static const uint8_t IRDA_3PH_MSG2[11];
  static const uint8_t IRDA_3PH_MSG5[11];
  static const uint8_t IRDA_3PH_MSG6[16];
  static const uint8_t IRDA_3PH_MSG7[16];
  
  // IR 3-Phase protocol message
  static const uint8_t IR_3PH_MSG[5];
  
  // IRDA 1-Phase command strings
  static const char* IRDA_1PH_CMD_STRINGS[5];
};

// ========================= METER READER CLASS =========================

class MeterReader {
private:
  CommunicationManager* comm;
  HardwareControl* hardware;
  
  // ========================= PROTOCOL HELPERS =========================
  bool readIRDAPacket(String& data, int expectedBytes, int timeoutMs = 2000);
  bool readIRPacket(String& data, int expectedBytes, int timeoutMs = 2000);
  void setupIRDABaudRate(int baudRate);
  void setupIRBaudRate(int baudRate);
  
  // ========================= PROTOCOL IMPLEMENTATIONS =========================
  bool readIRDA1Phase(MeterData& data, bool parseData);
  bool readIRDA3Phase(MeterData& data, bool parseData, bool isSolar = false);
  bool readIRDA3PhaseHP(MeterData& data, int digitCount);
  bool readIR1Phase(MeterData& data, bool parseData);
  bool readIR3Phase(MeterData& data, bool parseData);
  
  // ========================= UTILITY FUNCTIONS =========================
  bool sendIRDACommand(const uint8_t* command, size_t length);
  bool sendIRDACommand(const String& command);
  bool sendIRCommand(const uint8_t* command, size_t length);
  void clearBuffers();
  void logProtocolAction(const String& action);
  
public:
  MeterReader();
  
  // ========================= INITIALIZATION =========================
  void init();
  void initializeIRDA();
  void setCommunicationManager(CommunicationManager* commMgr) { comm = commMgr; }
  void setHardwareControl(HardwareControl* hwCtrl) { hardware = hwCtrl; }
  
  // ========================= MAIN READING INTERFACE =========================
  bool readMeter(MeterType type, MeterData& data);
  
  // ========================= PROTOCOL SPECIFIC READERS =========================
  bool readMeterIRDA1PH(bool parseData, MeterData& data);
  bool readMeterIRDA3PH(bool parseData, MeterData& data);
  bool readMeterIRDA3PHHP(int digitCount, MeterData& data);
  bool readMeterIRDA3PHSolar(bool parseData, MeterData& data);
  bool readMeterIR1PH(bool parseData, MeterData& data);
  bool readMeterIR3PH(bool parseData, MeterData& data);
  
  // ========================= DIAGNOSTIC FUNCTIONS =========================
  bool testIRDAConnection();
  bool testIRConnection();
  void printDiagnostics();
};

// ========================= PROTOCOL MESSAGE DEFINITIONS =========================

const uint8_t ProtocolMessages::IRDA_3PH_MSG1[11] = 
  { 0x95, 0x95, 0xFF, 0xFF, 0xFF, 0x0B, 0x96, 0x31, 0x11, 0x05, 0x00 };

const uint8_t ProtocolMessages::IRDA_3PH_MSG2[11] = 
  { 0x95, 0x95, 0xFF, 0xFF, 0xFF, 0x0B, 0x00, 0x31, 0x11, 0x05, 0x00 };

const uint8_t ProtocolMessages::IRDA_3PH_MSG5[11] = 
  { 0x95, 0x95, 0xFF, 0xFF, 0xFF, 0x0B, 0x01, 0x31, 0x11, 0x05, 0x00 };

const uint8_t ProtocolMessages::IRDA_3PH_MSG6[16] = 
  { 0x95, 0x95, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x10, 0x96, 0x31, 0x11, 0x05, 0x00 };

const uint8_t ProtocolMessages::IRDA_3PH_MSG7[16] = 
  { 0x95, 0x95, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x10, 0x00, 0x31, 0x11, 0x05, 0x00 };

const uint8_t ProtocolMessages::IR_3PH_MSG[5] = 
  { 0xB9, 0x9E, 0x8E, 0x7E, 0x1E };

const char* ProtocolMessages::IRDA_1PH_CMD_STRINGS[5] = {
  ":00413BC4",
  ":00423AC5", 
  ":004339C6",
  ":004537C8",
  ":004636C9"
};

// ========================= IMPLEMENTATION =========================

MeterReader::MeterReader() : comm(nullptr), hardware(nullptr) {
}

void MeterReader::init() {
  Serial.println("MeterReader initialized");
}

void MeterReader::initializeIRDA() {
  if (!comm || !hardware) {
    Serial.println("ERROR: MeterReader dependencies not set");
    return;
  }
  
  Serial.println("Initializing IRDA interface...");
  
  // Initialize with 2400 baud first
  setupIRDABaudRate(BAUD_RATE_2400);
  delay(1000);
  
  // Switch to 9600 baud
  setupIRDABaudRate(BAUD_RATE_9600);
  delay(100);
  
  hardware->enableIRDA();
  
  Serial.println("IRDA interface initialized");
}

bool MeterReader::readMeter(MeterType type, MeterData& data) {
  data.type = type;
  data.isValid = false;
  
  if (!comm || !hardware) {
    Serial.println("ERROR: MeterReader not properly initialized");
    return false;
  }
  
  logProtocolAction("Starting meter read for type: " + String((int)type));
  clearBuffers();
  
  bool success = false;
  
  switch (type) {
    case IRDA_1PH_RAW:
    case IRDA_1PH_PARSED:
      success = readMeterIRDA1PH(type == IRDA_1PH_PARSED, data);
      break;
      
    case IRDA_3PH_RAW:
    case IRDA_3PH_PARSED:
      success = readMeterIRDA3PH(type == IRDA_3PH_PARSED, data);
      break;
      
    case IRDA_3PH_14HP:
      success = readMeterIRDA3PHHP(8, data);
      break;
      
    case IRDA_3PH_13HP:
      success = readMeterIRDA3PHHP(7, data);
      break;
      
    case IRDA_3PH_SOLAR_RAW:
    case IRDA_3PH_SOLAR_PARSED:
      success = readMeterIRDA3PHSolar(type == IRDA_3PH_SOLAR_PARSED, data);
      break;
      
    case IR_1PH_RAW:
    case IR_1PH_PARSED:
      success = readMeterIR1PH(type == IR_1PH_PARSED, data);
      break;
      
    case IR_3PH_RAW:
    case IR_3PH_PARSED:
      success = readMeterIR3PH(type == IR_3PH_PARSED, data);
      break;
      
    default:
      Serial.println("ERROR: Unknown meter type");
      return false;
  }
  
  logProtocolAction("Meter read " + String(success ? "successful" : "failed"));
  return success;
}

bool MeterReader::readMeterIRDA1PH(bool parseData, MeterData& data) {
  logProtocolAction("Reading IRDA 1-Phase meter");
  
  setupIRDABaudRate(BAUD_RATE_2400);
  hardware->disableIRDA();
  
  String responseData = "";
  
  // Send all 5 commands and collect responses
  for (int i = 0; i < 5; i++) {
    logProtocolAction("Sending 1PH command " + String(i + 1));
    
    if (sendIRDACommand(ProtocolMessages::IRDA_1PH_CMD_STRINGS[i])) {
      delay(200);
      
      String packet;
      if (readIRDAPacket(packet, 30)) {
        responseData += packet;
        logProtocolAction("Received packet " + String(i + 1) + " (" + String(packet.length()) + " bytes)");
      } else {
        logProtocolAction("Failed to receive packet " + String(i + 1));
      }
    }
  }
  
  hardware->enableIRDA();
  
  if (responseData.length() > 0) {
    data.rawData = responseData;
    data.isValid = true;
    return true;
  }
  
  return false;
}

bool MeterReader::readMeterIRDA3PH(bool parseData, MeterData& data) {
  logProtocolAction("Reading IRDA 3-Phase meter");
  
  setupIRDABaudRate(BAUD_RATE_9600);
  hardware->disableIRDA();
  
  // First message - handshake
  if (sendIRDACommand(ProtocolMessages::IRDA_3PH_MSG1, sizeof(ProtocolMessages::IRDA_3PH_MSG1))) {
    String packet;
    if (readIRDAPacket(packet, 30)) {
      logProtocolAction("Handshake successful (" + String(packet.length()) + " bytes)");
      
      // Prepare second message with extracted data
      uint8_t msg2[11];
      memcpy(msg2, ProtocolMessages::IRDA_3PH_MSG2, sizeof(ProtocolMessages::IRDA_3PH_MSG2));
      
      if (packet.length() >= 25) {
        msg2[2] = packet[22];
        msg2[3] = packet[23]; 
        msg2[4] = packet[24];
      }
      
      delay(1500); // Protocol requires delay between messages
      
      if (sendIRDACommand(msg2, sizeof(msg2))) {
        String finalPacket;
        if (readIRDAPacket(finalPacket, 79)) {
          data.rawData = finalPacket;
          data.isValid = true;
          logProtocolAction("3PH data read successful (" + String(finalPacket.length()) + " bytes)");
        }
      }
    }
  }
  
  hardware->enableIRDA();
  return data.isValid;
}

bool MeterReader::readMeterIRDA3PHHP(int digitCount, MeterData& data) {
  logProtocolAction("Reading IRDA 3-Phase HP meter (" + String(digitCount) + " digits)");
  
  setupIRDABaudRate(BAUD_RATE_9600);
  hardware->disableIRDA();
  
  // HP meter protocol uses different message structure
  if (sendIRDACommand(ProtocolMessages::IRDA_3PH_MSG6, sizeof(ProtocolMessages::IRDA_3PH_MSG6))) {
    String packet;
    if (readIRDAPacket(packet, 45)) {
      logProtocolAction("HP handshake successful");
      
      // Prepare second message with extracted data
      uint8_t msg7[16];
      memcpy(msg7, ProtocolMessages::IRDA_3PH_MSG7, sizeof(ProtocolMessages::IRDA_3PH_MSG7));
      
      if (packet.length() >= 40) {
        for (int i = 0; i < 8; i++) {
          msg7[2 + i] = packet[32 + i];
        }
      }
      
      delay(1500);
      
      if (sendIRDACommand(msg7, sizeof(msg7))) {
        String finalPacket;
        if (readIRDAPacket(finalPacket, 71)) {
          data.rawData = finalPacket;
          data.isValid = true;
          logProtocolAction("HP data read successful");
        }
      }
    }
  }
  
  hardware->enableIRDA();
  return data.isValid;
}

bool MeterReader::readMeterIRDA3PHSolar(bool parseData, MeterData& data) {
  // First read normal data
  if (!readMeterIRDA3PH(parseData, data)) {
    return false;
  }
  
  logProtocolAction("Reading solar export data");
  
  // Now read export data with modified message
  uint8_t solarMsg[11];
  memcpy(solarMsg, ProtocolMessages::IRDA_3PH_MSG2, sizeof(ProtocolMessages::IRDA_3PH_MSG2));
  solarMsg[6] = 0x01; // Solar export flag
  
  String exportData;
  if (sendIRDACommand(solarMsg, sizeof(solarMsg))) {
    String packet;
    if (readIRDAPacket(packet, 79)) {
      data.rawData += "\n** EXPORT DATA **\n" + packet;
      logProtocolAction("Solar export data read successful");
    }
  }
  
  return data.isValid;
}

bool MeterReader::readMeterIR1PH(bool parseData, MeterData& data) {
  logProtocolAction("Reading IR 1-Phase meter");
  
  setupIRBaudRate(BAUD_RATE_2400);
  
  String responseData = "";
  
  // Send all 5 commands (same as IRDA but over IR)
  for (int i = 0; i < 5; i++) {
    if (sendIRDACommand(ProtocolMessages::IRDA_1PH_CMD_STRINGS[i])) {
      delay(200);
      
      String packet;
      if (readIRPacket(packet, 30)) {
        responseData += packet;
      }
    }
  }
  
  if (responseData.length() > 0) {
    data.rawData = responseData;
    data.isValid = true;
    return true;
  }
  
  return false;
}

bool MeterReader::readMeterIR3PH(bool parseData, MeterData& data) {
  logProtocolAction("Reading IR 3-Phase meter");
  
  setupIRBaudRate(BAUD_RATE_2400);
  
  if (sendIRCommand(ProtocolMessages::IR_3PH_MSG, sizeof(ProtocolMessages::IR_3PH_MSG))) {
    delay(500);
    String packet;
    if (readIRPacket(packet, 50)) {
      data.rawData = packet;
      data.isValid = true;
      return true;
    }
  }
  
  return false;
}

// ========================= HELPER FUNCTION IMPLEMENTATIONS =========================

bool MeterReader::readIRDAPacket(String& data, int expectedBytes, int timeoutMs) {
  data = "";
  unsigned long startTime = millis();
  
  HardwareSerial* serial = comm->getIRDASerial();
  
  while ((millis() - startTime < timeoutMs) && (data.length() < expectedBytes)) {
    if (serial->available()) {
      char c = serial->read();
      data += c;
    }
    delay(1);
  }
  
  return data.length() >= expectedBytes;
}

bool MeterReader::readIRPacket(String& data, int expectedBytes, int timeoutMs) {
  data = "";
  unsigned long startTime = millis();
  
  HardwareSerial* serial = comm->getIRSerial();
  
  while ((millis() - startTime < timeoutMs) && (data.length() < expectedBytes)) {
    if (serial->available()) {
      char c = serial->read();
      data += c;
    }
    delay(1);
  }
  
  return data.length() >= expectedBytes;
}

void MeterReader::setupIRDABaudRate(int baudRate) {
  comm->setupIRDASerial(baudRate);
  delay(50);
}

void MeterReader::setupIRBaudRate(int baudRate) {
  comm->setupIRSerial(baudRate);
  delay(50);
}

bool MeterReader::sendIRDACommand(const uint8_t* command, size_t length) {
  HardwareSerial* serial = comm->getIRDASerial();
  size_t written = serial->write(command, length);
  serial->flush();
  return (written == length);
}

bool MeterReader::sendIRDACommand(const String& command) {
  HardwareSerial* serial = comm->getIRDASerial();
  serial->println(command);
  serial->flush();
  return true;
}

bool MeterReader::sendIRCommand(const uint8_t* command, size_t length) {
  HardwareSerial* serial = comm->getIRSerial();
  
  // Send byte by byte with small delays for IR protocol
  for (size_t i = 0; i < length; i++) {
    serial->write(command[i]);
    delay(2);
  }
  serial->flush();
  return true;
}

void MeterReader::clearBuffers() {
  if (comm) {
    comm->clearBuffers();
  }
}

void MeterReader::logProtocolAction(const String& action) {
  Serial.println("[MeterReader] " + action);
}

bool MeterReader::testIRDAConnection() {
  // Simple IRDA connectivity test
  logProtocolAction("Testing IRDA connection");
  
  setupIRDABaudRate(BAUD_RATE_2400);
  hardware->disableIRDA();
  
  bool success = sendIRDACommand(ProtocolMessages::IRDA_1PH_CMD_STRINGS[0]);
  
  hardware->enableIRDA();
  
  return success;
}

bool MeterReader::testIRConnection() {
  // Simple IR connectivity test
  logProtocolAction("Testing IR connection");
  
  setupIRBaudRate(BAUD_RATE_2400);
  
  return sendIRCommand(ProtocolMessages::IR_3PH_MSG, sizeof(ProtocolMessages::IR_3PH_MSG));
}

void MeterReader::printDiagnostics() {
  if (comm) {
    comm->println("=== MeterReader Diagnostics ===");
    comm->println("IRDA Test: " + String(testIRDAConnection() ? "PASS" : "FAIL"));
    comm->println("IR Test: " + String(testIRConnection() ? "PASS" : "FAIL"));
    comm->println("==============================");
  }
}

#endif // METER_READER_H