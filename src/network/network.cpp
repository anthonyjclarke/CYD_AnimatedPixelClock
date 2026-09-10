/*
 * CYD_AnimatedPixelClock - Network Module
 *
 * WiFi connection management and NTP sync.
 */

#include "network.h"
#include "../display/display.h"
#include "../utils/utils.h"
#include "../timezones.h"
#include "improv_setup.h"
#include <Preferences.h>
#include <esp_wifi.h>
#include "ping/ping_sock.h"

#if QR_SETUP_ENABLED
#include "qrcode.h"
#include "debug.h"
#endif

// Global network objects
WiFiManager wifiManager;
extern Preferences preferences;

// ========== WiFi Callbacks ==========
void configModeCallback(WiFiManager *myWiFiManager) {
  DBG_INFO("Config mode entered, AP IP %s", WiFi.softAPIP().toString().c_str());

  if (displayAvailable) {
#if QR_SETUP_ENABLED
    displayQRCodeSetup();
#else
    displaySetupInstructions();
#endif
  }
}

void saveConfigCallback() {
  if (displayAvailable) {
    displayConnecting();
  }
}

// ========== Static IP Application ==========
void applyStaticIP() {
  if (settings.useStaticIP) {
    IPAddress local_IP, gateway_IP, subnet_IP, dns1_IP;

    if (local_IP.fromString(settings.staticIP) &&
        gateway_IP.fromString(settings.gateway) &&
        subnet_IP.fromString(settings.subnet) &&
        dns1_IP.fromString(settings.dns1)) {

      DBG_INFO("Configuring Static IP...");
      DBG_INFO("IP: %s", local_IP.toString().c_str());
      DBG_INFO("Gateway: %s", gateway_IP.toString().c_str());
      DBG_INFO("Subnet: %s", subnet_IP.toString().c_str());
      DBG_INFO("DNS1: %s", dns1_IP.toString().c_str());

      wifiManager.setSTAStaticIPConfig(local_IP, gateway_IP, subnet_IP, dns1_IP);
    } else {
      DBG_WARN("Invalid static IP configuration, using DHCP");
    }
  }
}

// ========== Manual WiFi Connection ==========
bool connectManualWiFi(const char* ssid, const char* password) {
  if (displayAvailable) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(10, 20);
    display.println("Connecting to");
    display.setCursor(10, 35);
    display.println(ssid);
    display.display();
  }

  WiFi.mode(WIFI_STA);

  // Apply static IP configuration if enabled
  if (settings.useStaticIP) {
    IPAddress local_IP, gateway_IP, subnet_IP, dns1_IP, dns2_IP;

    if (local_IP.fromString(settings.staticIP) &&
        gateway_IP.fromString(settings.gateway) &&
        subnet_IP.fromString(settings.subnet) &&
        dns1_IP.fromString(settings.dns1)) {

      dns2_IP.fromString(settings.dns2);

      DBG_INFO("Configuring Static IP for manual WiFi...");
      if (!WiFi.config(local_IP, gateway_IP, subnet_IP, dns1_IP, dns2_IP)) {
        DBG_ERROR("Static IP configuration failed!");
      }
    } else {
      DBG_WARN("Invalid static IP configuration, using DHCP");
    }
  }

  WiFi.begin(ssid, password);

  int attempts = 0;
  int maxAttempts = 30;

  while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
    delay(1000);
    attempts++;

    if (displayAvailable && attempts % 5 == 0) {
      display.clearDisplay();
      display.setTextSize(1);
      display.setCursor(10, 20);
      display.println("Connecting...");
      display.setCursor(10, 35);
      display.print("Attempt: ");
      display.print(String(attempts).c_str());
      display.print("/");
      display.println(String(maxAttempts).c_str());
      display.display();
    }
  }


  if (WiFi.status() == WL_CONNECTED) {
    DBG_INFO("WiFi Connected!");
    DBG_INFO("IP Address: %s", WiFi.localIP().toString().c_str());
    return true;
  } else {
    DBG_ERROR("WiFi Connection Failed!");
    return false;
  }
}

// ========== Network Initialization ==========
void initNetwork() {
  // Apply static IP if configured
  applyStaticIP();

  // Configure WiFiManager
  wifiManager.setConnectTimeout(30);
  wifiManager.setConfigPortalTimeout(180);
  wifiManager.setAPCallback(configModeCallback);
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  wifiManager.setDebugOutput(false);

#if IMPROV_SETUP_ENABLED
  // Run the config portal non-blocking so we can pump the Improv-Serial
  // listener in parallel (the web-flasher "Configure WiFi" path). setup() still
  // blocks here until WiFi connects or the portal times out - see the loop
  // below - so the rest of boot behaves exactly as before.
  wifiManager.setConfigPortalBlocking(false);
#endif

  bool connected = (strlen(AP_PASSWORD) > 0)
    ? wifiManager.autoConnect(AP_NAME, AP_PASSWORD)
    : wifiManager.autoConnect(AP_NAME);

#if IMPROV_SETUP_ENABLED
  if (!connected) {
    // autoConnect() started the captive portal (non-blocking). On genuinely
    // fresh devices (no stored SSID) also open an Improv-Serial window so a
    // browser that just flashed via the web flasher can push credentials over
    // USB. Returning users who only mistyped a password get the AP portal
    // alone - they need to fix what they typed, not a serial dialog over a
    // port that may not even be connected anymore.
    bool freshDevice = !wifiManager.getWiFiIsSaved();
    if (freshDevice) {
      improvSetupBegin(IMPROV_SETUP_WINDOW_MS);
    }

    while (!connected && wifiManager.getConfigPortalActive()) {
      // Service the captive portal (DNS + web server).
      if (wifiManager.process()) {
        connected = true;
        break;
      }
      // Service Improv-Serial. On success the library has already saved the
      // credentials and connected STA, so restart for a clean STA-only boot.
      if (freshDevice && improvSetupTick()) {
        DBG_ERROR("Improv: credentials received, restarting");
        Serial.flush();
        delay(200);  // let the response reach the browser before reset
        ESP.restart();
      }
      delay(5);
    }
    improvSetupEnd();
  }
#endif

  if (!connected) {
    DBG_WARN("Failed to connect and hit timeout");
    if (displayAvailable) {
      display.clearDisplay();
      display.setCursor(10, 20);
      display.println("WiFi Timeout!");
      display.setCursor(10, 35);
      display.println("Restarting...");
      display.display();
    }
    delay(3000);
    ESP.restart();
  }

  DBG_INFO("WiFi Connected!");
  DBG_INFO("IP Address: %s", WiFi.localIP().toString().c_str());

  // Set WiFi TX power to maximum for better range
  WiFi.setTxPower(WIFI_POWER_19_5dBm);


  // Start mDNS for app discovery
  initMDNS();
}

// ========== mDNS Service Discovery ==========
void initMDNS() {
  MDNS.end();  // Stop any previous mDNS instance
  if (MDNS.begin(settings.deviceName)) {
    MDNS.addService("http", "tcp", 80);
    MDNS.addServiceTxt("http", "tcp", "version", FIRMWARE_VERSION);
    MDNS.addServiceTxt("http", "tcp", "model", "AnimatedPixelClock");
    MDNS.addServiceTxt("http", "tcp", "mac", WiFi.macAddress().c_str());
    DBG_INFO("mDNS started: %s.local", settings.deviceName);
  } else {
    DBG_WARN("mDNS failed to start");
  }
}

// ========== NTP Functions ==========
void applyTimezone() {
  // Configured servers, falling back to the compiled defaults. A blank
  // secondary is passed as nullptr so SNTP skips the slot.
  const char* ntp1 = strlen(settings.ntpServer1) > 0 ? settings.ntpServer1
                                                     : NTP_SERVER_PRIMARY;
  const char* ntp2 = strlen(settings.ntpServer2) > 0 ? settings.ntpServer2
                                                     : nullptr;

  // If timezone string is set, use automatic DST with configTzTime()
  if (strlen(settings.timezoneString) > 0) {
    configTzTime(settings.timezoneString, ntp1, ntp2);
    DBG_INFO("Timezone set (automatic DST): %s", settings.timezoneString);
  }
  else {
    // Fallback: Try to map old GMT offset to default timezone
    const char* defaultTz = getDefaultTimezoneForOffset(settings.gmtOffset);
    if (defaultTz != nullptr) {
      configTzTime(defaultTz, ntp1, ntp2);
      DBG_INFO("Auto-detected timezone: %s", defaultTz);
    }
    else {
      // Ultimate fallback: Manual offset without DST
      int gmtOffset_sec = settings.gmtOffset * 60;
      configTime(gmtOffset_sec, 0, ntp1, ntp2);
      DBG_INFO("Manual offset (no DST): GMT%+d", settings.gmtOffset / 60);
    }
  }
  DBG_INFO("NTP servers: %s, %s", ntp1, ntp2 ? ntp2 : "(none)");
}

void initNTP() {
  applyTimezone();
  ntpSynced = false;

  if (displayAvailable) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(10, 20);
    display.println("Syncing time...");
    display.display();
  }

  struct tm timeinfo;
  for (int i = 0; i < 30; i++) {
    if (getLocalTime(&timeinfo, 100)) {
      if (timeinfo.tm_year > 120) {
        ntpSynced = true;
        lastNtpSyncTime = millis();
        DBG_INFO("NTP time synchronized successfully");
        break;
      }
    }
    delay(100);
  }

  if (!ntpSynced) {
    DBG_WARN("NTP sync pending, will retry in background");
  }
}

// ========== Link Health ==========
// WiFi.status() stays WL_CONNECTED even when the stack moves no traffic at all,
// so the association record and a gateway ping decide instead.

#define NET_IDLE_BEFORE_PROBE_MS 120000UL
#define NET_PROBE_RETRY_MS 60000UL
#define NET_PROBE_FAILS_BEFORE_RECOVERY 2
#define NET_REBOOT_AFTER_MS 360000UL

static uint32_t netLastHttpMs = 0;
static uint32_t netLastTrafficMs = 0;
static uint32_t netBadSinceMs = 0;
static uint32_t netLastRecoverMs = 0;
static uint32_t netNextProbeMs = 0;
static uint32_t netHttpCount = 0;
static uint32_t netRecoverCount = 0;
static uint8_t netProbeFails = 0;
static const char* netRecoverReason = "";
static esp_ping_handle_t netPing = nullptr;
static volatile bool netPingReplied = false;
static volatile bool netPingDone = false;

static void netMarkAlive() {
  netLastTrafficMs = millis();
  netBadSinceMs = 0;
  netProbeFails = 0;
}

void netMarkHttp() {
  netHttpCount++;
  netLastHttpMs = millis();
  netMarkAlive();
}

void netMarkInbound() { netMarkAlive(); }
void netMarkOutboundOk() { netMarkAlive(); }

uint32_t netHttpServed() { return netHttpCount; }
uint32_t netSecsSinceHttp() { return netLastHttpMs ? (millis() - netLastHttpMs) / 1000 : 0; }
uint32_t netSecsSinceTraffic() { return (millis() - netLastTrafficMs) / 1000; }
uint32_t netRecoveryCount() { return netRecoverCount; }
const char* netLastRecoveryReason() { return netRecoverReason; }

static void netPingSuccess(esp_ping_handle_t, void*) { netPingReplied = true; }
static void netPingEnd(esp_ping_handle_t, void*) { netPingDone = true; }

static void netPingRelease() {
  if (!netPing) return;
  esp_ping_stop(netPing);
  esp_ping_delete_session(netPing);
  netPing = nullptr;
}

static bool netStartProbe() {
  if (netPing) return false;
  uint32_t gw = (uint32_t)WiFi.gatewayIP();
  if (!gw) return false;

  esp_ping_config_t cfg;
  memset(&cfg, 0, sizeof(cfg));
  cfg.count = 3;
  cfg.interval_ms = 300;
  cfg.timeout_ms = 1000;
  cfg.data_size = 16;
  cfg.ttl = 64;
  cfg.task_stack_size = 3072;
  cfg.task_prio = 2;
  cfg.target_addr.type = IPADDR_TYPE_V4;
  cfg.target_addr.u_addr.ip4.addr = gw;

  esp_ping_callbacks_t cb;
  memset(&cb, 0, sizeof(cb));
  cb.on_ping_success = netPingSuccess;
  cb.on_ping_end = netPingEnd;

  netPingReplied = false;
  netPingDone = false;
  if (esp_ping_new_session(&cfg, &cb, &netPing) != ESP_OK) {
    netPing = nullptr;
    return false;
  }
  esp_ping_start(netPing);
  return true;
}

// Full radio restart. WiFi.reconnect() alone does not recover this state.
static void netRecover(const char* why) {
  uint32_t now = millis();
  if (!netBadSinceMs) netBadSinceMs = now;
  if (now - netBadSinceMs > NET_REBOOT_AFTER_MS) {
    DBG_INFO("Link dead (%s) despite recovery, restarting", why);
    Serial.flush();
    delay(100);
    ESP.restart();
  }

  netPingRelease();
  netRecoverReason = why;
  netRecoverCount++;
  netProbeFails = 0;
  netLastRecoverMs = now;
  netNextProbeMs = now + NET_PROBE_RETRY_MS;
  netLastTrafficMs = now;

  DBG_INFO("Link recovery (%s): restarting WiFi", why);
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(200);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin();

  wifiConnected = false;
  wifiDisconnectTime = now;  // makes the reconnect branch re-init mDNS
}

static void netHealthTick() {
  uint32_t now = millis();
  if (!netLastTrafficMs) netLastTrafficMs = now;
  bool cooling = (now - netLastRecoverMs) < NET_PROBE_RETRY_MS;

  wifi_ap_record_t ap;
  if (esp_wifi_sta_get_ap_info(&ap) != ESP_OK) {
    if (!cooling) netRecover("no AP association");
    return;
  }

  if (netPing) {
    if (!netPingDone) return;
    bool replied = netPingReplied;
    netPingRelease();
    netNextProbeMs = now + NET_PROBE_RETRY_MS;
    if (replied) {
      netMarkAlive();
    } else if (++netProbeFails >= NET_PROBE_FAILS_BEFORE_RECOVERY && !cooling) {
      netRecover("gateway unreachable");
    }
    return;
  }

  if (now - netLastTrafficMs < NET_IDLE_BEFORE_PROBE_MS) return;
  if ((int32_t)(now - netNextProbeMs) < 0) return;
  netNextProbeMs = now + NET_PROBE_RETRY_MS;
  netStartProbe();
}

// ========== WiFi Reconnection Handling ==========
// Reconnection interval in milliseconds (try every 30 seconds)
#define WIFI_RECONNECT_INTERVAL 30000

void handleWiFiReconnection() {
  static unsigned long lastReconnectAttempt = 0;

  if (WiFi.status() != WL_CONNECTED) {
    // Update global flag for icon display
    wifiConnected = false;

    if (wifiDisconnectTime == 0) {
      wifiDisconnectTime = millis();
      DBG_WARN("WiFi disconnected");
    }

    // Periodic reconnection attempt every 30 seconds
    unsigned long currentMillis = millis();
    if (currentMillis - lastReconnectAttempt > WIFI_RECONNECT_INTERVAL) {
      DBG_INFO("Attempting WiFi reconnection...");
      WiFi.reconnect();
      lastReconnectAttempt = currentMillis;
    }

    // NOTE: Auto-reboot removed - device continues as clock-only
    // NOTE: Display drawing removed - clock functions show small icon instead
  } else {
    // WiFi is connected
    wifiConnected = true;

    if (wifiDisconnectTime != 0) {
      DBG_INFO("WiFi reconnected successfully!");
      DBG_INFO("IP Address: %s", WiFi.localIP().toString().c_str());
      wifiDisconnectTime = 0;
      ntpSynced = false;  // Force NTP resync after reconnection
      applyTimezone();    // Restart SNTP client and reapply timezone
      initMDNS();         // Re-register mDNS after reconnection
      netMarkOutboundOk();
    }

    netHealthTick();
  }
}

// ========== Display Status Screens ==========
void displaySetupInstructions() {
  display.clearDisplay();
  display.setTextSize(1);

  display.setCursor(20, 0);
  display.println("WiFi Setup");
  display.drawLine(0, 10, 128, 10, DISPLAY_WHITE);

  display.setCursor(0, 14);
  display.println("1.Connect to WiFi:");

  display.setCursor(0, 26);
  display.print("  ");
  display.println(AP_NAME);

  display.setCursor(0, 38);
  if (strlen(AP_PASSWORD) > 0) {
    display.print("  Pass: ");
    display.println(AP_PASSWORD);
  } else {
    display.println("  (no password)");
  }

  display.setCursor(0, 50);
  display.println("2.Open 192.168.4.1");

  display.display();
}

#if QR_SETUP_ENABLED
void displayQRCodeSetup() {
  // WiFi QR format: open AP uses T:nopass, secured uses T:WPA
  char qrData[80];
  if (strlen(AP_PASSWORD) > 0) {
    snprintf(qrData, sizeof(qrData), "WIFI:T:WPA;S:%s;P:%s;;", AP_NAME, AP_PASSWORD);
  } else {
    snprintf(qrData, sizeof(qrData), "WIFI:T:nopass;S:%s;;", AP_NAME);
  }

  // QR Version 3 = 29x29 modules, fits 53 alphanumeric chars with ECC_LOW
  QRCode qrcode;
  uint8_t qrcodeBytes[qrcode_getBufferSize(3)];
  qrcode_initText(&qrcode, qrcodeBytes, 3, ECC_LOW, qrData);

  display.clearDisplay();

  // Layout: text on left, QR code on right
  // QR: 29x29 modules * 2px = 58x58 pixels, right-aligned, vertically centered
  const uint8_t qrSize = qrcode.size;       // 29 for Version 3
  const uint8_t pixelSize = 2;              // 2x2 pixels per module
  const uint8_t qrDisplaySize = qrSize * pixelSize;  // 58 pixels
  const uint8_t qrX = SCREEN_WIDTH - qrDisplaySize - 1;  // right side with 1px margin
  const uint8_t qrY = (SCREEN_HEIGHT - qrDisplaySize) / 2;  // vertically centered

  // Draw QR code
  for (uint8_t y = 0; y < qrSize; y++) {
    for (uint8_t x = 0; x < qrSize; x++) {
      if (qrcode_getModule(&qrcode, x, y)) {
        display.fillRect(qrX + (x * pixelSize), qrY + (y * pixelSize),
                         pixelSize, pixelSize, DISPLAY_WHITE);
      }
    }
  }

  // Text labels on the left side (68px available, ~11 chars at 6px each)
  display.setTextSize(1);
  display.setCursor(0, 4);
  display.println("Scan QR");
  display.setCursor(0, 14);
  display.println("to join");
  display.setCursor(0, 24);
  display.println("WiFi");

  display.setCursor(0, 42);
  display.println("Open:");
  display.setCursor(0, 52);
  display.println("192.168.4.1");

  display.display();
}
#endif

void displayConnecting() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(20, 25);
  display.println("Connecting to");
  display.setCursor(30, 40);
  display.println("WiFi...");
  display.display();
}

void displayConnected() {
  display.clearDisplay();
  display.setTextSize(1);

  display.setCursor(25, 4);
  display.println("Connected!");

  display.setCursor(8, 18);
  display.println("IP (for Python):");

  String ip = WiFi.localIP().toString();
  int ip_width = ip.length() * 6;
  int ip_x = (SCREEN_WIDTH - ip_width) / 2;
  display.setCursor(ip_x, 30);
  display.println(ip.c_str());

  display.drawLine(0, 42, 128, 42, DISPLAY_WHITE);

  display.setCursor(4, 48);
  display.println("Open IP in browser");
  display.setCursor(12, 56);
  display.println("to change settings");

  display.display();
}
