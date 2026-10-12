/*
 * AnimatedPixelClock - Weather Module (Open-Meteo)
 *
 * The fetch (DNS + TLS handshake + transfer) can block for seconds, so it
 * runs in a task for one fetch, pinned to core 0 - never in loop(), where it would
 * visibly freeze a 60 Hz animation. Results are copied into `published`
 * under a spinlock; the render loop takes snapshots via getWeather().
 */

#include "weather.h"
#include "weather_parse.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "../config/globals.h"
#include "../clocks/cycle_config.h"
#include "../network/network.h"
#include "debug.h"

#define WEATHER_FETCH_INTERVAL_MS (10UL * 60UL * 1000UL)
#define WEATHER_RETRY_INTERVAL_MS (60UL * 1000UL)
#define WEATHER_IDLE_POLL_MS 5000UL

static WeatherData published = {};
static portMUX_TYPE weatherMux = portMUX_INITIALIZER_UNLOCKED;
struct WeatherRequest {float lat,lon;char key[33];uint32_t generation;};
static WeatherRequest request;
static bool fetchBusy=false, fetchKick=true;
static uint32_t generation=0, waitFrom=0, waitMs=0;

bool weatherConfigured() {
  // 0,0 (middle of the Atlantic) doubles as the "unset" marker.
  return settings.weatherEnabled &&
         !(settings.weatherLat == 0 && settings.weatherLon == 0);
}

// The TLS handshake is the largest allocation this firmware makes, so it only
// runs when a screen can actually show the result.
static bool weatherOnScreen() {
  if (settings.clockStyle == 14) return true;
  if (settings.clockStyle != 9) return false;
  CycleEntry entries[CYCLE_COUNT];
  if (!parseCycleConfig(settings.cycleConfig, entries)) return true;
  for (unsigned i = 0; i < CYCLE_COUNT; ++i)
    if (entries[i].style == 14 && entries[i].seconds) return true;
  return false;
}

WeatherData getWeather() {
  portENTER_CRITICAL(&weatherMux);
  WeatherData copy = published;
  portEXIT_CRITICAL(&weatherMux);
  return copy;
}

WeatherIconKind weatherIconFromCode(int code) {
  if (code == 0) return WICON_SUN;
  if (code <= 2) return WICON_PARTCLOUD;
  if (code == 3) return WICON_CLOUD;
  if (code == 45 || code == 48) return WICON_FOG;
  if (code >= 71 && code <= 77) return WICON_SNOW;
  if (code == 85 || code == 86) return WICON_SNOW;
  if (code >= 95) return WICON_STORM;
  // Everything else in the 5x/6x/8x ranges is some form of rain/drizzle.
  return WICON_RAIN;
}

static bool fetchWeather(const WeatherRequest& req) {
  char url[512];
  bool hasKey = req.key[0] != '\0';
  // The commercial tier uses the same API on a customer- host with an apikey.
  int urlLength = snprintf(url, sizeof(url),
      "https://%s/v1/forecast?latitude=%.4f&longitude=%.4f"
      "&current=temperature_2m,relative_humidity_2m,weather_code,wind_speed_10m,is_day"
      "&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max,sunrise,sunset"
      "&hourly=temperature_2m,precipitation_probability&forecast_hours=%d"
      "&timezone=auto&forecast_days=%d%s%s",
      hasKey?"customer-api.open-meteo.com":"api.open-meteo.com",req.lat,req.lon,
      WEATHER_FORECAST_HOURS,1+WEATHER_FORECAST_DAYS,hasKey?"&apikey=":"",hasKey?req.key:"");

  if (urlLength < 0 || urlLength >= (int)sizeof(url)) {
    DBG_WARN("Weather request URL too long");
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure();  // public, non-sensitive data; saves a cert bundle
  HTTPClient http;
  http.setTimeout(10000);
  http.useHTTP10(true);
  if (!http.begin(client, url)) return false;

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    DBG_INFO("Weather fetch failed: HTTP %d", code);
    http.end();
    return false;
  }

  JsonDocument filter;
  filter["current"]=true;filter["daily"]=true;filter["hourly"]=true;
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();
  if (err) {
    DBG_INFO("Weather JSON error: %s", err.c_str());
    return false;
  }

  WeatherData fresh;
  if (!decodeWeather(doc,fresh)) return false;
  fresh.fetchedAt=millis();
  portENTER_CRITICAL(&weatherMux);
  if (req.generation==generation) published=fresh;
  portEXIT_CRITICAL(&weatherMux);
  netMarkOutboundOk();

  DBG_INFO("Weather: %.1fC code %d (RH %d%%)", fresh.tempC,
                fresh.weatherCode, fresh.humidity);
  return true;
}

static void weatherFetchTask(void*) {
  const WeatherRequest snapshot=request;
  bool ok=fetchWeather(snapshot);
  portENTER_CRITICAL(&weatherMux);
  waitFrom=millis();waitMs=ok?WEATHER_FETCH_INTERVAL_MS:WEATHER_RETRY_INTERVAL_MS;
  fetchBusy=false;
  portEXIT_CRITICAL(&weatherMux);
  vTaskDelete(nullptr);
}

void weatherSettingsChanged() {
  portENTER_CRITICAL(&weatherMux);
  ++generation;fetchKick=true;published.valid=false;
  portEXIT_CRITICAL(&weatherMux);
}

void weatherLoop() {
  const uint32_t now=millis();
  portENTER_CRITICAL(&weatherMux);
  if (fetchBusy) {portEXIT_CRITICAL(&weatherMux);return;}
  if (fetchKick) {fetchKick=false;waitMs=0;}
  bool due=(uint32_t)(now-waitFrom)>=waitMs;
  portEXIT_CRITICAL(&weatherMux);
  if (!due) return;
  if (!weatherConfigured() || !weatherOnScreen() || WiFi.status()!=WL_CONNECTED) {
    portENTER_CRITICAL(&weatherMux);
    waitFrom=now;waitMs=WEATHER_IDLE_POLL_MS;
    portEXIT_CRITICAL(&weatherMux);return;
  }
  // Web/settings changes and this scheduler run on the Arduino loop core.
  request.lat=settings.weatherLat;request.lon=settings.weatherLon;
  memcpy(request.key,settings.weatherApiKey,sizeof(request.key));
  portENTER_CRITICAL(&weatherMux);
  request.generation=generation;fetchBusy=true;
  portEXIT_CRITICAL(&weatherMux);
  if (xTaskCreatePinnedToCore(weatherFetchTask,"weather",8192,nullptr,1,nullptr,0)!=pdPASS) {
    portENTER_CRITICAL(&weatherMux);
    fetchBusy=false;waitFrom=now;waitMs=WEATHER_RETRY_INTERVAL_MS;
    portEXIT_CRITICAL(&weatherMux);
    DBG_WARN("Weather task not started; retry in one minute");
  }
}
