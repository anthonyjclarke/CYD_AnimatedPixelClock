#!/usr/bin/env python3
"""Compile production clock code with host stand-ins, then exercise its state.

No device or network access. Requires c++ and the project's installed ArduinoJson
headers. Graphics calls record bounds/text rather than simulate TFT pixels.
"""
from pathlib import Path
import os
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
JSON = ROOT / '.pio/libdeps/esp32-cyd-28/ArduinoJson/src'


def read(name):
    return (ROOT / name).read_text()


def renderer(name):
    # Keep the production functions unchanged; replace only platform includes.
    return re.sub(r'^#include .*$', '', read(name), flags=re.MULTILINE)


common = read('src/clocks/clock_common.cpp')
colour = common[common.index('uint16_t digitColor()'):common.index('static void advanceDisplayTimeState')]
time_helpers = common[common.index('static void advanceDisplayTimeState'):common.index('// ========== Digit Bounce')]
standard = common[common.index('void displayStandardClock()'):common.index('// ========== Style identity') ]
targets = common[common.index('void calculateTargetDigits'):common.index('// ========== Standard Clock Display')]
web = read('src/web/web.cpp')
imports = web[web.index(' if (doc["matrixRainSpeed"].is<int>())'):web.index(' if (!doc["tronBikeStyle"].isNull())')]
exports = ''
settings_source = read('src/config/settings.cpp')
loads = settings_source[settings_source.index('  settings.matrixSmoothScroll = preferences'):settings_source.index('  settings.mcMissileSpeed =')]
saves = settings_source[settings_source.index('  preferences.putUChar("mxSpeed"'):settings_source.index('  preferences.putUChar("mcSpeed"')]
palette_defaults = settings_source[settings_source.index('const uint16_t SPRITE_COLOR_DEFAULTS[]'):settings_source.index('// Every ColorSlot')]
palette_load = settings_source[settings_source.index('  for (int i = 0; i < COL_COUNT; i++) {'):settings_source.index('  preferences.end();', settings_source.index('  for (int i = 0; i < COL_COUNT; i++) {'))]
colours = web[web.index(' // Sprite colors (array'):web.index(' // Save imported settings')]
time_import = web[web.index(' if (doc["hideAmPm"].is<bool>())'):web.index(' if (!doc["dateFormat"].isNull())')]
time_save = next(line for line in settings_source.splitlines() if 'preferences.putBool("hideAmPm"' in line)
time_load = next(line for line in settings_source.splitlines() if 'preferences.getBool("hideAmPm"' in line)
time_form = web[web.index(' if (server.hasArg("use24Hour"))'):web.index(' if (server.hasArg("dateFormat"))')]
time_export = ''
doom = read('src/clocks/clock_doom.cpp')
doom_outline = doom[doom.index('static void doomOutlineText'):doom.index('void displayClockWithDoom')]
request_helpers = web[web.index('static bool refuseCrossSite()'):web.index('// ========== Web Server Setup')]
ota_route = web[web.index(' static OtaUploadState ota;'):web.index(' server.begin();',web.index(' static OtaUploadState ota;'))]
layout_placeholder = web[web.index('  if (!strncmp(n,"SEL_WEATHERLAYOUT_'):web.index('  if (!strcmp(n, "V_WEATHERKEY"))')]
weather = read('src/weather/weather.cpp')
wmo = weather[weather.index('WeatherIconKind weatherIconFromCode'):weather.index('static bool fetchWeather')]

STUBS = r'''
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>
#include <functional>
#include <ArduinoJson.h>
#define PI 3.14159265358979323846f
#define TIME_OVERRIDE_MAX_MS 15000
#define TIME_Y TIME_Y_BASE
#define DBG_VERBOSE(...)
#define DBG_WARN(...)
#define DBG_INFO(...)
#define HTTP_POST 1
#define HTTP_ANY 0
#define UPDATE_SIZE_UNKNOWN 0
#define UPLOAD_FILE_START 0
#define UPLOAD_FILE_WRITE 1
#define UPLOAD_FILE_END 2
#define UPLOAD_FILE_ABORTED 3
#define DBG_INFO(...)
constexpr uint16_t DISPLAY_BLACK=0, DISPLAY_WHITE=65535;
unsigned long tick=1000;
unsigned long millis() { return tick; }
long random(long lo, long hi) { static uint32_t r=1; r=r*1664525+1013904223; return lo+r%(hi-lo); }
template<class T> T constrain(T v,T lo,T hi) { return std::max(lo,std::min(v,hi)); }
struct Settings {
  char cycleConfig[128];       // Ordered style:seconds pairs; 0 seconds disables
  // Clock settings
  uint8_t clockStyle;       // 0=Mario, 1=Standard, 2=Large, 3=Space Invader, 4=Space Ship, 5=Pong, 6=Pac-Man, 7=Snake, 8=Tetris, 9=Cycle All, 10=Asteroids, 11=Dino Runner, 12=Matrix Rain, 14=Weather, 15=Bomberman, 16=TRON, 17=Doom Fire (13 retired: Missile Command)
  int16_t gmtOffset;        // GMT offset in minutes (deprecated, kept for migration)
  bool daylightSaving;      // DST enabled (deprecated, kept for migration)
  char timezoneString[64];  // POSIX TZ string (e.g., "CET-1CEST,M3.5.0/02:00,M10.5.0/03:00")
  uint8_t timezoneIndex;    // Index into timezone database (for UI display)
  bool hideAmPm;           // Hide indicator while retaining 12-hour time
  bool use24Hour;           // 24-hour format
  uint8_t dateFormat;       // 0=DD/MM/YYYY, 1=MM/DD/YYYY, 2=YYYY-MM-DD, 3=DD.MM.YYYY
  uint8_t clockPosition;    // 0=Center, 1=Left, 2=Right
  int8_t clockOffset;       // Fine-tune clock position (-10 to +10)
  // Display settings
  uint8_t colonBlinkMode;   // 0=Always On, 1=Blink, 2=Always Off
  uint8_t colonBlinkRate;   // Tenths of Hz (10 = 1.0 Hz)
  uint8_t displayBrightness;   // Display brightness 0-255 (default: 255)

  // Scheduled dimming (night mode). Start/end are hour:minute of day.
  bool enableScheduledDimming;  // Enable time-based automatic dimming
  uint8_t dimStartHour;         // Hour to start dimming (0-23)
  uint8_t dimStartMinute;       // Minute to start dimming (0-59)
  uint8_t dimEndHour;           // Hour to end dimming (0-23)
  uint8_t dimEndMinute;         // Minute to end dimming (0-59)
  uint8_t dimBrightness;        // Brightness level during dim period (0-255)

  // Scheduled power-off (nightly full-dark window; spares the LEDs). Takes
  // priority over dimming. Home Assistant can also toggle live via
  // /api/display/on|off (runtime override, separate from this schedule).
  bool enableScheduledOff;      // Enable the nightly off window
  uint8_t offStartHour;         // Hour to power the panel off (0-23)
  uint8_t offStartMinute;       // Minute to power the panel off (0-59)
  uint8_t offEndHour;           // Hour to power it back on (0-23)
  uint8_t offEndMinute;         // Minute to power it back on (0-59)

  // Notification banner (POST /api/notify)
  bool notifyEnabled;           // Master toggle for the /api/notify endpoint
  uint8_t notifyPosition;       // Default banner position: 0=Bottom, 1=Top

  // Weather clock (style 14, Open-Meteo)
  bool weatherEnabled;          // Master toggle for the fetch task
  float weatherLat;             // Location latitude (0,0 = unset)
  float weatherLon;             // Location longitude
  uint8_t weatherLayout;       // 0=Current, 1=Rows, 2=Columns, 3=Hourly, 4=Tomorrow
  bool weatherUseFahrenheit;    // false = Celsius
  char weatherApiKey[33];       // Optional commercial API key ("" = free endpoint)
  char weatherPlace[48];        // Place name from the web UI search ("" = coordinates typed by hand)

  // Network settings
  char deviceName[32];          // Device name for mDNS and app (default: "pixelclock")
  bool showIPAtBoot;          // Show IP address on the panel at startup (default: true)
  bool useStaticIP;
  char staticIP[16];
  char gateway[16];
  char subnet[16];
  char dns1[16];
  char dns2[16];
  char ntpServer1[64];        // Primary NTP server (blank = compiled default)
  char ntpServer2[64];        // Secondary NTP server (blank = none)

  // TRON clock settings
  uint8_t tronBikeStyle;       // 0=Motorcycle profile, 1=Light cycle top view

  // Doom Fire clock settings. Heights are in fire rows (2 canvas px each).
  uint8_t doomFlameHeight;     // Digit plume reach above the digits (8-40)
  uint8_t doomGroundHeight;    // Ground fire reach (5-40)
  uint8_t doomWind;            // 0=Left (classic), 1=None, 2=Right
  bool doomShowDate;           // Show date row (off = centred clock)
  bool doomBurningDigits;      // Digits feed the fire and throw their own flames
  bool doomSmoothFire;         // Soften the flames off the digits (ground fire untouched)

  // Ambient screensaver (scheduled or forced, replaces the clock)
  bool ambientEnabled;         // Enable the scheduled window
  uint8_t ambientStyle;        // 0=Space Invaders, 1=Pac-Man maze, 3=Starfield, 4=Aquarium
  uint8_t ambientStartHour;    // Window start (0-23, wraps midnight)
  uint8_t ambientEndHour;      // Window end (0-23)
  bool ambientShowClock;       // Small HH:MM overlay in the corner

  // Mario clock settings
  uint8_t marioBounceHeight;  // Tenths (40 = 4.0)
  uint8_t marioBounceSpeed;   // Tenths (6 = 0.6)
  bool marioSmoothAnimation;  // Enable 4-frame walk cycle (default: false = 2-frame)
  uint8_t marioWalkSpeed;     // Tenths (20 = 2.0, 25 = 2.5 old/fast)
  bool marioIdleEncounters;   // Enable idle enemy encounters (default: false)
  uint8_t marioEncounterFreq; // 0=Rare(25-35s), 1=Normal(15-25s), 2=Frequent(8-15s)
  uint8_t marioEncounterSpeed; // 0=Slow, 1=Normal, 2=Fast (default: 1)
  bool marioScenery;          // Classic World 1-1 backdrop in the sky band

  // Space clock settings
  uint8_t spaceCharacterType;   // 0=Invader, 1=Ship
  uint8_t spacePatrolSpeed;     // Tenths (10 = 1.0)
  uint8_t spaceAttackSpeed;     // Tenths (25 = 2.5)
  uint8_t spaceLaserSpeed;      // Tenths (40 = 4.0)
  uint8_t spaceExplosionGravity; // Tenths (5 = 0.5)

  // Pong clock settings
  uint8_t pongBallSpeed;        // Fixed-point (16 = 1.0)
  uint8_t pongBounceStrength;   // Tenths (3 = 0.3)
  uint8_t pongBounceDamping;    // Hundredths (85 = 0.85)
  uint8_t pongPaddleWidth;      // Pixels (20)
  bool pongHorizontalBounce;    // Enable horizontal digit bounce on side hits
  bool pongDigitShatter;        // Shatter/reassemble on digit change (off = blink only)

  // Pac-Man clock settings
  uint8_t pacmanSpeed;          // Tenths (10 = 1.0)
  uint8_t pacmanEatingSpeed;    // Tenths (20 = 2.0)
  uint8_t pacmanMouthSpeed;     // Mouth animation speed (10 = 100ms)
  uint8_t pacmanPelletCount;    // Number of patrol pellets (0-20)
  bool pacmanPelletRandomSpacing;  // Random or even spacing
  bool pacmanBounceEnabled;     // Enable digit bounce on eat

  // Snake clock settings
  uint8_t snakeSpeed;           // Step pace, tenths (higher = faster)
  uint8_t snakeLength;          // Base body length in cells (4-12)
  bool snakeWallBorder;         // Draw Nokia-style arena frame
  bool snakeShowDate;           // Show date row (off = snake uses full screen)

  // Tetris clock settings
  uint8_t tetrisFallSpeed;      // Slab/dot drop accel, tenths (12 = 1.2)
  uint8_t tetrisBlockStyle;     // 0=LCD grid (gaps), 1=Solid blocks
  bool tetrisIdleTumble;        // Show occasional tumbling piece when idle
  uint8_t tetrisAnimStyle;      // 0=Drop-in slabs, 1=Falling dots build-up
  bool tetrisShowDate;          // Show the date row (off = cleaner screen)
  uint8_t tetrisDatePosition;   // 0=Top, 1=Bottom
  uint8_t tetrisDotSpeed;       // Falling-dot build speed, tenths (12 = 1.2)
  uint8_t tetrisDotOrder;       // 0=Bottom-up, 1=Random
  bool tetrisDigitBounce;       // Bounce the new digit after it rebuilds
  bool tetrisSmoothGame;        // Block Game plays near-perfectly (smart piece pick, avoids holes)
  bool tetrisSmallClock;        // Small corner clock; frees the panel for a taller block-game well (auto-enables Block game)
  uint8_t tetrisSmallClockPos;  // 0=Top-left, 1=Top-right

  // Asteroids clock settings
  uint8_t asteroidsShipSpeed;   // Ship thrust/drift scale, tenths (12 = 1.2)
  uint8_t asteroidsRockCount;   // Rocks kept in play (1-4)
  uint8_t asteroidsRockSpeed;   // Rock drift speed, tenths (8 = 0.8)
  bool asteroidsShowDate;       // Show date row (off = centred clock)
  bool asteroidsTransparent;    // No mask behind digits, ship flies through (default: true)

  // Dino Runner clock settings
  uint8_t dinoSpeed;            // World scroll speed, tenths (12 = 1.2)
  uint8_t dinoCactusFreq;       // 0=Rare, 1=Normal, 2=Frequent
  bool dinoShowClouds;          // Parallax clouds (default: true)
  bool dinoShowDate;            // Show date row (off = centred clock)

  // Matrix Rain clock settings
  uint8_t matrixRainSpeed;      // Rain fall speed, tenths (12 = 1.2)
  uint8_t matrixRainDensity;    // 0=Sparse, 1=Normal, 2=Dense
  bool matrixShowDate;          // Show date row (off = centred clock)
  bool matrixSmoothScroll;      // Pixel-by-pixel leading glyph; fixed-cell trail
  bool matrixSmallClock;        // Small top-right clock, with digit decoding
  bool matrixTransparent;       // No mask behind digits, rain falls through (default: false)

  // Missile Command clock settings (style retired; kept inert for NVS/export stability)
  uint8_t mcMissileSpeed;       // Enemy missile speed, tenths (12 = 1.2)
  uint8_t mcMissileFreq;        // Idle volley cadence: 0=Rare, 1=Normal, 2=Frequent
  bool mcShowDate;              // Show date row (off = centred clock)

  // ---- CYD hardware (no upstream equivalent) ----
  bool touchEnabled;            // Tap the screen to advance the clock style
  bool ldrAutoBrightness;       // Drive the backlight from the onboard LDR
  uint8_t ldrMinBrightness;     // Floor for auto-brightness (0-255)
  bool rgbLedEnabled;           // Mirror WiFi / notification state on the RGB LED

  // User-editable sprite colors (RGB565), indexed by ColorSlot.
  uint16_t spriteColors[COL_COUNT];
};
Settings settings=[]() {Settings v{};v.clockStyle=12;v.matrixRainSpeed=12;v.matrixRainDensity=1;v.matrixShowDate=true;v.use24Hour=true;v.weatherEnabled=true;v.weatherLat=-33;v.weatherLon=151;v.colonBlinkRate=10;return v;}();
#define SPRITE_COLOR(s) settings.spriteColors[s]
uint8_t cycleActiveStyle=255;
const int DIGIT_X[5]={DIGIT_X_0,DIGIT_X_1,DIGIT_X_2,DIGIT_X_3,DIGIT_X_4};
int displayed_hour=0, displayed_min=0, num_targets=0;
bool displayed_is_pm=false, time_overridden=false, ntpSynced=true, wifiConnected=true;
unsigned long time_override_start=0;
int target_x_positions[5], target_digit_index[5], target_digit_values[5];
struct String : std::string {
 using std::string::string;
 String(int v):std::string(std::to_string(v)) {}
 size_t write(uint8_t c) {push_back(c);return 1;}
 size_t write(const uint8_t* p,size_t n) {append((const char*)p,n);return n;}
 int toInt() const {return std::stoi(*this);}
 bool startsWith(const char* p) const {return rfind(p,0)==0;}
};
struct HTTPUpload {int status=0;String filename="firmware.bin";uint8_t bytes[4]{};uint8_t* buf=bytes;size_t currentSize=4;};
struct FakeServer {
 JsonDocument args,headers;int methodValue=HTTP_POST,reply=0,uploadReads=0;
 HTTPUpload part;std::function<void()> finish,uploadCallback;
 bool hasArg(const char* k) {return !args[k].isNull();}
 String arg(const char* k) {return String(args[k].as<int>());}
 String header(const char* k) {return String(headers[k] | "");}
 String hostHeader() {return String("clock.local");}
 int method() {return methodValue;}
 void sendHeader(const char*,const char*) {}
 void send(int status,const char*,const std::string&) {reply=status;}
 void on(const char*,int,std::function<void()> f,std::function<void()> u={}) {finish=f;uploadCallback=u;}
 void collectHeaders(const char**,int) {}
 HTTPUpload& upload() {++uploadReads;return part;}
} server;
struct FakeUpdate {
 int begins=0,aborts=0;bool failBegin=false,failWrite=false,failEnd=false;
 bool begin(int) {++begins;return !failBegin;}
 size_t write(uint8_t*,size_t n) {return failWrite?0:n;}
 bool end(bool) {return !failEnd;}
 void abort() {++aborts;}
 bool hasError() {return failBegin || failWrite || failEnd;}
 const char* errorString() {return "test error";}
 void printError(int) {}
} Update;
int Serial=0;
struct FakeESP {int restarts=0;void restart() {++restarts;}} ESP;
void delay(unsigned long n) {tick+=n;}
void esp_task_wdt_reset() {}
struct Preferences {
 JsonDocument stored;
 std::vector<uint8_t> blob;
 size_t getBytesLength(const char*) { return blob.size(); }
 size_t getBytes(const char*,void* out,size_t n) { n=std::min(n,blob.size());memcpy(out,blob.data(),n);return n; }
 void putBool(const char* k,bool v) {stored[k]=v;}
 void putUChar(const char* k,int v) {stored[k]=v;}
 bool getBool(const char* k,bool def) {return stored[k] | def;}
 int getUChar(const char* k,int def) {return stored[k] | def;}
} preferences;
struct CydDisplay {
 int scale=1, ax=0, ay=0, cx=0, cy=0, textSize=1;
 int minX=10000, minY=10000, maxX=-10000, maxY=-10000, calls=0, circleCalls=0;
 std::vector<std::string> text;
 std::vector<uint16_t> inks; uint16_t ink=DISPLAY_WHITE;
 void bounds(int x,int y,int w,int h) {
   x=ax+(x-ax)*scale; y=ay+(y-ay)*scale; w*=scale; h*=scale;
   minX=std::min(minX,x); minY=std::min(minY,y); maxX=std::max(maxX,x+w-1); maxY=std::max(maxY,y+h-1); ++calls;
 }
 void reset() { minX=minY=10000; maxX=maxY=-10000; calls=0; circleCalls=0; text.clear(); inks.clear(); }
 void drawPixel(int x,int y,uint16_t) { bounds(x,y,1,1); }
 void fillRect(int x,int y,int w,int h,uint16_t) { bounds(x,y,w,h); }
 void drawFastHLine(int x,int y,int w,uint16_t) { bounds(x,y,w,1); }
 void drawFastVLine(int x,int y,int h,uint16_t) { bounds(x,y,1,h); }
 void drawCircle(int x,int y,int r,uint16_t) { ++circleCalls; bounds(x-r,y-r,2*r+1,2*r+1); }
 void fillCircle(int x,int y,int r,uint16_t c) { drawCircle(x,y,r,c); }
 void drawLine(int x,int y,int xx,int yy,uint16_t) { bounds(std::min(x,xx),std::min(y,yy),abs(x-xx)+1,abs(y-yy)+1); }
 void setTextSize(int s) { textSize=s; }
 void setTextColor(uint16_t c) { ink=c; }
 void setCursor(int x,int y) { cx=x; cy=y; }
 int getCursorX() { return cx; }
 void print(const char* s) { text.emplace_back(s); inks.push_back(ink); bounds(cx,cy,strlen(s)*6*textSize,7*textSize); cx+=strlen(s)*6*textSize; }
 void print(char c) { char s[]={c,0}; print(s); }
 void setSpriteScale(int s,int x,int y) { scale=s;ax=x;ay=y; }
 void clearSpriteScale() { scale=1; }
} display;
struct SpriteScale {
 CydDisplay& d;
 SpriteScale(CydDisplay& v,int s,int x,int y):d(v) {d.setSpriteScale(s,x,y);}
 ~SpriteScale() {d.clearSpriteScale();}
};
uint16_t dateColor();
struct tm nowTime{};
bool haveTime=true;
bool getTimeWithTimeout(struct tm* t) {*t=nowTime;return haveTime;}
void drawNoWiFiIcon(int,int) {}
'''

WEATHER_PLATFORM = r'''
#include <functional>
#include <sstream>
using portMUX_TYPE=int;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(x) ((void)(x))
#define portEXIT_CRITICAL(x) ((void)(x))
#define pdPASS 1
#define WL_CONNECTED 3
#define HTTP_CODE_OK 200
struct FakeWifi {int state=WL_CONNECTED;int status() {return state;}} WiFi;
struct WiFiClient {};
struct WiFiClientSecure:WiFiClient {void setInsecure() {}int lastError(char* buf,size_t n) {if(n) buf[0]=0;return 0;}};
#define MALLOC_CAP_INTERNAL 1
#define MALLOC_CAP_8BIT 2
size_t heap_caps_get_free_size(int) {return 60000;}
size_t heap_caps_get_largest_free_block(int) {return 30000;}
std::string httpBody=R"({"current":{"temperature_2m":10,"weather_code":0,"is_day":1},"daily":{}})";
std::string lastUrl;
std::function<void()> duringGet;
int httpStatus=200;
struct HTTPClient {
 std::istringstream stream;
 void setTimeout(int) {}void useHTTP10(bool) {}
 bool begin(WiFiClient&,const char* url) {lastUrl=url;return true;}
 int GET() {if(duringGet) {auto f=duringGet;duringGet=nullptr;f();}return httpStatus;}
 std::istream& getStream() {stream.clear();stream.str(httpBody);return stream;}
 void end() {}
};
void (*pendingTask)(void*)=nullptr;
bool taskFail=false;int taskStarts=0,taskDeletes=0;
int xTaskCreatePinnedToCore(void (*fn)(void*),const char*,int,void*,int,void*,int) {
 if(taskFail) return 0;pendingTask=fn;++taskStarts;return pdPASS;
}
void vTaskDelete(void*) {++taskDeletes;}
void netMarkOutboundOk() {}
void runPendingTask() {assert(pendingTask);auto fn=pendingTask;pendingTask=nullptr;fn(nullptr);}
'''

TESTS = r'''
void importMatrix(JsonDocument& doc) {
IMPORTS
}
void saveMatrix() {
SAVES
}
void loadMatrix() {
LOADS
}
String exportMatrix() {
 JsonDocument doc;writeConfigExport(doc,settings);String json;serializeJson(doc,json);return json;
}
void loadPalette() {
PALETTE_LOAD
}
void importColours(JsonDocument& doc, const char* colorSchema) {
COLOURS
}
void importTimePresentation(JsonDocument& doc) {
TIME_IMPORT
}
void saveTimePresentation() {
TIME_SAVE
}
void loadTimePresentation() {
TIME_LOAD
}
void saveTimeForm() {
TIME_FORM
}
String exportTimePresentation() {
 return exportMatrix();
}
void importWeatherLayout(JsonDocument& doc) {
LAYOUT_IMPORT
}
void saveWeatherLayout() {
LAYOUT_SAVE
}
void loadWeatherLayout() {
LAYOUT_LOAD
}
void setupOtaTestRoute() {
OTA_ROUTE
}
int mutations=0;
void testMutation() {++mutations;}
bool weatherLayoutPlaceholder(const char* n,String& out) {
LAYOUT_PLACEHOLDER
 return false;
}
int main() {
 // Run the real route wrappers and OTA callbacks with a simulated Update engine.
 mutationRoute("/test",testMutation);server.methodValue=2;server.finish();assert(server.reply==405 && mutations==0);
 server.methodValue=HTTP_POST;server.headers["Origin"]="http://evil.test";server.finish();assert(server.reply==403 && mutations==0);
 server.headers.clear();server.finish();assert(mutations==1);
 setupOtaTestRoute();server.reply=0;server.uploadCallback();assert(server.uploadReads==0);server.finish();assert(server.reply==400 && ESP.restarts==0);
 server.headers["Content-Type"]="multipart/form-data";server.uploadCallback();assert(server.uploadReads==0);server.finish();assert(server.reply==400);
 assert(!multipartUploadContentType("multipart/form-data; boundary=\"\""));
 server.headers["Content-Type"]="multipart/form-data; boundary=test";server.headers["Origin"]="http://evil.test";
 server.part.status=UPLOAD_FILE_START;server.uploadCallback();server.finish();assert(server.reply==403 && Update.begins==0);
 server.headers.remove("Origin");server.part.status=UPLOAD_FILE_START;server.uploadCallback();server.part.status=UPLOAD_FILE_WRITE;server.uploadCallback();
 server.part.status=UPLOAD_FILE_ABORTED;server.uploadCallback();server.finish();assert(server.reply==400 && ESP.restarts==0);
 server.part.status=UPLOAD_FILE_START;server.uploadCallback();server.part.status=UPLOAD_FILE_WRITE;server.uploadCallback();server.part.status=UPLOAD_FILE_END;server.uploadCallback();
 server.finish();assert(server.reply==200 && ESP.restarts==1);
 server.finish();assert(server.reply==400 && ESP.restarts==1); // no success leaking to empty next request
 Update.failWrite=true;server.part.status=UPLOAD_FILE_START;server.uploadCallback();server.part.status=UPLOAD_FILE_WRITE;server.uploadCallback();
 server.part.status=UPLOAD_FILE_END;server.uploadCallback();server.finish();assert(server.reply==500 && ESP.restarts==1);
 Update.failWrite=false;server.headers.clear();
 // Actual weather scheduler/fetch code under deterministic task/HTTP stand-ins.
 settings.clockStyle=14;weatherSettingsChanged();weatherLoop();assert(taskStarts==1 && fetchBusy);
 weatherLoop();assert(taskStarts==1);runPendingTask();assert(!fetchBusy && taskDeletes==1 && getWeather().valid);
 assert(lastUrl.rfind("https://api.open-meteo.com/",0)==0);  // HTTPS unless a board sets WEATHER_PLAIN_HTTP
 assert(lastUrl.find("forecast_days=4")!=std::string::npos && lastUrl.find("forecast_hours=12")!=std::string::npos && lastUrl.find("timezone=auto")!=std::string::npos);
 weatherLoop();assert(taskStarts==1);tick+=10UL*60UL*1000UL;weatherLoop();assert(taskStarts==2);runPendingTask();
 // A settings change mid-request invalidates cached weather and rejects its response.
 weatherSettingsChanged();weatherLoop();duringGet=[](){settings.weatherLat=51;weatherSettingsChanged();};
 runPendingTask();assert(!getWeather().valid);weatherLoop();runPendingTask();assert(getWeather().valid);
 assert(lastUrl.find("latitude=51.0000")!=std::string::npos);
 taskFail=true;weatherSettingsChanged();int before=taskStarts;weatherLoop();assert(!fetchBusy && taskStarts==before);
 taskFail=false;weatherLoop();assert(taskStarts==before);tick+=60000;weatherLoop();assert(fetchBusy);runPendingTask();
 // Disabled/ineligible screens don't create a task; headerless failure retries later.
 settings.clockStyle=12;weatherSettingsChanged();before=taskStarts;weatherLoop();assert(taskStarts==before);
 settings.clockStyle=14;weatherSettingsChanged();httpStatus=500;weatherLoop();runPendingTask();assert(!getWeather().valid);
 httpStatus=200;before=taskStarts;weatherLoop();assert(taskStarts==before);tick+=60000;weatherLoop();runPendingTask();
 assert(getWeather().valid && taskStarts==taskDeletes);
 settings.clockStyle=12;
 // Browser request policy preserves scripts, but rejects foreign/opaque origins.
 assert(!requestFromOtherSite("","","clock.local"));
 assert(!requestFromOtherSite("http://clock.local","same-origin","clock.local"));
 assert(!requestFromOtherSite("https://CLOCK.local:80","same-origin","clock.local:80"));
 assert(requestFromOtherSite("http://evil.test","cross-site","clock.local"));
 assert(requestFromOtherSite("http://clock.local","cross-site","clock.local"));
 assert(requestFromOtherSite("null","","clock.local"));
 assert(requestFromOtherSite("http://clock.local.evil.test","same-site","clock.local"));
 assert(requestFromOtherSite("http://user@clock.local","","clock.local"));
 OtaUploadState upload;assert(!upload.canRestart());upload.begin();assert(!upload.canRestart());
 upload.complete=true;assert(upload.canRestart());upload.clear();assert(!upload.canRestart());
 upload.begin();upload.failed=true;upload.complete=true;assert(!upload.canRestart());upload.clear();assert(!upload.canRestart());
 // Decode partial data, midnight-crossing hours, missing forecast and invalid data.
 JsonDocument feed;
 deserializeJson(feed,R"({"current":{"temperature_2m":-40,"weather_code":95,"is_day":0},"daily":{"time":["2026-10-12","2026-10-13","2026-10-14","2026-10-15"],"temperature_2m_max":[0,1,2,3],"temperature_2m_min":[-4,-3,-2,-1],"precipitation_probability_max":[50,0,100,101]},"hourly":{"time":["2026-10-12T23:00"]}})");
 auto temps=feed["hourly"]["temperature_2m"].to<JsonArray>();auto rain=feed["hourly"]["precipitation_probability"].to<JsonArray>();
 for(int i=0;i<WEATHER_FORECAST_HOURS;i++) {temps.add(-40+i);rain.add(i%2?100:0);}
 WeatherData parsed;assert(decodeWeather(feed,parsed));assert(!parsed.isDay && parsed.hourStart==23);
 assert(parsed.days[0].wday==2 && parsed.days[2].precipChance==-1);
 feed["hourly"]["temperature_2m"][11]=nullptr;assert(decodeWeather(feed,parsed) && parsed.hourStart==-1);
 feed["current"]["temperature_2m"]=1000000;assert(!decodeWeather(feed,parsed));
 feed.remove("daily");assert(!decodeWeather(feed,parsed));
 assert(weatherWeekday("2024-02-29")==4 && weatherWeekday("2026-02-29")==-1 && weatherWeekday("2026-04-31")==-1);
 JsonDocument layoutDoc;deserializeJson(layoutDoc,R"({"weatherLayout":4})");importWeatherLayout(layoutDoc);assert(settings.weatherLayout==4);
 saveWeatherLayout();settings.weatherLayout=0;loadWeatherLayout();assert(settings.weatherLayout==4);
 deserializeJson(layoutDoc,R"({"weatherLayout":5})");importWeatherLayout(layoutDoc);assert(settings.weatherLayout==4);
 String selected;assert(weatherLayoutPlaceholder("SEL_WEATHERLAYOUT_4",selected) && selected=="selected");
 assert(weatherLayoutPlaceholder("SEL_WEATHERLAYOUT_0",selected) && selected.empty());
 assert(!weatherLayoutPlaceholder("SEL_WEATHERLAYOUT_9",selected));
 settings.weatherLayout=0;
 // Real defaults overlay an old short NVS palette without shifting slots.
 std::vector<uint16_t> old(COL_DATE);
 for(int i=0;i<COL_DATE;i++) old[i]=200+i;
 preferences.blob.resize(old.size()*sizeof(uint16_t));
 memcpy(preferences.blob.data(),old.data(),preferences.blob.size());loadPalette();
 for(int i=0;i<COL_DATE;i++) assert(settings.spriteColors[i]==old[i]);
 assert(settings.spriteColors[COL_DATE]==DISPLAY_WHITE && settings.spriteColors[COL_WEATHER_DETAIL]==DISPLAY_WHITE);
 assert(supportedColorSchema(nullptr) && supportedColorSchema("cyd-v1"));
 assert(!supportedColorSchema("unknown"));
 for(int i=0;i<COL_COUNT;i++) assert(paletteImportSlot(i,nullptr)==i);
 assert(paletteImportSlot(-1,nullptr)==-1 && paletteImportSlot(COL_COUNT,nullptr)==-1);
 const int upTail=COL_SCOPE_PEAK+1;
 assert(paletteImportSlot(upTail,"upstream-25b1928")==COL_DOOM_EMBER);
 assert(paletteImportSlot(upTail+3,"upstream-25b1928")==COL_DIGITS_S17);
 assert(paletteImportSlot(upTail+4,"upstream-25b1928")==-1);
 assert(paletteImportSlot(upTail+5,"upstream-25b1928")==-1);
 assert(paletteImportSlot(upTail+6,"upstream-25b1928")==COL_DATE);
 assert(paletteImportSlot(upTail+7,"upstream-25b1928")==COL_WEATHER_DETAIL);
 JsonDocument palette;
 auto array=palette["spriteColors"].to<JsonArray>();
 for(int i=0;i<upTail+8;i++) array.add("#ff0000");
 auto marioHair=settings.spriteColors[COL_MARIO_HAIR];
 importColours(palette,"upstream-25b1928");
 assert(settings.spriteColors[COL_MARIO_HAIR]==marioHair);
 assert(settings.spriteColors[COL_DOOM_EMBER]==0xF800 && settings.spriteColors[COL_DATE]==0xF800);
 for(int i=0;i<COL_COUNT;i++) settings.spriteColors[i]=100+i;
 settings.spriteColors[COL_DATE]=0x07E0;settings.spriteColors[COL_WEATHER_DETAIL]=0xF800;
 nowTime.tm_year=126;nowTime.tm_mon=9;nowTime.tm_mday=12;nowTime.tm_wday=1;
 const char* dates[]={"12/10/2026","10/12/2026","2026-10-12","12.10.2026"};
 for(int format=0;format<4;format++) {
   settings.dateFormat=format;char date[12];formatDate(date,sizeof(date),&nowTime);
   assert(!strcmp(date,dates[format]));
   char tiny[4];formatDate(tiny,sizeof(tiny),&nowTime);assert(tiny[3]==0);
   display.reset();drawDate(2,4,&nowTime);
   assert(display.text.back()==dates[format] && display.inks.back()==dateColor() && display.ink==DISPLAY_WHITE);
 }
 settings.dateFormat=255;char date[12];formatDate(date,sizeof(date),&nowTime);assert(!strcmp(date,dates[0]));
 settings.dateFormat=0;
 for(bool hour24: {false,true}) for(bool hide: {false,true}) {
   settings.use24Hour=hour24;settings.hideAmPm=hide;
   display.reset();drawMeridiemIndicator(2,3,true);
   assert(display.text.empty()==(hour24 || hide));
   if(!display.text.empty()) assert(display.inks.back()==dateColor() && display.ink==DISPLAY_WHITE);
   for(auto render: {displayStandardClock,displayLargeClock}) {
     display.reset();render();
     for(size_t i=0;i<display.text.size();i++) {
       if(display.text[i]=="AM" || display.text[i]=="PM") assert(!hour24 && !hide && display.inks[i]==dateColor());
       if(display.text[i]==dates[0] || display.text[i]=="Monday") assert(display.inks[i]==dateColor());
     }
   }
 }
 display.reset();doomOutlineText(20,4,"AM");assert(display.inks.back()==dateColor() && display.ink==DISPLAY_WHITE);
 for(int format: {0,1,2}) {
   server.args["use24Hour"]=format;saveTimeForm();
   assert(settings.use24Hour==(format==1) && settings.hideAmPm==(format==2));
 }
 server.args["use24Hour"]=3;saveTimeForm();assert(!settings.use24Hour && settings.hideAmPm);
 JsonDocument presentation;assert(!deserializeJson(presentation,exportTimePresentation()));assert(presentation["hideAmPm"].as<bool>());
 deserializeJson(presentation,R"({"hideAmPm":true})");importTimePresentation(presentation);
 assert(settings.hideAmPm);saveTimePresentation();settings.hideAmPm=false;loadTimePresentation();assert(settings.hideAmPm);
 deserializeJson(presentation,R"({"hideAmPm":"false"})");importTimePresentation(presentation);assert(settings.hideAmPm);
 deserializeJson(presentation,"{}");importTimePresentation(presentation);assert(settings.hideAmPm);
 settings.hideAmPm=false;settings.use24Hour=true;

 // Actual digitColor() must resolve each active rotation style, including the
 // non-contiguous late slots. The sentinel remains safe before the first frame.
 for(int style: {0,1,2,3,5,6,7,8,10,11,12,14,15,16,17}) {
   settings.clockStyle=style; auto expected=digitColor();
   settings.clockStyle=9; cycleActiveStyle=style; assert(digitColor()==expected);
 }
 cycleActiveStyle=255; assert(digitColor()==settings.spriteColors[COL_DIGITS_S9]);
 settings.clockStyle=12;
 JsonDocument doc;
 assert(!deserializeJson(doc,R"({"matrixRainSpeed":30,"matrixRainDensity":2,"matrixShowDate":false,"matrixTransparent":true,"matrixSmoothScroll":true,"matrixSmallClock":true})"));
 importMatrix(doc);
 assert(settings.matrixRainSpeed==30 && settings.matrixRainDensity==2);
 assert(!settings.matrixShowDate && settings.matrixTransparent && settings.matrixSmoothScroll && settings.matrixSmallClock);
 strcpy(settings.weatherPlace,"Quote \" slash \\ newline\n");strcpy(settings.ntpServer1,"pool\".example");
 auto backup=exportMatrix();JsonDocument escaped;assert(!deserializeJson(escaped,backup));
 assert(!strcmp(escaped["weatherPlace"].as<const char*>(),settings.weatherPlace));
 assert(!strcmp(escaped["ntpServer1"].as<const char*>(),settings.ntpServer1));
 saveMatrix();
 settings.matrixRainSpeed=5;settings.matrixRainDensity=0;
 settings.matrixShowDate=true;settings.matrixTransparent=false;
 settings.matrixSmoothScroll=false;settings.matrixSmallClock=false;
 loadMatrix();
 assert(settings.matrixRainSpeed==30 && settings.matrixRainDensity==2);
 assert(!settings.matrixShowDate && settings.matrixTransparent && settings.matrixSmoothScroll && settings.matrixSmallClock);
 settings.matrixRainSpeed=5;settings.matrixSmoothScroll=false;
 assert(!deserializeJson(doc,backup));importMatrix(doc);
 assert(settings.matrixRainSpeed==30 && settings.matrixSmoothScroll);
 deserializeJson(doc,R"({"matrixRainSpeed":31,"matrixRainDensity":-1,"matrixShowDate":"false","matrixSmallClock":2})");
 importMatrix(doc);
 assert(settings.matrixRainSpeed==30 && settings.matrixRainDensity==2 && !settings.matrixShowDate && settings.matrixSmallClock);
 deserializeJson(doc,"{}"); importMatrix(doc); assert(settings.matrixSmoothScroll);
 // Matrix's custom masked corner AM/PM must obey the shared visibility option.
 nowTime.tm_hour=13;nowTime.tm_min=20;nowTime.tm_sec=10;
 settings.use24Hour=false;settings.matrixSmallClock=true;settings.matrixShowDate=true;
 for(bool hide: {false,true}) {
   settings.hideAmPm=hide;time_overridden=false;syncDisplayedTime(&nowTime);resetMatrixRainAnimation();
   display.reset();displayClockWithMatrixRain();
   bool meridiem=false;
   for(size_t i=0;i<display.text.size();i++) {
     if(display.text[i]=="PM") {meridiem=true;assert(display.inks[i]==dateColor());}
     if(display.text[i]==dates[0]) assert(display.inks[i]==dateColor());
   }
   assert(meridiem==!hide);
 }
 settings.hideAmPm=false;settings.use24Hour=true;
 // Actual rain engine under both geometries/modes, then decode across midnight
 // and noon. Host ASan/UBSan also exercise every glyph and rain cell access.
 nowTime.tm_year=126;nowTime.tm_mon=9;nowTime.tm_mday=12;
 for(bool smooth: {false,true}) for(bool small: {false,true}) {
   settings.matrixSmoothScroll=smooth;settings.matrixSmallClock=small;
   assert(mxDigitX(0)>=0 && mxDigitX(4)+mxDigitW()<SCREEN_WIDTH);
   assert(mxTimeY()>=0 && mxTimeY()+mxDigitH()<SCREEN_HEIGHT);
   if(small) assert(2+60<mxDigitX(0)-1); // date and clock plates do not overlap
   for(int sourceHour: {11,23}) for(bool hour24: {false,true}) {
     settings.use24Hour=hour24;
     nowTime.tm_hour=sourceHour;nowTime.tm_min=59;nowTime.tm_sec=55;
     time_overridden=false;syncDisplayedTime(&nowTime);resetMatrixRainAnimation();
     for(int frame=0;frame<380;frame++) {
       tick+=16;int seconds=55+frame*16/1000;
       nowTime.tm_sec=seconds%60;nowTime.tm_min=seconds>=60?0:59;
       nowTime.tm_hour=seconds>=60?(sourceHour+1)%24:sourceHour;
       display.reset(); displayClockWithMatrixRain();assert(display.scale==1);
     }
     assert(!mxDecodeActive());assert(displayed_min==0);
     assert(displayed_hour==(hour24?(sourceHour+1)%24:12));
     assert(displayed_is_pm==(sourceHour==11));
     assert(!time_overridden);
   }
 }
 // Exercise every condition at multiple timestamps. Weather's scaled drawing
 // must remain clear of the temperature and details, and restore the transform.
 for(int code: {0,1,2,3,45,48,51,53,55,61,63,65,71,73,75,80,82,85,86,95,99}) {
   for(bool night: {false,true}) for(bool wind: {false,true}) {
     for(unsigned long ms=1000;ms<12000;ms+=83) {
       IconScene scene{code,night,wind,ms/1000.0f,ms};
       display.reset();drawWeatherIcon(WICON_X,WICON_Y,weatherIconFromCode(code),scene);
       assert(display.calls>0 && display.scale==1);
       assert(display.minX>=0 && display.maxX<WTEMP_X);
       assert(display.minY>=WTIME_Y+WTIME_H && display.maxY<WDETAIL_Y);
     }
   }
 }
 for(bool day: {false,true}) for(bool time: {false,true}) for(bool fahrenheit: {false,true}) {
   wx.isDay=day;wx.weatherCode=0;wx.tempC=-40;wx.windKmh=35;haveTime=time;
   settings.weatherUseFahrenheit=fahrenheit;
   display.reset();displayClockWithWeather();assert(display.scale==1);
   assert(display.maxX<SCREEN_WIDTH && display.maxY<SCREEN_HEIGHT);
   assert(display.inks.back()==settings.spriteColors[COL_WEATHER_DETAIL] && display.ink==DISPLAY_WHITE);
   assert(display.circleCalls==(day?2:3)); // unit + sun OR unit + moon/cutout
 }
 // All forecast layouts fit both canvases with negative/three-digit units,
 // missing days/hourly data, 12/24-hour format and AM/PM visibility combinations.
 haveTime=true;wx.valid=true;wx.isDay=true;wx.tempC=-150;wx.humidity=80;
 for(int i=0;i<3;i++) {wx.days[i].wday=(2+i)%7;wx.days[i].weatherCode=95;wx.days[i].tempMaxC=50;wx.days[i].tempMinC=-80;wx.days[i].precipChance=100;}
 for(int i=0;i<12;i++) {wx.hourTempC[i]=20;wx.hourPrecip[i]=100;}
 for(int layout=1;layout<=4;layout++) for(bool fahrenheit: {false,true}) for(bool missing: {false,true}) {
   settings.weatherLayout=layout;settings.weatherUseFahrenheit=fahrenheit;wx.hourStart=missing?-1:23;
   wx.days[0].wday=missing?-1:2;
   display.reset();displayClockWithWeather();assert(display.scale==1);
   assert(display.minX>=0 && display.maxX<SCREEN_WIDTH && display.maxY<SCREEN_HEIGHT);
 }
 settings.weatherLayout=0;
 wx.valid=false;display.reset();displayClockWithWeather();
 assert(display.text.back()=="Fetching weather...");
 settings.weatherEnabled=false;display.reset();displayClockWithWeather();
 assert(display.text.back()=="Enable it in the web UI");
 printf("PASS %dx%d: rotation colours, validated Matrix import, rain/decode rollovers, weather variants/bounds, date colours, hidden AM/PM, palette migration, forecasts/parser/export/request policy\n",SCREEN_WIDTH,SCREEN_HEIGHT);
}
'''.replace('IMPORTS', imports).replace('EXPORTS', exports).replace('SAVES', saves).replace('LOADS', loads).replace('PALETTE_LOAD', palette_load).replace('COLOURS', colours).replace('TIME_IMPORT', time_import).replace('TIME_SAVE', time_save).replace('TIME_LOAD', time_load).replace('TIME_FORM', time_form).replace('TIME_EXPORT', time_export).replace('OTA_ROUTE', ota_route).replace('LAYOUT_IMPORT', ' if (doc["weatherLayout"].is<int>()) {\n   int layout=doc["weatherLayout"].as<int>();\n   if (layout>=0 && layout<=4) settings.weatherLayout=layout;\n }\n').replace('LAYOUT_LOAD', '  settings.weatherLayout = preferences.getUChar("weatherLayout", 0);\n  if (settings.weatherLayout > 4) settings.weatherLayout = 0;\n').replace('LAYOUT_PLACEHOLDER', layout_placeholder).replace('LAYOUT_SAVE', '  preferences.putUChar("weatherLayout", settings.weatherLayout);')

with tempfile.TemporaryDirectory(prefix='cyd-clock-tests-') as folder:
    temp=Path(folder)
    (temp/'Arduino.h').write_text('#pragma once\n#include <cstdint>\n')
    for width,height in [(160,120),(240,160)]:
        layout=read('src/clocks/clock_layout.h').replace('#include "config.h"','').replace('#pragma once','')
        weather_header=read('src/weather/weather.h')
        harness=(f'#define SCREEN_WIDTH {width}\n#define SCREEN_HEIGHT {height}\n'
                 +layout+'\n'+read('src/config/color_slots.h')+'\n'+STUBS+'\n'+palette_defaults+'\n'+read('src/config/palette_import.h').replace('#pragma once','').replace('#include "color_slots.h"','')+'\n'
                 +renderer('src/config/settings_export.h').replace('#pragma once','')+'\n'+renderer('src/web/request_policy.h').replace('#pragma once','')+'\n'+request_helpers+'\n'+colour+time_helpers+targets+standard+doom_outline+'\n'+weather_header+'\n'+renderer('src/weather/weather_parse.h').replace('#pragma once','')+'\n'
                 +WEATHER_PLATFORM+'\n'+read('src/clocks/cycle_config.h').replace('#pragma once','')+'\n'+renderer('src/weather/weather.cpp')+'\nWeatherData& wx=published;\n'+read('src/clocks/matrix_glyphs.h')+'\n'
                 +renderer('src/clocks/clock_matrix.cpp')+'\n')
        harness+='\n'+renderer('src/clocks/clock_weather.cpp')+'\n'+TESTS
        source=temp/f'clock_{width}.cpp';source.write_text(harness)
        binary=temp/f'clock_{width}'
        subprocess.run([os.environ.get('CXX','c++'),'-std=c++17','-fsanitize=address,undefined','-g',
                        '-I',str(temp),'-I',str(JSON),str(source),'-o',str(binary)],check=True)
        subprocess.run([str(binary)],check=True)
