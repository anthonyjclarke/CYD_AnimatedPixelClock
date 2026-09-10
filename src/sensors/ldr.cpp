/*
 * CYD_AnimatedPixelClock - Ambient light sensor (LDR)
 *
 * See ldr.h. Everything here is compiled out when the board has no LDR.
 */

#include "ldr.h"

#include "../config/globals.h"
#include "../config/settings.h"
#include "debug.h"

#if HAS_LDR

namespace {

uint16_t samples[LDR_SAMPLES] = {0};
uint8_t sampleIndex = 0;
bool windowFilled = false;
bool started = false;
uint32_t lastSampleMs = 0;
uint32_t runningSum = 0;

// Averaged reading, or 0 before the first sample.
uint16_t average() {
  const uint8_t count = windowFilled ? LDR_SAMPLES : sampleIndex;
  if (count == 0) {
    return 0;
  }
  return (uint16_t)(runningSum / count);
}

}  // namespace

void initLdr() {
  pinMode(LDR_PIN, INPUT);
  // 11dB attenuation is required for the full 0-3.3V swing; without it the ADC
  // saturates around 1.1V and the sensor appears stuck in bright light.
  analogSetPinAttenuation(LDR_PIN, ADC_11db);
  started = true;
  lastSampleMs = 0;  // sample on the first updateLdr() call
  DBG_INFO("LDR ready on GPIO %u", LDR_PIN);
}

void updateLdr() {
  if (!started) {
    return;
  }
  const uint32_t now = millis();
  if (lastSampleMs != 0 && (now - lastSampleMs) < LDR_SAMPLE_INTERVAL_MS) {
    return;
  }
  lastSampleMs = now;

  const uint16_t raw = (uint16_t)analogRead(LDR_PIN);

  // Replace this slot's contribution rather than re-summing the window.
  if (windowFilled) {
    runningSum -= samples[sampleIndex];
  }
  samples[sampleIndex] = raw;
  runningSum += raw;

  sampleIndex++;
  if (sampleIndex >= LDR_SAMPLES) {
    sampleIndex = 0;
    windowFilled = true;
  }

  DBG_VERBOSE("LDR raw %u avg %u -> backlight %u", raw, average(), ldrBrightness());
}

bool ldrAvailable() {
  return started && (windowFilled || sampleIndex > 0);
}

uint16_t ldrRaw() {
  return ldrAvailable() ? average() : 0;
}

uint8_t ldrBrightness() {
  if (!ldrAvailable()) {
    return sanitizeBrightnessValue(settings.displayBrightness);
  }

  uint16_t raw = average();
  if (raw < LDR_RAW_BRIGHT) raw = LDR_RAW_BRIGHT;
  if (raw > LDR_RAW_DARK) raw = LDR_RAW_DARK;

  // Inverted: a LOW reading means bright ambient light, which wants a HIGH
  // backlight. map() handles the descending source range directly.
  long level = map(raw, LDR_RAW_BRIGHT, LDR_RAW_DARK, LDR_MAX_BRIGHTNESS,
                   settings.ldrMinBrightness);

  if (level < settings.ldrMinBrightness) level = settings.ldrMinBrightness;
  if (level > LDR_MAX_BRIGHTNESS) level = LDR_MAX_BRIGHTNESS;
  return (uint8_t)level;
}

#else  // !HAS_LDR - board has no photoresistor

void initLdr() {}
void updateLdr() {}
bool ldrAvailable() { return false; }
uint16_t ldrRaw() { return 0; }
uint8_t ldrBrightness() { return sanitizeBrightnessValue(settings.displayBrightness); }

#endif  // HAS_LDR
