#include "improv_setup.h"

#if IMPROV_SETUP_ENABLED

#include <Arduino.h>
#include <WiFi.h>
#include <ImprovWiFiLibrary.h>
#include <esp_task_wdt.h>

#include "../config/globals.h"
#include "debug.h"

static ImprovWiFi improvSerial(&Serial);
static bool improvSucceeded = false;

// Bytes handled per improvTick(). The library parses one byte per
// handleSerial() call; at one call per loop() pass an 11-byte request would
// take 11 passes and could miss ESP Web Tools' 1.5s Connect window.
constexpr int IMPROV_BYTES_PER_TICK = 64;

static ImprovTypes::ChipFamily detectChipFamily() {
#if CONFIG_IDF_TARGET_ESP32S3
  return ImprovTypes::ChipFamily::CF_ESP32_S3;
#elif CONFIG_IDF_TARGET_ESP32C3
  return ImprovTypes::ChipFamily::CF_ESP32_C3;
#elif CONFIG_IDF_TARGET_ESP32S2
  return ImprovTypes::ChipFamily::CF_ESP32_S2;
#else
  return ImprovTypes::ChipFamily::CF_ESP32;
#endif
}

static String buildDeviceName() {
  uint32_t mac = (uint32_t)(ESP.getEfuseMac() & 0xFFFF);
  char buf[32];
  snprintf(buf, sizeof(buf), "PixelClock-%04X", mac);
  return String(buf);
}

static void onImprovError(ImprovTypes::Error err) {
  DBG_INFO("Improv: error %d", (int)err);
}

static void onImprovConnected(const char *ssid, const char *password) {
  // improvCustomConnect() has already connected and persisted the credentials
  // to the ESP WiFi NVS, where WiFiManager.autoConnect() finds them next boot.
  DBG_INFO("Improv: connected as %s, credentials saved", ssid);
  improvSucceeded = true;
}

// Custom connect keeps WIFI_AP_STA mode intact so the WiFiManager captive
// portal AP (when running) stays reachable during the STA attempt. The 15s
// wait feeds the task watchdog (also 15s) because it can now run from loop().
// Credentials are saved as soon as WiFi.begin() runs, so a failed attempt
// puts the previous network back - otherwise a typo in "Change WiFi" would
// replace working credentials.
static bool improvCustomConnect(const char *ssid, const char *password) {
  const String prevSsid = WiFi.SSID();
  const String prevPass = WiFi.psk();
  DBG_INFO("Improv: attempting STA connect to %s", ssid);
  WiFi.persistent(true);  // store creds so the next boot connects silently
  WiFi.begin(ssid, password);
  const uint32_t deadline = millis() + 15000;  // 15s STA connect timeout
  while (WiFi.status() != WL_CONNECTED &&
         (int32_t)(millis() - deadline) < 0) {
    esp_task_wdt_reset();
    delay(100);
  }
  if (WiFi.status() == WL_CONNECTED) return true;

  if (prevSsid.length() > 0) {
    DBG_WARN("Improv: could not join %s, restoring %s", ssid, prevSsid.c_str());
    WiFi.begin(prevSsid.c_str(), prevPass.c_str());
  }
  return false;
}

void improvBegin() {
  String deviceName = buildDeviceName();
  improvSerial.setDeviceInfo(
      detectChipFamily(),
      PROJECT_NAME,
      FIRMWARE_VERSION,
      deviceName.c_str(),
      "http://{LOCAL_IPV4}/");
  improvSerial.onImprovError(onImprovError);
  improvSerial.onImprovConnected(onImprovConnected);
  improvSerial.setCustomConnectWiFi(improvCustomConnect);
  DBG_INFO("Improv: listening on Serial as %s", deviceName.c_str());
}

void improvTick() {
  for (int n = 0; n < IMPROV_BYTES_PER_TICK && Serial.available() > 0; n++) {
    improvSerial.handleSerial();
  }
  if (!improvSucceeded) return;
  // The library sent "provisioned" and the device URL before the callback ran;
  // flush so they reach the browser, then restart onto the new network.
  DBG_WARN("Improv: new WiFi credentials, restarting");
  Serial.flush();
  ESP.restart();
}

#endif // IMPROV_SETUP_ENABLED
