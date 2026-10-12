#pragma once
#include "globals.h"
#include "palette_import.h"
#include <ArduinoJson.h>
#include <cstdio>

inline void writeConfigExport(JsonDocument& doc,const Settings& settings) {
  doc["colorSchema"]=CYD_COLOR_SCHEMA;
  doc["matrixRainSpeed"]=settings.matrixRainSpeed;
  doc["matrixRainDensity"]=settings.matrixRainDensity;
  doc["matrixShowDate"]=settings.matrixShowDate;
  doc["matrixTransparent"]=settings.matrixTransparent;
  doc["matrixSmoothScroll"]=settings.matrixSmoothScroll;
  doc["matrixSmallClock"]=settings.matrixSmallClock;
  doc["cycleConfig"]=settings.cycleConfig;
  doc["clockStyle"]=settings.clockStyle;
  doc["tronBikeStyle"]=settings.tronBikeStyle;
  doc["doomFlameHeight"]=settings.doomFlameHeight;
  doc["doomGroundHeight"]=settings.doomGroundHeight;
  doc["doomWind"]=settings.doomWind;
  doc["doomShowDate"]=settings.doomShowDate;
  doc["doomBurningDigits"]=settings.doomBurningDigits;
  doc["doomSmoothFire"]=settings.doomSmoothFire;
  doc["ambientEnabled"]=settings.ambientEnabled;
  doc["ambientStyle"]=settings.ambientStyle;
  doc["ambientStartHour"]=settings.ambientStartHour;
  doc["ambientEndHour"]=settings.ambientEndHour;
  doc["ambientShowClock"]=settings.ambientShowClock;
  doc["timezoneString"]=settings.timezoneString;
  doc["gmtOffset"]=settings.gmtOffset;
  doc["daylightSaving"]=settings.daylightSaving;
  doc["use24Hour"]=settings.use24Hour;
  doc["hideAmPm"]=settings.hideAmPm;
  doc["dateFormat"]=settings.dateFormat;
  doc["clockPosition"]=settings.clockPosition;
  doc["clockOffset"]=settings.clockOffset;
  doc["deviceName"]=settings.deviceName;
  doc["showIPAtBoot"]=settings.showIPAtBoot;
  doc["ntpServer1"]=settings.ntpServer1;
  doc["ntpServer2"]=settings.ntpServer2;
  doc["notifyEnabled"]=settings.notifyEnabled;
  doc["notifyPosition"]=settings.notifyPosition;
  doc["weatherEnabled"]=settings.weatherEnabled;
  doc["weatherLat"]=settings.weatherLat;
  doc["weatherLon"]=settings.weatherLon;
  doc["weatherUseFahrenheit"]=settings.weatherUseFahrenheit;
  doc["weatherApiKey"]=settings.weatherApiKey;
  doc["weatherPlace"]=settings.weatherPlace;
  doc["weatherLayout"]=settings.weatherLayout;
  JsonArray colors=doc["spriteColors"].to<JsonArray>();
  for (int i=0;i<COL_COUNT;++i) {
    uint16_t c=settings.spriteColors[i];char hex[8];
    snprintf(hex,sizeof(hex),"#%02x%02x%02x",((c>>11)&31)*255/31,((c>>5)&63)*255/63,(c&31)*255/31);
    colors.add(hex);
  }
}
