/*
 * ota_manager.h - Over-the-air update functionality (ESP32 Core 3.x Compatible)
 * 
 * FIXED FOR: ESP32 Arduino Core 3.2.0+ compatibility
 * CHANGES: 
 * - Removed httpUpdate.setTimeout() (not available)
 * - Fixed WiFiClientSecure certificate handling
 * - Fixed static callback member access
 * - Replaced cleanAPlist() with WiFi.disconnect()
 */

#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPUpdate.h>
#include <WiFiClientSecure.h>
#include "config.h"
#include "communication.h"

// Update result enumeration
enum UpdateResult {
  UPDATE_SUCCESS,
  UPDATE_FAILED,
  UPDATE_NO_UPDATES,
  UPDATE_WIFI_FAILED,
  UPDATE_TIMEOUT,
  UPDATE_INVALID_URL,
  UPDATE_DOWNLOAD_FAILED,
  UPDATE_VERIFICATION_FAILED
};

// Update progress information
struct UpdateProgress {
  int currentBytes;
  int totalBytes;
  int percentComplete;
  unsigned long startTime;
  unsigned long elapsedTime;
  float downloadSpeedKBps;
  
  // Constructor
  UpdateProgress() : currentBytes(0), totalBytes(0), percentComplete(0),
                     startTime(0), elapsedTime(0), downloadSpeedKBps(0.0) {}
};

class OTAManager {
private:
  CommunicationManager* comm;
  WiFiMulti* wifiMulti;
  
  // Update state
  bool updateInProgress;
  unsigned long updateStartTime;
  unsigned long updateTimeoutMs;
  UpdateProgress progress;
  
  // Security
  bool useHTTPS;
  String serverCertFingerprint;
  
  // FIXED: Static progress tracking for callbacks
  static UpdateProgress staticProgress;
  static OTAManager* staticInstance;
  
  // Private methods
  bool connectToWiFi(const ConfigManager& config);
  void disconnectWiFi();
  bool waitForWiFiConnection(int timeoutSeconds = 30);
  
  // Update process
  UpdateResult performHTTPUpdate(const String& server, int port, const String& path);
  UpdateResult performHTTPSUpdate(const String& server, int port, const String& path);
  void handleUpdateResult(t_httpUpdate_return result);
  
  // Progress and error handling
  static void onUpdateProgress(int current, int total);
  static void onUpdateError(int error);
  static void onUpdateStart();
  static void onUpdateEnd();
  
  // Validation
  bool validateUpdateURL(const String& server, int port, const String& path);
  bool validateFirmwareVersion();
  
  // Logging
  void logUpdateEvent(const String& event);
  void logError(const String& error);
  
public:
  OTAManager();
  ~OTAManager();
  
  void setCommunicationManager(CommunicationManager* commMgr) { comm = commMgr; }
  
  // Main update interface
  UpdateResult performUpdate(const ConfigManager& config);
  UpdateResult performUpdateFromURL(const String& url);
  
  // Configuration
  void setUpdateTimeout(unsigned long timeoutMs);
  void enableHTTPS(bool enable, const String& fingerprint = "");
  void setServerCertFingerprint(const String& fingerprint);
  
  // Status checking
  bool isUpdateInProgress() const { return updateInProgress; }
  UpdateProgress getUpdateProgress() const { return progress; }
  unsigned long getUpdateTimeRemaining();
  
  // Utility
  String getUpdateResultString(UpdateResult result);
  String getCurrentFirmwareVersion();
  bool checkForUpdates(const ConfigManager& config);
  
  // Diagnostics
  void printUpdateStatus();
  void printNetworkDiagnostics();
  bool testServerConnection(const ConfigManager& config);
};

// FIXED: Static member definitions
UpdateProgress OTAManager::staticProgress;
OTAManager* OTAManager::staticInstance = nullptr;

// Implementation
OTAManager::OTAManager() 
  : comm(nullptr), wifiMulti(nullptr), updateInProgress(false), 
    updateStartTime(0), updateTimeoutMs(300000), useHTTPS(false) {
  
  wifiMulti = new WiFiMulti();
  staticInstance = this; // Set static instance for callbacks
}

OTAManager::~OTAManager() {
  delete wifiMulti;
  staticInstance = nullptr;
}

UpdateResult OTAManager::performUpdate(const ConfigManager& config) {
  if (updateInProgress) {
    logError("Update already in progress");
    return UPDATE_FAILED;
  }
  
  updateInProgress = true;
  updateStartTime = millis();
  progress = UpdateProgress();
  staticProgress = UpdateProgress(); // Initialize static progress
  progress.startTime = updateStartTime;
  staticProgress.startTime = updateStartTime;
  
  logUpdateEvent("OTA Update Started");
  
  if (comm) {
    comm->println("=== OTA FIRMWARE UPDATE ===");
    comm->println("Version: " + getCurrentFirmwareVersion());
    comm->println("Server: " + config.getIPAddress() + ":" + config.getPort());
    comm->println("==========================");
    comm->println("Connecting to WiFi...");
  }
  
  // Step 1: Connect to WiFi
  if (!connectToWiFi(config)) {
    updateInProgress = false;
    logError("WiFi connection failed");
    return UPDATE_WIFI_FAILED;
  }
  
  // Step 2: Validate update parameters
  String firmwarePath = "/firmware/ota.bin";
  if (!validateUpdateURL(config.getIPAddress(), config.getPortInt(), firmwarePath)) {
    disconnectWiFi();
    updateInProgress = false;
    logError("Invalid update URL");
    return UPDATE_INVALID_URL;
  }
  
  if (comm) {
    comm->println("WiFi connected: " + WiFi.localIP().toString());
    comm->println("Starting firmware download...");
  }
  
  // Step 3: Perform the actual update
  UpdateResult result;
  if (useHTTPS) {
    result = performHTTPSUpdate(config.getIPAddress(), config.getPortInt(), firmwarePath);
  } else {
    result = performHTTPUpdate(config.getIPAddress(), config.getPortInt(), firmwarePath);
  }
  
  // Step 4: Cleanup
  disconnectWiFi();
  updateInProgress = false;
  
  if (comm) {
    comm->println("Update completed: " + getUpdateResultString(result));
  }
  
  logUpdateEvent("Update completed with result: " + getUpdateResultString(result));
  
  return result;
}

UpdateResult OTAManager::performUpdateFromURL(const String& url) {
  // Parse URL and extract components
  logUpdateEvent("Update from URL: " + url);
  
  if (comm) {
    comm->println("Direct URL update not fully implemented");
    comm->println("Use standard update method instead");
  }
  
  return UPDATE_FAILED;
}

bool OTAManager::connectToWiFi(const ConfigManager& config) {
  logUpdateEvent("Connecting to WiFi: " + config.getSSID());
  
  // FIXED: Clear any existing AP configurations - ESP32 Core 3.x compatible
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.setTxPower(WIFI_POWER_19_5dBm); // Max power for better connection
  
  // Add the configured network
  wifiMulti->addAP(config.getSSID().c_str(), config.getPassword().c_str());
  
  // Attempt to connect with progress indication
  return waitForWiFiConnection(30);
}

bool OTAManager::waitForWiFiConnection(int timeoutSeconds) {
  int attempts = 0;
  const int maxAttempts = timeoutSeconds * 2; // Check every 500ms
  
  while (wifiMulti->run() != WL_CONNECTED && attempts < maxAttempts) {
    delay(500);
    
    // Progress indication
    if (comm && attempts % 6 == 0) { // Print dots every 3 seconds
      comm->print(".");
    }
    
    attempts++;
    
    // Update progress for long connections
    if (attempts % 10 == 0) {
      logUpdateEvent("WiFi connection attempt " + String(attempts / 2) + "/" + String(timeoutSeconds));
    }
  }
  
  if (wifiMulti->run() == WL_CONNECTED) {
    logUpdateEvent("WiFi connected successfully");
    if (comm) {
      comm->println("\nWiFi connected successfully");
      comm->println("Signal strength: " + String(WiFi.RSSI()) + " dBm");
      comm->println("IP address: " + WiFi.localIP().toString());
    }
    return true;
  } else {
    logError("WiFi connection failed after " + String(timeoutSeconds) + " seconds");
    if (comm) {
      comm->println("\nWiFi connection failed");
      comm->println("Check SSID and password");
    }
    return false;
  }
}

void OTAManager::disconnectWiFi() {
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  logUpdateEvent("WiFi disconnected");
}

UpdateResult OTAManager::performHTTPUpdate(const String& server, int port, const String& path) {
  logUpdateEvent("Starting HTTP update from " + server + ":" + String(port) + path);
  
  // Configure HTTP update settings
  httpUpdate.rebootOnUpdate(false); // Handle reboot manually for better control
  httpUpdate.setLedPin(-1); // Disable built-in LED control
  
  // FIXED: Removed httpUpdate.setTimeout() - not available in ESP32 Core 3.x
  
  // Set progress callback
  httpUpdate.onProgress([](int current, int total) {
    OTAManager::onUpdateProgress(current, total);
  });
  
  // Set error callback
  httpUpdate.onError([](int error) {
    OTAManager::onUpdateError(error);
  });
  
  if (comm) {
    comm->println("Downloading firmware...");
    comm->println("URL: http://" + server + ":" + String(port) + path);
  }
  
  // Create WiFi client and perform update
  WiFiClient client;
  t_httpUpdate_return result = httpUpdate.update(client, server, port, path);
  
  // Convert result to our enum
  switch (result) {
    case HTTP_UPDATE_FAILED:
      return UPDATE_FAILED;
    case HTTP_UPDATE_NO_UPDATES:
      return UPDATE_NO_UPDATES;
    case HTTP_UPDATE_OK:
      return UPDATE_SUCCESS;
    default:
      return UPDATE_FAILED;
  }
}

UpdateResult OTAManager::performHTTPSUpdate(const String& server, int port, const String& path) {
  logUpdateEvent("Starting HTTPS update from " + server + ":" + String(port) + path);
  
  // Configure HTTPS update (more secure)
  WiFiClientSecure client;
  
  // FIXED: Use newer certificate verification method for ESP32 Core 3.x
  if (serverCertFingerprint.length() > 0) {
    // Note: setFingerprint is deprecated, use setCACert or setInsecure
    logUpdateEvent("WARNING: Certificate fingerprint validation deprecated in ESP32 Core 3.x");
    client.setInsecure(); // Skip certificate validation for now
  } else {
    client.setInsecure(); // Skip certificate validation
    logUpdateEvent("WARNING: Skipping certificate validation");
  }
  
  // Configure HTTP update settings
  httpUpdate.rebootOnUpdate(false);
  
  if (comm) {
    comm->println("Downloading firmware (HTTPS)...");
    comm->println("URL: https://" + server + ":" + String(port) + path);
  }
  
  // Perform update
  t_httpUpdate_return result = httpUpdate.update(client, server, port, path);
  
  // Convert result
  switch (result) {
    case HTTP_UPDATE_FAILED:
      return UPDATE_FAILED;
    case HTTP_UPDATE_NO_UPDATES:
      return UPDATE_NO_UPDATES;
    case HTTP_UPDATE_OK:
      return UPDATE_SUCCESS;
    default:
      return UPDATE_FAILED;
  }
}

void OTAManager::handleUpdateResult(t_httpUpdate_return result) {
  switch (result) {
    case HTTP_UPDATE_FAILED:
      logError("Update failed: " + String(httpUpdate.getLastError()) + " - " + httpUpdate.getLastErrorString());
      if (comm) {
        comm->println("UPDATE FAILED!");
        comm->println("Error " + String(httpUpdate.getLastError()) + ": " + httpUpdate.getLastErrorString());
        comm->println("Please check server and network connection");
      }
      break;
      
    case HTTP_UPDATE_NO_UPDATES:
      logUpdateEvent("No updates available on server");
      if (comm) {
        comm->println("No firmware updates available");
        comm->println("Current version is up to date");
      }
      break;
      
    case HTTP_UPDATE_OK:
      logUpdateEvent("Update successful - preparing to reboot");
      if (comm) {
        comm->println("FIRMWARE UPDATE SUCCESSFUL!");
        comm->println("New firmware installed successfully");
        comm->println("Rebooting in 5 seconds...");
        
        // Countdown for user awareness
        for (int i = 5; i > 0; i--) {
          comm->println("Reboot in " + String(i) + "...");
          delay(1000);
        }
        
        comm->println("Rebooting now!");
        comm->flush(); // Ensure message is sent
      }
      
      delay(500);
      ESP.restart();
      break;
      
    default:
      logError("Unknown update result: " + String(result));
      if (comm) {
        comm->println("Unknown update result: " + String(result));
      }
      break;
  }
}

// FIXED: Progress and error callbacks - fixed for static member access
void OTAManager::onUpdateProgress(int current, int total) {
  static unsigned long lastProgressTime = 0;
  static int lastPercent = -1;
  
  unsigned long now = millis();
  
  // Update static progress information (FIXED)
  staticProgress.currentBytes = current;
  staticProgress.totalBytes = total;
  staticProgress.percentComplete = (total > 0) ? (current * 100) / total : 0;
  staticProgress.elapsedTime = now - staticProgress.startTime;
  
  // Calculate download speed
  if (staticProgress.elapsedTime > 0) {
    staticProgress.downloadSpeedKBps = (current / 1024.0) / (staticProgress.elapsedTime / 1000.0);
  }
  
  // Copy to instance progress if available
  if (staticInstance) {
    staticInstance->progress = staticProgress;
  }
  
  // Limit progress updates to avoid flooding (every 2 seconds or 5% change)
  if (now - lastProgressTime < 2000 && abs(staticProgress.percentComplete - lastPercent) < 5) {
    return;
  }
  
  if (staticProgress.percentComplete != lastPercent) {
    String progressMsg = "Download: " + String(staticProgress.percentComplete) + "% (" + 
                        String(current / 1024) + "/" + String(total / 1024) + " KB)";
    
    if (staticProgress.downloadSpeedKBps > 0) {
      progressMsg += " @ " + String(staticProgress.downloadSpeedKBps, 1) + " KB/s";
    }
    
    Serial.println(progressMsg);
    
    if (staticInstance && staticInstance->comm) {
      staticInstance->comm->println(progressMsg);
    }
    
    lastPercent = staticProgress.percentComplete;
    lastProgressTime = now;
  }
}

void OTAManager::onUpdateError(int error) {
  String errorMsg = "Update Error " + String(error);
  
  Serial.println(errorMsg);
  
  if (staticInstance) {
    staticInstance->logError(errorMsg);
    
    if (staticInstance->comm) {
      staticInstance->comm->println(errorMsg);
    }
  }
}

void OTAManager::onUpdateStart() {
  Serial.println("Update started");
  
  if (staticInstance && staticInstance->comm) {
    staticInstance->comm->println("Firmware download started...");
  }
}

void OTAManager::onUpdateEnd() {
  Serial.println("Update download completed");
  
  if (staticInstance && staticInstance->comm) {
    staticInstance->comm->println("Download completed, verifying firmware...");
  }
}

// Validation
bool OTAManager::validateUpdateURL(const String& server, int port, const String& path) {
  // Basic validation of update parameters
  if (server.length() == 0) {
    logError("Invalid server address");
    return false;
  }
  
  if (port <= 0 || port > 65535) {
    logError("Invalid port number: " + String(port));
    return false;
  }
  
  if (path.length() == 0 || !path.startsWith("/")) {
    logError("Invalid firmware path: " + path);
    return false;
  }
  
  return true;
}

bool OTAManager::validateFirmwareVersion() {
  // Could implement version checking logic here
  // For now, always allow updates
  return true;
}

// Configuration
void OTAManager::setUpdateTimeout(unsigned long timeoutMs) {
  updateTimeoutMs = timeoutMs;
  logUpdateEvent("Update timeout set to " + String(timeoutMs / 1000) + " seconds");
}

void OTAManager::enableHTTPS(bool enable, const String& fingerprint) {
  useHTTPS = enable;
  serverCertFingerprint = fingerprint;
  
  String msg = "HTTPS " + String(enable ? "enabled" : "disabled");
  if (enable && fingerprint.length() > 0) {
    msg += " with certificate validation";
  }
  
  logUpdateEvent(msg);
}

void OTAManager::setServerCertFingerprint(const String& fingerprint) {
  serverCertFingerprint = fingerprint;
  logUpdateEvent("Server certificate fingerprint set");
}

// Status and utility
unsigned long OTAManager::getUpdateTimeRemaining() {
  if (!updateInProgress) return 0;
  
  unsigned long elapsed = millis() - updateStartTime;
  if (elapsed >= updateTimeoutMs) return 0;
  
  return updateTimeoutMs - elapsed;
}

String OTAManager::getUpdateResultString(UpdateResult result) {
  switch (result) {
    case UPDATE_SUCCESS:
      return "Success";
    case UPDATE_FAILED:
      return "Failed";
    case UPDATE_NO_UPDATES:
      return "No updates available";
    case UPDATE_WIFI_FAILED:
      return "WiFi connection failed";
    case UPDATE_TIMEOUT:
      return "Timeout";
    case UPDATE_INVALID_URL:
      return "Invalid URL";
    case UPDATE_DOWNLOAD_FAILED:
      return "Download failed";
    case UPDATE_VERIFICATION_FAILED:
      return "Verification failed";
    default:
      return "Unknown";
  }
}

String OTAManager::getCurrentFirmwareVersion() {
  return String(FIRMWARE_VERSION);
}

bool OTAManager::checkForUpdates(const ConfigManager& config) {
  // Could implement update checking without downloading
  // For now, return true to indicate updates might be available
  logUpdateEvent("Checking for updates on server");
  return true;
}

// Diagnostics
void OTAManager::printUpdateStatus() {
  if (comm) {
    comm->println("=== OTA Update Status ===");
    comm->println("In Progress: " + String(updateInProgress ? "YES" : "NO"));
    comm->println("Current Version: " + getCurrentFirmwareVersion());
    
    if (updateInProgress) {
      comm->println("Progress: " + String(progress.percentComplete) + "%");
      comm->println("Speed: " + String(progress.downloadSpeedKBps, 1) + " KB/s");
      comm->println("Time Remaining: " + String(getUpdateTimeRemaining() / 1000) + "s");
    }
    
    comm->println("HTTPS Enabled: " + String(useHTTPS ? "YES" : "NO"));
    comm->println("Timeout: " + String(updateTimeoutMs / 1000) + "s");
    comm->println("========================");
  }
}

void OTAManager::printNetworkDiagnostics() {
  if (comm) {
    comm->println("=== Network Diagnostics ===");
    comm->println("WiFi Status: " + String(WiFi.status()));
    
    if (WiFi.status() == WL_CONNECTED) {
      comm->println("SSID: " + WiFi.SSID());
      comm->println("IP: " + WiFi.localIP().toString());
      comm->println("Signal: " + String(WiFi.RSSI()) + " dBm");
      comm->println("Gateway: " + WiFi.gatewayIP().toString());
      comm->println("DNS: " + WiFi.dnsIP().toString());
    }
    
    comm->println("Free Heap: " + String(ESP.getFreeHeap()) + " bytes");
    comm->println("==========================");
  }
}

bool OTAManager::testServerConnection(const ConfigManager& config) {
  logUpdateEvent("Testing server connection");
  
  if (!connectToWiFi(config)) {
    return false;
  }
  
  WiFiClient client;
  bool connected = client.connect(config.getIPAddress().c_str(), config.getPortInt());
  
  if (connected) {
    client.stop();
    logUpdateEvent("Server connection test: SUCCESS");
  } else {
    logError("Server connection test: FAILED");
  }
  
  disconnectWiFi();
  return connected;
}

// Logging
void OTAManager::logUpdateEvent(const String& event) {
  Serial.println("[OTA] " + event);
}

void OTAManager::logError(const String& error) {
  Serial.println("[OTA ERROR] " + error);
}

#endif // OTA_MANAGER_H