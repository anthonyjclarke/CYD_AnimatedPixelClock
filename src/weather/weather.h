/*
 * AnimatedPixelClock - Weather Module (Open-Meteo)
 *
 * Background FreeRTOS task fetches current conditions + today's range every
 * 10 minutes and publishes them into a small struct the render loop reads.
 */

#ifndef WEATHER_H
#define WEATHER_H

#include <Arduino.h>

// Condition groups the icon renderer understands (mapped from WMO codes).
enum WeatherIconKind : uint8_t {
  WICON_SUN = 0,
  WICON_PARTCLOUD,
  WICON_CLOUD,
  WICON_FOG,
  WICON_RAIN,
  WICON_SNOW,
  WICON_STORM,
};

#define WEATHER_FORECAST_DAYS 3
#define WEATHER_FORECAST_HOURS 12

// One day after today, for the forecast layout
struct WeatherDay {
  int8_t wday;           // 0 = Sunday; -1 when the day is missing
  int weatherCode;       // raw WMO code
  float tempMaxC;
  float tempMinC;
  int precipChance;      // max precipitation probability %, -1 if unknown
};

struct WeatherData {
  bool valid;            // true once at least one fetch succeeded
  float tempC;           // current temperature (Celsius)
  float tempMinC;        // today's minimum
  float tempMaxC;        // today's maximum
  int humidity;          // relative humidity %
  float windKmh;         // wind speed km/h
  bool isDay;            // Open-Meteo day/night at the selected location
  int weatherCode;       // raw WMO code
  char sunrise[6];       // "HH:MM"
  char sunset[6];        // "HH:MM"
  WeatherDay days[WEATHER_FORECAST_DAYS];  // tomorrow onwards
  int8_t hourStart;      // local hour of hourTempC[0]; -1 when hourly data is missing
  float hourTempC[WEATHER_FORECAST_HOURS];
  int8_t hourPrecip[WEATHER_FORECAST_HOURS];  // precipitation probability %, -1 if unknown
  unsigned long fetchedAt;  // millis() of the last successful fetch
};

// Call each loop pass; creates a background task only when a fetch is due.
void weatherLoop();

// Thread-safe snapshot of the latest data.
WeatherData getWeather();

// True when the user has enabled weather and set a location.
bool weatherConfigured();

// Invalidate old-location data and schedule a fetch with the new settings.
void weatherSettingsChanged();

// Map a WMO weather code to an icon kind.
WeatherIconKind weatherIconFromCode(int wmoCode);

#endif // WEATHER_H
