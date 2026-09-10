/*
 * CYD_AnimatedPixelClock - Weather Clock (style 14)
 *
 * Time on top, animated condition icon + big temperature in the middle,
 * details row (min/max + humidity alternating with sunrise/sunset) at the
 * bottom. Icon animation phases run off millis() so there is no state to
 * reset between style switches.
 */

#include "clocks.h"
#include "clock_globals.h"
#include "../display/display.h"
#include "../weather/weather.h"

// Layout. This style composes four bands - time, condition icon, temperature
// and an alternating details line - rather than reusing the shared digit row,
// so it derives its own metrics from the canvas.
#define WTIME_SIZE (DIGIT_TEXT_SIZE - 1)   // secondary clock: a step down from the hero row
#define WTIME_W (5 * TEXT1_W * WTIME_SIZE)
#define WTIME_H (TEXT1_H * WTIME_SIZE)
#define WTIME_Y 4
#define WICON_SIZE 24                      // icon art is fixed-size pixel work
#define WICON_X (SCREEN_WIDTH / 16)
#define WICON_Y (WTIME_Y + WTIME_H + 10)
#define WTEMP_X (WICON_X + WICON_SIZE + 8)
#define WDETAIL_Y (SCREEN_HEIGHT - TEXT1_H - 4)
#define WDETAIL_SWAP_MS 5000

static void drawCloudShape(int cx, int cy, uint16_t color) {
  // Puffy cloud built from three discs on a base bar, ~20px wide.
  display.fillCircle(cx - 6, cy + 2, 4, color);
  display.fillCircle(cx, cy - 1, 5, color);
  display.fillCircle(cx + 6, cy + 2, 4, color);
  display.fillRect(cx - 6, cy + 2, 13, 4, color);
}

static void drawWeatherIcon(int x, int y, WeatherIconKind kind) {
  uint16_t body = SPRITE_COLOR(COL_WEATHER_ICON);
  uint16_t accent = SPRITE_COLOR(COL_WEATHER_ACCENT);
  int cx = x + WICON_SIZE / 2;
  int cy = y + WICON_SIZE / 2;
  unsigned long now = millis();

  switch (kind) {
    case WICON_SUN: {
      display.fillCircle(cx, cy, 6, body);
      // Eight rays; every other ray pulses longer on a slow beat.
      int pulse = (now / 400) % 2;
      for (int i = 0; i < 8; i++) {
        float a = i * (PI / 4.0f);
        int len = 10 + ((i % 2 == pulse) ? 1 : -1);
        int x0 = cx + (int)(cosf(a) * 8);
        int y0 = cy + (int)(sinf(a) * 8);
        int x1 = cx + (int)(cosf(a) * len);
        int y1 = cy + (int)(sinf(a) * len);
        display.drawLine(x0, y0, x1, y1, body);
      }
      break;
    }
    case WICON_PARTCLOUD: {
      display.fillCircle(cx - 4, cy - 4, 5, body);
      for (int i = 0; i < 4; i++) {
        float a = i * (PI / 2.0f) - PI / 4.0f;
        display.drawLine(cx - 4 + (int)(cosf(a) * 6), cy - 4 + (int)(sinf(a) * 6),
                         cx - 4 + (int)(cosf(a) * 9), cy - 4 + (int)(sinf(a) * 9),
                         body);
      }
      drawCloudShape(cx + 3, cy + 4, accent);
      break;
    }
    case WICON_CLOUD: {
      // Slow 1px horizontal bob.
      int bob = ((now / 700) % 2) ? 1 : 0;
      drawCloudShape(cx + bob, cy - 1, body);
      break;
    }
    case WICON_FOG: {
      // Four haze lines drifting in alternating directions.
      for (int i = 0; i < 4; i++) {
        int yy = y + 5 + i * 5;
        int shift = (int)((now / 150 + i * 4) % 8) - 4;
        if (i % 2) shift = -shift;
        display.drawFastHLine(x + 3 + shift, yy, 16, i % 2 ? accent : body);
      }
      break;
    }
    case WICON_RAIN: {
      drawCloudShape(cx, cy - 5, body);
      // Three drop columns falling below the cloud.
      for (int i = 0; i < 3; i++) {
        int dropY = (int)((now / 60 + i * 5) % 12);
        display.drawFastVLine(cx - 6 + i * 6, cy + 2 + dropY, 3, accent);
      }
      break;
    }
    case WICON_SNOW: {
      drawCloudShape(cx, cy - 5, body);
      // Drifting flakes with a slight side sway.
      for (int i = 0; i < 3; i++) {
        int fy = (int)((now / 120 + i * 6) % 12);
        int fx = cx - 6 + i * 6 + (((now / 240 + i) % 2) ? 1 : -1);
        display.drawPixel(fx, cy + 2 + fy, accent);
        display.drawPixel(fx, cy + 3 + fy, accent);
      }
      break;
    }
    case WICON_STORM: {
      drawCloudShape(cx, cy - 5, body);
      for (int i = 0; i < 2; i++) {
        int dropY = (int)((now / 60 + i * 7) % 10);
        display.drawFastVLine(cx - 7 + i * 13, cy + 2 + dropY, 3, accent);
      }
      // Lightning bolt flashes ~300ms out of every 1.6s.
      if ((now % 1600) < 300) {
        display.drawLine(cx + 1, cy + 1, cx - 2, cy + 6, DISPLAY_WHITE);
        display.drawLine(cx - 2, cy + 6, cx + 2, cy + 6, DISPLAY_WHITE);
        display.drawLine(cx + 2, cy + 6, cx - 1, cy + 12, DISPLAY_WHITE);
      }
      break;
    }
  }
}

// Big temperature: size-2 digits plus a small degree circle and unit letter.
static void drawTemperature(int x, int y, float tempC) {
  float t = settings.weatherUseFahrenheit ? tempC * 9.0f / 5.0f + 32.0f : tempC;
  char buf[8];
  snprintf(buf, sizeof(buf), "%d", (int)roundf(t));

  display.setTextSize(DIGIT_TEXT_SIZE);
  display.setTextColor(SPRITE_COLOR(COL_WEATHER_TEMP));
  display.setCursor(x, y);
  display.print(buf);

  int endX = x + strlen(buf) * 18;
  display.drawCircle(endX + 2, y + 1, 2, SPRITE_COLOR(COL_WEATHER_TEMP));
  display.setTextSize(1);
  display.setCursor(endX + 7, y);
  display.print(settings.weatherUseFahrenheit ? "F" : "C");
  display.setTextColor(DISPLAY_WHITE);
}

void displayClockWithWeather() {
  struct tm timeinfo;
  bool haveTime = getTimeWithTimeout(&timeinfo);

  // --- Time row (size 2, centered) ---
  if (haveTime) {
    int displayHour, displayMin;
    bool isPM;
    formatTimeForDisplay(timeinfo.tm_hour, timeinfo.tm_min, displayHour,
                         displayMin, isPM);
    char timeStr[9];
    char separator = shouldShowColon() ? ':' : ' ';
    sprintf(timeStr, "%02d%c%02d", displayHour, separator, displayMin);

    display.setTextSize(WTIME_SIZE);
    display.setCursor((SCREEN_WIDTH - WTIME_W) / 2, WTIME_Y);
    display.setTextColor(digitColor());
    display.print(timeStr);
    display.setTextColor(DISPLAY_WHITE);
    drawMeridiemIndicator(SCREEN_WIDTH - 2 * TEXT1_W - 2, WTIME_Y + 4, isPM);
  } else {
    display.setTextSize(1);
    const char *msg = ntpSynced ? "Time Error" : "Syncing time...";
    display.setCursor(centerText1(strlen(msg)), WTIME_Y + 4);
    display.print(msg);
  }

  // --- Weather block ---
  WeatherData wx = getWeather();
  display.setTextSize(1);

  if (!settings.weatherEnabled || !weatherConfigured()) {
    const char *l1 = "Weather not set up";
    const char *l2 = "Enable it in the web UI";
    display.setCursor(centerText1(strlen(l1)), SCREEN_CENTER_Y - TEXT1_H);
    display.print(l1);
    display.setCursor(centerText1(strlen(l2)), SCREEN_CENTER_Y + 4);
    display.print(l2);
    return;
  }
  if (!wx.valid) {
    const char *msg = "Fetching weather...";
    display.setCursor(centerText1(strlen(msg)), SCREEN_CENTER_Y - TEXT1_H / 2);
    display.print(msg);
    return;
  }

  drawWeatherIcon(WICON_X, WICON_Y, weatherIconFromCode(wx.weatherCode));
  drawTemperature(WTEMP_X, WICON_Y + 3, wx.tempC);

  // --- Details row: min/max + humidity alternating with sun times ---
  char line[30];
  if ((millis() / WDETAIL_SWAP_MS) % 2 == 0) {
    float lo = wx.tempMinC, hi = wx.tempMaxC;
    if (settings.weatherUseFahrenheit) {
      lo = lo * 9.0f / 5.0f + 32.0f;
      hi = hi * 9.0f / 5.0f + 32.0f;
    }
    snprintf(line, sizeof(line), "%d\x18 %d\x19  %d%% RH", (int)roundf(hi),
             (int)roundf(lo), wx.humidity);
  } else {
    snprintf(line, sizeof(line), "\x18%s  \x19%s", wx.sunrise, wx.sunset);
  }
  display.setCursor(centerText1(strlen(line)), WDETAIL_Y);
  display.print(line);

  if (!wifiConnected) {
    drawNoWiFiIcon(0, 0);
  }
}
