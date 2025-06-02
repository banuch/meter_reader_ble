/*
 * config.h - Configuration management header
 * 
 * This file contains all system constants, pin definitions,
 * and the ConfigManager class for handling persistent settings.
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <Preferences.h>

// ========================= SYSTEM CONSTANTS =========================

#define FIRMWARE_VERSION "V13.MODULAR"
#define DEFAULT_BLE_NAME "PTA-DEFAULT"
#define DEFAULT_SSID "Default-WIFI"
#define DEFAULT_PASSWORD "password"
#define DEFAULT_IP "122.169.206.214"
#define DEFAULT_PORT "3000"

// ========================= HARDWARE PIN DEFINITIONS =========================

// IRDA and External Controls
#define PIN_IRDA_EN 12
#define PIN_EXT_SW 33
#define PIN_EXT_PW 32

// Serial Communication Pins
#define PIN_RXD2 16
#define PIN_TXD2 17
#define PIN_RXD1 18
#define PIN_TXD1 19

// LED and Buzzer Pins
#define PIN_LED 2
#define PIN_BUZZER 22
#define PIN_BUZZER1 19
#define PIN_LED_PWM 23

// Analog Pins
#define PIN_BATTERY 15

// ========================= COMMUNICATION SETTINGS =========================

#define SERIAL_TIMEOUT 100
#define IRDA_TIMEOUT 2000
#define BT_TIMEOUT 100
#define BAUD_RATE_2400 2400
#define BAUD_RATE_9600 9600
#define BAUD_RATE_115200 115200

// ========================= POWER MANAGEMENT SETTINGS =========================

#define SLEEP_TIMEOUT_MS 210000  // 3.5 minutes
#define BUTTON_DEBOUNCE_MS 2000

// ========================= PWM SETTINGS =========================

#define PWM_FREQ 38000
#define PWM_CHANNEL 0
#define PWM_RESOLUTION 8
#define PWM_DUTY_CYCLE 85

// ========================= BUFFER SIZES =========================

#define MAX_BT_NAME_LENGTH 20
#define PACKET_BUFFER_SIZE 100
#define COMMAND_BUFFER_SIZE 50

// ========================= ENUMERATIONS =========================

// Meter types enumeration
enum MeterType {
  METER_TYPE_UNKNOWN = 0,
  IRDA_1PH_RAW,
  IRDA_1PH_PARSED,
  IRDA_3PH_RAW,
  IRDA_3PH_PARSED,
  IRDA_3PH_14HP,
  IRDA_3PH_13HP,
  IRDA_3PH_SOLAR_RAW,
  IRDA_3PH_SOLAR_PARSED,
  IR_1PH_RAW,
  IR_1PH_PARSED,
  IR_3PH_RAW,
  IR_3PH_PARSED
};

// ========================= DATA STRUCTURES =========================

// Configuration data structure
struct SystemConfig {
  String bluetoothName;
  String ssid;
  String password;
  String ipAddress;
  String port;
};

// Meter data structure
struct MeterData {
  String rawData;
  uint8_t* binaryData;
  size_t dataLength;
  bool isValid;
  MeterType type;
  
  // Constructor
  MeterData() : binaryData(nullptr), dataLength(0), isValid(false), type(METER_TYPE_UNKNOWN) {}
  
  // Destructor
  ~MeterData() {
    if (binaryData) {
      free(binaryData);
      binaryData = nullptr;
    }
  }
};

// ========================= CONFIGURATION MANAGER CLASS =========================

class ConfigManager {
private:
  Preferences preferences;
  SystemConfig config;
  
public:
  ConfigManager();
  ~ConfigManager();
  
  // Initialization
  void init();
  
  // Configuration operations
  void loadAll();
  void saveAll();
  
  // Individual parameter updates
  void updateBluetoothName(const String& name);
  void updateSSID(const String& ssid);
  void updatePassword(const String& password);
  void updateIPAddress(const String& ip);
  void updatePort(const String& port);
  
  // Getters
  String getBluetoothName() const { return config.bluetoothName; }
  String getSSID() const { return config.ssid; }
  String getPassword() const { return config.password; }
  String getIPAddress() const { return config.ipAddress; }
  String getPort() const { return config.port; }
  int getPortInt() const { return config.port.toInt(); }
  
  // Utility
  void printConfig() const;
  void resetToDefaults();
  
  // Validation helpers
  bool isValidBluetoothName(const String& name) const;
  bool isValidSSID(const String& ssid) const;
  bool isValidIP(const String& ip) const;
  bool isValidPort(const String& port) const;
};

#endif // CONFIG_H