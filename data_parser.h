/*
 * data_parser.h - Data parsing functionality
 * 
 * This file contains the DataParser class that handles
 * parsing of raw meter data into structured, human-readable format.
 */

#ifndef DATA_PARSER_H
#define DATA_PARSER_H

#include <Arduino.h>
#include "config.h"
#include "communication.h"

// ========================= PARSED DATA STRUCTURES =========================

struct MeterInfo {
  String serialNumber;
  String manufacturerId;
  String timestamp;
  String date;
  String make;
  int phase;
  float multiplicationFactor;
  int mdResetCount;
  
  // Constructor
  MeterInfo() : phase(0), multiplicationFactor(0.0), mdResetCount(0) {}
};

struct EnergyData {
  float kwh;
  float kvah;
  float kvarh;
  float kvarhLag;
  float kvarhLead;
  float kva;
  float powerFactor;
  float maxDemand;
  String mdTime;
  String mdDate;
  
  // Constructor
  EnergyData() : kwh(0.0), kvah(0.0), kvarh(0.0), kvarhLag(0.0), 
                 kvarhLead(0.0), kva(0.0), powerFactor(0.0), maxDemand(0.0) {}
};

struct ElectricalData {
  float voltageR, voltageY, voltageB;
  float currentR, currentY, currentB;
  float frequency;
  int tamperCount;
  int tamperStatus;
  
  // Constructor
  ElectricalData() : voltageR(0.0), voltageY(0.0), voltageB(0.0),
                     currentR(0.0), currentY(0.0), currentB(0.0),
                     frequency(0.0), tamperCount(0), tamperStatus(0) {}
};

struct ParsedMeterData {
  MeterInfo info;
  EnergyData energy;
  ElectricalData electrical;
  bool isValid;
  
  // Constructor
  ParsedMeterData() : isValid(false) {}
};

// ========================= DATA PARSER CLASS =========================

class DataParser {
private:
  CommunicationManager* comm;
  
  // ========================= UTILITY FUNCTIONS =========================
  uint32_t hexToDecimal(const uint8_t* data, int length);
  uint32_t hexToDecimal(const String& hexStr);
  void bcdToDecimal(uint8_t bcd, uint8_t& tens, uint8_t& units);
  String formatBCDTime(uint8_t hour, uint8_t minute, uint8_t second = 0);
  String formatBCDDate(uint8_t day, uint8_t month, uint8_t year);
  float convertToFloat(uint32_t value, int decimals);
  
  // ========================= PARSER IMPLEMENTATIONS =========================
  bool parse1PhaseData(const String& rawData, ParsedMeterData& parsed);
  bool parse3PhaseData(const String& rawData, ParsedMeterData& parsed);
  bool parse3PhaseHPData(const String& rawData, ParsedMeterData& parsed, int digitCount);
  bool parseIR3PhaseData(const String& rawData, ParsedMeterData& parsed);
  
  // ========================= OUTPUT FORMATTERS =========================
  void printMeterInfo(const MeterInfo& info);
  void printEnergyData(const EnergyData& energy);
  void printElectricalData(const ElectricalData& electrical);
  void printSeparator(const String& title);
  
  // ========================= VALIDATION HELPERS =========================
  bool validatePacketLength(const String& data, int minLength);
  bool validateBCDValue(uint8_t bcd);
  
public:
  DataParser();
  
  void setCommunicationManager(CommunicationManager* commMgr) { comm = commMgr; }
  
  // ========================= MAIN PARSING INTERFACE =========================
  bool parseAndPrint(const MeterData& data, MeterType type);
  
  // ========================= INDIVIDUAL PARSERS =========================
  bool parse1PhaseIRDA(const String& rawData, ParsedMeterData& parsed);
  bool parse3PhaseIRDA(const String& rawData, ParsedMeterData& parsed);
  bool parse3PhaseHPIRDA(const String& rawData, ParsedMeterData& parsed, int digitCount);
  bool parse1PhaseIR(const String& rawData, ParsedMeterData& parsed);
  bool parse3PhaseIR(const String& rawData, ParsedMeterData& parsed);
  
  // ========================= UTILITY FUNCTIONS =========================
  void printRawDataHex(const String& data);
  void printDataStatistics(const ParsedMeterData& parsed);
};

// ========================= IMPLEMENTATION =========================

DataParser::DataParser() : comm(nullptr) {
}

bool DataParser::parseAndPrint(const MeterData& data, MeterType type) {
  if (!comm || !data.isValid || data.rawData.length() == 0) {
    if (comm) comm->println("ERROR: Invalid data or parser not initialized");
    return false;
  }
  
  ParsedMeterData parsed;
  bool success = false;
  
  printSeparator("PARSING DATA");
  
  switch (type) {
    case IRDA_1PH_PARSED:
      success = parse1PhaseIRDA(data.rawData, parsed);
      break;
    case IRDA_3PH_PARSED:
      success = parse3PhaseIRDA(data.rawData, parsed);
      break;
    case IRDA_3PH_14HP:
      success = parse3PhaseHPIRDA(data.rawData, parsed, 8);
      break;
    case IRDA_3PH_13HP:
      success = parse3PhaseHPIRDA(data.rawData, parsed, 7);
      break;
    case IR_1PH_PARSED:
      success = parse1PhaseIR(data.rawData, parsed);
      break;
    case IR_3PH_PARSED:
      success = parse3PhaseIR(data.rawData, parsed);
      break;
    default:
      comm->println("ERROR: Unsupported parsing type");
      return false;
  }
  
  if (success && parsed.isValid) {
    printMeterInfo(parsed.info);
    printEnergyData(parsed.energy);
    printElectricalData(parsed.electrical);
    printDataStatistics(parsed);
    return true;
  } else {
    comm->println("ERROR: Failed to parse meter data");
    return false;
  }
}

bool DataParser::parse1PhaseIRDA(const String& rawData, ParsedMeterData& parsed) {
  if (!validatePacketLength(rawData, 120)) {
    return false;
  }
  
  parsed.isValid = false;
  
  // Extract serial number from first packet (positions 16-24)
  if (rawData.length() > 24) {
    parsed.info.serialNumber = rawData.substring(16, 24);
    parsed.info.serialNumber.replace(String(char(6)), ""); // Remove control characters
    parsed.info.serialNumber.trim();
  }
  
  // Extract manufacturer ID from second packet
  int secondPacketStart = rawData.indexOf(":", 30);
  if (secondPacketStart > 0 && rawData.length() > secondPacketStart + 32) {
    parsed.info.manufacturerId = rawData.substring(secondPacketStart + 16, secondPacketStart + 32);
    parsed.info.manufacturerId.replace(String(char(6)), "");
    parsed.info.manufacturerId.trim();
  }
  
  // Extract KWH reading from third packet
  int thirdPacketStart = rawData.indexOf(":", secondPacketStart + 30);
  if (thirdPacketStart > 0 && rawData.length() > thirdPacketStart + 25) {
    String kwhStr = rawData.substring(thirdPacketStart + 16, thirdPacketStart + 25);
    kwhStr.replace(String(char(6)), "");
    kwhStr.trim();
    parsed.energy.kwh = kwhStr.toFloat();
  }
  
  // Extract RMD (Remaining Days) from fourth packet
  int fourthPacketStart = rawData.indexOf(":", thirdPacketStart + 30);
  if (fourthPacketStart > 0 && rawData.length() > fourthPacketStart + 21) {
    String rmdStr = rawData.substring(fourthPacketStart + 16, fourthPacketStart + 21);
    rmdStr.trim();
    // RMD processing - could be added as additional field if needed
  }
  
  parsed.info.phase = 1; // Single phase
  parsed.isValid = true;
  return true;
}

bool DataParser::parse3PhaseIRDA(const String& rawData, ParsedMeterData& parsed) {
  if (!validatePacketLength(rawData, 79)) {
    return false;
  }
  
  parsed.isValid = false;
  
  const uint8_t* data = reinterpret_cast<const uint8_t*>(rawData.c_str());
  
  // Extract manufacturer ID (bytes 18-20)
  if (rawData.length() > 21) {
    uint32_t mfId = hexToDecimal(&data[18], 3);
    parsed.info.manufacturerId = String(mfId);
  }
  
  // Extract time (bytes 21-23) - BCD format
  if (rawData.length() > 26) {
    parsed.info.timestamp = formatBCDTime(data[21], data[22], data[23]);
  }
  
  // Extract date (bytes 24-26) - BCD format
  if (rawData.length() > 29) {
    parsed.info.date = formatBCDDate(data[24], data[25], data[26]);
  }
  
  // Extract voltages (bytes 27-32)
  if (rawData.length() > 33) {
    parsed.electrical.voltageR = convertToFloat(hexToDecimal(&data[27], 2), 1);
    parsed.electrical.voltageY = convertToFloat(hexToDecimal(&data[29], 2), 1);
    parsed.electrical.voltageB = convertToFloat(hexToDecimal(&data[31], 2), 1);
  }
  
  // Extract currents (bytes 33-38)
  if (rawData.length() > 39) {
    parsed.electrical.currentR = convertToFloat(hexToDecimal(&data[33], 2), 2);
    parsed.electrical.currentY = convertToFloat(hexToDecimal(&data[35], 2), 2);
    parsed.electrical.currentB = convertToFloat(hexToDecimal(&data[37], 2), 2);
  }
  
  // Extract energy readings
  if (rawData.length() > 47) {
    parsed.energy.kwh = convertToFloat(hexToDecimal(&data[43], 4), 2);
  }
  if (rawData.length() > 59) {
    parsed.energy.kvah = convertToFloat(hexToDecimal(&data[55], 4), 2);
  }
  if (rawData.length() > 61) {
    parsed.energy.maxDemand = convertToFloat(hexToDecimal(&data[59], 2), 2);
  }
  
  // Extract make (bytes 66-68)
  if (rawData.length() > 69) {
    parsed.info.make = String(char(data[66])) + String(char(data[67])) + String(char(data[68]));
  }
  
  // Extract phase count
  if (rawData.length() > 70) {
    parsed.info.phase = data[69];
  }
  
  // Extract multiplication factor
  if (rawData.length() > 72) {
    parsed.info.multiplicationFactor = convertToFloat(hexToDecimal(&data[70], 2), 2);
  }
  
  parsed.isValid = true;
  return true;
}

bool DataParser::parse3PhaseHPIRDA(const String& rawData, ParsedMeterData& parsed, int digitCount) {
  if (!validatePacketLength(rawData, 71)) {
    return false;
  }
  
  parsed.isValid = false;
  
  const uint8_t* data = reinterpret_cast<const uint8_t*>(rawData.c_str());
  
  // Extract manufacturer ID with digit count consideration
  if (rawData.length() > 27) {
    uint32_t mfId = hexToDecimal(&data[23], 4);
    
    // Format with leading zeros based on digit count
    String mfIdStr = String(mfId);
    while (mfIdStr.length() < digitCount) {
      mfIdStr = "0" + mfIdStr;
    }
    parsed.info.manufacturerId = mfIdStr;
  }
  
  // Similar parsing to regular 3-phase but with adjusted byte positions for HP format
  // Extract time (bytes 31-33)
  if (rawData.length() > 34) {
    parsed.info.timestamp = formatBCDTime(data[31], data[32], data[33]);
  }
  
  // Extract date (bytes 34-36)
  if (rawData.length() > 37) {
    parsed.info.date = formatBCDDate(data[34], data[35], data[36]);
  }
  
  // Extract voltages with HP-specific offsets
  if (rawData.length() > 43) {
    parsed.electrical.voltageR = convertToFloat(hexToDecimal(&data[38], 2), 1);
    parsed.electrical.voltageY = convertToFloat(hexToDecimal(&data[40], 2), 1);
    parsed.electrical.voltageB = convertToFloat(hexToDecimal(&data[42], 2), 1);
  }
  
  // Extract currents
  if (rawData.length() > 49) {
    parsed.electrical.currentR = convertToFloat(hexToDecimal(&data[44], 2), 2);
    parsed.electrical.currentY = convertToFloat(hexToDecimal(&data[46], 2), 2);
    parsed.electrical.currentB = convertToFloat(hexToDecimal(&data[48], 2), 2);
  }
  
  // Extract energy readings
  if (rawData.length() > 57) {
    parsed.energy.kwh = convertToFloat(hexToDecimal(&data[49], 4), 2);
  }
  if (rawData.length() > 61) {
    parsed.energy.kvah = convertToFloat(hexToDecimal(&data[53], 4), 2);
  }
  
  parsed.info.phase = 3; // Three phase
  parsed.isValid = true;
  return true;
}

bool DataParser::parse1PhaseIR(const String& rawData, ParsedMeterData& parsed) {
  // IR 1-phase uses similar format to IRDA 1-phase
  return parse1PhaseIRDA(rawData, parsed);
}

bool DataParser::parse3PhaseIR(const String& rawData, ParsedMeterData& parsed) {
  if (!validatePacketLength(rawData, 43)) {
    return false;
  }
  
  parsed.isValid = false;
  
  const uint8_t* data = reinterpret_cast<const uint8_t*>(rawData.c_str());
  
  // IR 3-phase has different format than IRDA
  if (rawData.length() > 10) {
    uint32_t mfId = hexToDecimal(&data[6], 4);
    parsed.info.manufacturerId = String(mfId);
  }
  
  // Extract date and time (different positions than IRDA)
  if (rawData.length() > 15) {
    parsed.info.date = formatBCDDate(data[10], data[11], data[12]);
    parsed.info.timestamp = formatBCDTime(data[13], data[14]);
  }
  
  // Extract energy readings
  if (rawData.length() > 19) {
    parsed.energy.kwh = convertToFloat(hexToDecimal(&data[15], 4), 3);
  }
  if (rawData.length() > 23) {
    parsed.energy.kvarhLag = convertToFloat(hexToDecimal(&data[19], 4), 3);
  }
  if (rawData.length() > 27) {
    parsed.energy.kvarhLead = convertToFloat(hexToDecimal(&data[23], 4), 3);
  }
  if (rawData.length() > 31) {
    parsed.energy.kvah = convertToFloat(hexToDecimal(&data[27], 4), 3);
  }
  
  // Power factor
  if (rawData.length() > 32) {
    parsed.energy.powerFactor = convertToFloat(data[31], 2);
  }
  
  // Max demand
  if (rawData.length() > 35) {
    parsed.energy.maxDemand = convertToFloat(hexToDecimal(&data[32], 2), 3);
  }
  
  // Tamper information
  if (rawData.length() > 43) {
    parsed.electrical.tamperCount = hexToDecimal(&data[39], 2);
    parsed.electrical.tamperStatus = hexToDecimal(&data[41], 2);
  }
  
  parsed.info.phase = 3; // Three phase
  parsed.isValid = true;
  return true;
}

// ========================= UTILITY FUNCTION IMPLEMENTATIONS =========================

uint32_t DataParser::hexToDecimal(const uint8_t* data, int length) {
  uint32_t result = 0;
  for (int i = 0; i < length; i++) {
    result = (result << 8) | data[i];
  }
  return result;
}

uint32_t DataParser::hexToDecimal(const String& hexStr) {
  return strtoul(hexStr.c_str(), nullptr, 16);
}

void DataParser::bcdToDecimal(uint8_t bcd, uint8_t& tens, uint8_t& units) {
  tens = (bcd >> 4) & 0x0F;
  units = bcd & 0x0F;
}

String DataParser::formatBCDTime(uint8_t hour, uint8_t minute, uint8_t second) {
  uint8_t h_tens, h_units, m_tens, m_units;
  bcdToDecimal(hour, h_tens, h_units);
  bcdToDecimal(minute, m_tens, m_units);
  
  String result = String(h_tens) + String(h_units) + ":" + String(m_tens) + String(m_units);
  
  if (second != 0) {
    uint8_t s_tens, s_units;
    bcdToDecimal(second, s_tens, s_units);
    result += ":" + String(s_tens) + String(s_units);
  }
  
  return result;
}

String DataParser::formatBCDDate(uint8_t day, uint8_t month, uint8_t year) {
  uint8_t d_tens, d_units, m_tens, m_units, y_tens, y_units;
  bcdToDecimal(day, d_tens, d_units);
  bcdToDecimal(month, m_tens, m_units);
  bcdToDecimal(year, y_tens, y_units);
  
  return String(d_tens) + String(d_units) + ":" + 
         String(m_tens) + String(m_units) + ":" + 
         String(y_tens) + String(y_units);
}

float DataParser::convertToFloat(uint32_t value, int decimals) {
  float divisor = 1.0;
  for (int i = 0; i < decimals; i++) {
    divisor *= 10.0;
  }
  return value / divisor;
}

bool DataParser::validatePacketLength(const String& data, int minLength) {
  if (data.length() < minLength) {
    if (comm) {
      comm->println("ERROR: Packet too short (" + String(data.length()) + " < " + String(minLength) + ")");
    }
    return false;
  }
  return true;
}

bool DataParser::validateBCDValue(uint8_t bcd) {
  uint8_t high = (bcd >> 4) & 0x0F;
  uint8_t low = bcd & 0x0F;
  return (high <= 9 && low <= 9);
}

// ========================= OUTPUT FORMATTERS =========================

void DataParser::printMeterInfo(const MeterInfo& info) {
  if (!comm) return;
  
  printSeparator("METER INFORMATION");
  
  if (info.serialNumber.length() > 0) {
    comm->println("Serial Number: " + info.serialNumber);
  }
  if (info.manufacturerId.length() > 0) {
    comm->println("Manufacturer ID: " + info.manufacturerId);
  }
  if (info.timestamp.length() > 0) {
    comm->println("Time: " + info.timestamp);
  }
  if (info.date.length() > 0) {
    comm->println("Date: " + info.date);
  }
  if (info.make.length() > 0) {
    comm->println("Make: " + info.make);
  }
  if (info.phase > 0) {
    comm->println("Phase: " + String(info.phase));
  }
  if (info.multiplicationFactor > 0) {
    comm->println("Multiplication Factor: " + String(info.multiplicationFactor, 2));
  }
  if (info.mdResetCount > 0) {
    comm->println("MD Reset Count: " + String(info.mdResetCount));
  }
}

void DataParser::printEnergyData(const EnergyData& energy) {
  if (!comm) return;
  
  printSeparator("ENERGY DATA");
  
  if (energy.kwh > 0) {
    comm->println("KWh: " + String(energy.kwh, 2));
  }
  if (energy.kvah > 0) {
    comm->println("KVAh: " + String(energy.kvah, 2));
  }
  if (energy.kvarh > 0) {
    comm->println("KVArh: " + String(energy.kvarh, 3));
  }
  if (energy.kvarhLag > 0) {
    comm->println("KVArh Lag: " + String(energy.kvarhLag, 3));
  }
  if (energy.kvarhLead > 0) {
    comm->println("KVArh Lead: " + String(energy.kvarhLead, 3));
  }
  if (energy.maxDemand > 0) {
    comm->println("Max Demand: " + String(energy.maxDemand, 2));
  }
  if (energy.powerFactor > 0) {
    comm->println("Power Factor: " + String(energy.powerFactor, 2));
  }
  if (energy.mdTime.length() > 0) {
    comm->println("MD Time: " + energy.mdTime);
  }
  if (energy.mdDate.length() > 0) {
    comm->println("MD Date: " + energy.mdDate);
  }
}

void DataParser::printElectricalData(const ElectricalData& electrical) {
  if (!comm) return;
  
  printSeparator("ELECTRICAL DATA");
  
  if (electrical.voltageR > 0) {
    comm->println("Voltage R: " + String(electrical.voltageR, 1) + "V");
    comm->println("Voltage Y: " + String(electrical.voltageY, 1) + "V");
    comm->println("Voltage B: " + String(electrical.voltageB, 1) + "V");
  }
  if (electrical.currentR > 0) {
    comm->println("Current R: " + String(electrical.currentR, 2) + "A");
    comm->println("Current Y: " + String(electrical.currentY, 2) + "A");
    comm->println("Current B: " + String(electrical.currentB, 2) + "A");
  }
  if (electrical.frequency > 0) {
    comm->println("Frequency: " + String(electrical.frequency, 1) + "Hz");
  }
  if (electrical.tamperCount > 0) {
    comm->println("Tamper Count: " + String(electrical.tamperCount));
  }
  if (electrical.tamperStatus > 0) {
    comm->println("Tamper Status: " + String(electrical.tamperStatus));
  }
}

void DataParser::printSeparator(const String& title) {
  if (!comm) return;
  
  comm->println("=== " + title + " ===");
}

void DataParser::printRawDataHex(const String& data) {
  if (!comm) return;
  
  comm->println("=== RAW DATA (HEX) ===");
  for (int i = 0; i < data.length(); i++) {
    if (i % 16 == 0) {
      comm->print(String(i, HEX) + ": ");
    }
    comm->print(String((uint8_t)data[i], HEX) + " ");
    if ((i + 1) % 16 == 0) {
      comm->println("");
    }
  }
  comm->println("\n====================");
}

void DataParser::printDataStatistics(const ParsedMeterData& parsed) {
  if (!comm) return;
  
  printSeparator("DATA STATISTICS");
  comm->println("Parsing Status: " + String(parsed.isValid ? "SUCCESS" : "FAILED"));
  comm->println("Total Power: " + String(parsed.energy.kwh + parsed.energy.kvah, 2) + " units");
  if (parsed.info.phase > 0) {
    comm->println("System Type: " + String(parsed.info.phase) + "-Phase");
  }
}

#endif // DATA_PARSER_H