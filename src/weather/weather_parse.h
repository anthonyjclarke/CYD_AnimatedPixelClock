#pragma once
#include "weather.h"
#include <ArduinoJson.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cctype>

inline int8_t weatherWeekday(const char* iso) {
  int y, m, d;
  if (!iso || sscanf(iso, "%d-%d-%d", &y, &m, &d) != 3 || y < 1970 || y > 9999 || m < 1 || m > 12) return -1;
  static const int days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
  bool leap = y%4 == 0 && (y%100 != 0 || y%400 == 0);
  if (d < 1 || d > days[m-1] + (m == 2 && leap)) return -1;
  static const int T[] = {0,3,2,5,0,3,5,1,4,6,2,4};
  if (m < 3) --y;
  return (y+y/4-y/100+y/400+T[m-1]+d)%7;
}
inline void weatherClockTime(const char* iso, char out[6]) {
  out[0]=0;
  const char* t=iso?strchr(iso,'T'):nullptr;
  if (t && strlen(t)>=6) {memcpy(out,t+1,5);out[5]=0;}
}
inline int weatherChance(JsonVariantConst v) {
  if (!v.is<int>()) return -1;
  int n=v.as<int>();return n>=0 && n<=100?n:-1;
}
inline bool weatherTemperatureValid(float value) {
  // Reject corrupt/non-finite values before graph arithmetic or integer text.
  return std::isfinite(value) && value >= -150 && value <= 150;
}
inline bool decodeWeather(const JsonDocument& doc, WeatherData& fresh) {
  JsonObjectConst current=doc["current"], daily=doc["daily"];
  if (current.isNull() || daily.isNull() || !current["temperature_2m"].is<float>()) return false;
  fresh={};fresh.hourStart=-1;
  fresh.tempC=current["temperature_2m"].as<float>();
  if (!weatherTemperatureValid(fresh.tempC)) return false;
  fresh.valid=true;fresh.humidity=current["relative_humidity_2m"] | 0;
  fresh.isDay=(current["is_day"] | 1)!=0;
  fresh.weatherCode=current["weather_code"] | 3;
  fresh.windKmh=current["wind_speed_10m"] | 0.0f;
  fresh.tempMaxC=daily["temperature_2m_max"][0] | fresh.tempC;
  fresh.tempMinC=daily["temperature_2m_min"][0] | fresh.tempC;
  if (!weatherTemperatureValid(fresh.tempMaxC)) fresh.tempMaxC=fresh.tempC;
  if (!weatherTemperatureValid(fresh.tempMinC)) fresh.tempMinC=fresh.tempC;
  weatherClockTime(daily["sunrise"][0],fresh.sunrise);
  weatherClockTime(daily["sunset"][0],fresh.sunset);
  for (int i=0;i<WEATHER_FORECAST_DAYS;++i) {
    WeatherDay& day=fresh.days[i];day.wday=-1;
    day.precipChance=weatherChance(daily["precipitation_probability_max"][i+1]);
    if (!daily["temperature_2m_max"][i+1].is<float>() || !daily["temperature_2m_min"][i+1].is<float>()) continue;
    day.tempMaxC=daily["temperature_2m_max"][i+1].as<float>();
    day.tempMinC=daily["temperature_2m_min"][i+1].as<float>();
    if (!weatherTemperatureValid(day.tempMaxC) || !weatherTemperatureValid(day.tempMinC)) continue;
    day.wday=weatherWeekday(daily["time"][i+1]);
    day.weatherCode=daily["weather_code"][i+1] | 3;
  }
  JsonObjectConst hourly=doc["hourly"];
  const char* first=hourly["time"][0];const char* t=first?strchr(first,'T'):nullptr;
  int hour=-1;
  if (t && strlen(t)>=3 && isdigit((unsigned char)t[1]) && isdigit((unsigned char)t[2])) hour=(t[1]-'0')*10+t[2]-'0';
  bool hoursOK=hour>=0 && hour<24;
  for (int i=0;i<WEATHER_FORECAST_HOURS;++i) {
    auto value=hourly["temperature_2m"][i];
    fresh.hourTempC[i]=value | 0.0f;
    fresh.hourPrecip[i]=weatherChance(hourly["precipitation_probability"][i]);
    hoursOK=hoursOK && value.is<float>() && weatherTemperatureValid(fresh.hourTempC[i]);
  }
  if (hoursOK) fresh.hourStart=hour;
  return true;
}
