/*
 * CYD_AnimatedPixelClock - Boot health: OTA rollback and crash report
 *
 * Ported from NickoScope's fork of AnimatedPixelClock (commit 8ec3045, by
 * Nikolay Miroshnichenko). Not part of upstream. Adapted for this port: DBG_*
 * logging, thresholds in include/config.h, and the web UI's Diagnostics box.
 *
 * ---- OTA rollback ----------------------------------------------------------
 * The SDK's bootloader rolls back an image that booted once and was not
 * confirmed before the next reset (CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE, set in
 * arduino-esp32 2.0.17's prebuilt SDK). But the core confirms every image in
 * initArduino(), before setup(), unless the weak verifyRollbackLater() hook says
 * the application will do it (cores/esp32/esp32-hal-misc.c). That left rollback
 * catching only an image that dies before setup(). Here the image is confirmed
 * once it has proven itself, so one that crashes - or hangs into the task
 * watchdog, which panics - before that is rolled back on the reset that
 * follows. An image flashed over USB has no OTA state and needs no confirming.
 *
 * ---- Crash report ----------------------------------------------------------
 * On a panic the SDK writes a core dump to the `coredump` partition
 * (CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH, ELF). Its summary is read at the next
 * boot, logged, kept in NVS and the dump erased, so the one after that is not
 * mistaken for this one. Decoding the backtrace needs the ELF of the image
 * named by `image`.
 */

#include "boot_health.h"

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_core_dump.h>
#include <esp_ota_ops.h>
#include <string.h>
#include <time.h>

#include "config.h"
#include "debug.h"

#if CONFIG_APP_ROLLBACK_ENABLE
extern "C" bool verifyRollbackLater() { return true; }
#endif

namespace {

const esp_partition_t *s_running = nullptr;
const esp_partition_t *s_invalid = nullptr;  // the last image that was rolled back
esp_ota_img_states_t s_state = ESP_OTA_IMG_UNDEFINED;
bool s_stateKnown = false;
bool s_pending = false;
uint32_t s_confirmedAtMs = 0;
uint32_t s_frames = 0;
const char *s_confirmError = nullptr;

const char *stateName() {
  if (!s_stateKnown) return "unknown";
  switch (s_state) {
    case ESP_OTA_IMG_NEW: return "new";
    case ESP_OTA_IMG_PENDING_VERIFY: return "pending";
    case ESP_OTA_IMG_VALID: return "valid";
    case ESP_OTA_IMG_INVALID: return "invalid";
    case ESP_OTA_IMG_ABORTED: return "aborted";
    default: return "undefined";  // flashed over USB: no OTA state
  }
}

struct CrashRecord {
  uint32_t magic;
  char task[16];
  uint32_t pc;
  uint32_t cause;  // Xtensa EXCCAUSE
  uint32_t vaddr;  // EXCVADDR
  uint32_t bt[8];
  uint8_t depth;
  bool corrupted;
  char sha[17];        // first 16 hex digits of the crashed image's ELF SHA-256
  int32_t bootReason;  // esp_reset_reason() of the boot that found it
  int64_t seenUtc;     // when that boot first had the time; 0 until then
};
constexpr uint32_t CRASH_MAGIC = 0x31445243;  // "CRD1"
constexpr const char *NVS_NAMESPACE = "health";
CrashRecord s_crash = {};
bool s_crashThisBoot = false;
bool s_crashNeedsUtc = false;

// Xtensa exception causes, in the order of ESP-IDF's own table
// (components/esp_system/port/arch/xtensa/panic_arch.c).
const char *causeName(uint32_t c) {
  static const char *const NAMES[] = {
      "IllegalInstruction", "Syscall", "InstructionFetchError", "LoadStoreError",
      "Level1Interrupt", "Alloca", "IntegerDivideByZero", "PCValue",
      "Privileged", "LoadStoreAlignment", "res", "res",
      "InstrPDAddrError", "LoadStorePIFDataError", "InstrPIFAddrError", "LoadStorePIFAddrError",
      "InstTLBMiss", "InstTLBMultiHit", "InstFetchPrivilege", "res",
      "InstrFetchProhibited", "res", "res", "res",
      "LoadStoreTLBMiss", "LoadStoreTLBMultihit", "LoadStorePrivilege", "res",
      "LoadProhibited", "StoreProhibited",
  };
  return c < sizeof(NAMES) / sizeof(NAMES[0]) ? NAMES[c] : "other";
}

void saveCrash() {
  Preferences p;
  if (p.begin(NVS_NAMESPACE, false)) {
    p.putBytes("crash", &s_crash, sizeof(s_crash));
    p.end();
  }
}

void readCrashReport() {
  Preferences p;
  if (p.begin(NVS_NAMESPACE, false)) {  // read-write: a read-only open of a new namespace logs an error
    if (p.getBytesLength("crash") == sizeof(s_crash)) p.getBytes("crash", &s_crash, sizeof(s_crash));
    p.end();
  }
  if (s_crash.magic != CRASH_MAGIC) memset(&s_crash, 0, sizeof(s_crash));

#if CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH && CONFIG_ESP_COREDUMP_DATA_FORMAT_ELF
  size_t addr = 0, size = 0;
  if (esp_core_dump_image_get(&addr, &size) != ESP_OK || size == 0) return;
  esp_core_dump_summary_t *sum = (esp_core_dump_summary_t *)calloc(1, sizeof(*sum));
  if (!sum) return;
  if (esp_core_dump_get_summary(sum) == ESP_OK) {
    CrashRecord c = {};
    c.magic = CRASH_MAGIC;
    strncpy(c.task, sum->exc_task, sizeof(c.task) - 1);
    c.pc = sum->exc_pc;
    c.cause = sum->ex_info.exc_cause;
    c.vaddr = sum->ex_info.exc_vaddr;
    const uint32_t depth = sum->exc_bt_info.depth;
    c.depth = (uint8_t)(depth < 8 ? depth : 8);
    for (uint8_t i = 0; i < c.depth; i++) c.bt[i] = sum->exc_bt_info.bt[i];
    c.corrupted = sum->exc_bt_info.corrupted;
    // The SDK sizes this string from CONFIG_APP_RETRIEVE_LEN_ELF_SHA; copy no more
    // than either buffer holds.
    size_t shaLen = sizeof(c.sha) - 1;
    if (shaLen > APP_ELF_SHA256_SZ - 1) shaLen = APP_ELF_SHA256_SZ - 1;
    memcpy(c.sha, sum->app_elf_sha256, shaLen);
    c.sha[shaLen] = '\0';
    c.bootReason = (int32_t)esp_reset_reason();
    s_crash = c;
    s_crashThisBoot = true;
    s_crashNeedsUtc = true;
    saveCrash();
    DBG_WARN("Boot health: crash in the last run - task %s, %s (%u), pc 0x%08x, "
             "addr 0x%08x, image %s", c.task, causeName(c.cause), (unsigned)c.cause,
             (unsigned)c.pc, (unsigned)c.vaddr, c.sha);
  } else {
    DBG_WARN("Boot health: a crash report is in flash but could not be read");
  }
  free(sum);
  // Erased either way: a report that cannot be read now will not become readable.
  esp_core_dump_image_erase();
#endif
}

}  // namespace

void healthBegin() {
  s_running = esp_ota_get_running_partition();
  if (s_running && esp_ota_get_state_partition(s_running, &s_state) == ESP_OK) {
    s_stateKnown = true;
    s_pending = (s_state == ESP_OTA_IMG_PENDING_VERIFY);
  }
  s_invalid = esp_ota_get_last_invalid_partition();
  DBG_INFO("Boot health: running %s, OTA state %s%s%s", s_running ? s_running->label : "?",
           stateName(), s_invalid ? ", rolled back from " : "",
           s_invalid ? s_invalid->label : "");
  if (s_pending) {
    DBG_INFO("Boot health: new image, confirmed after %us with WiFi up and %u frames drawn",
             (unsigned)(OTA_CONFIRM_AFTER_MS / 1000UL), (unsigned)OTA_CONFIRM_FRAMES);
  }
  readCrashReport();
}

void healthClear() {
  Preferences p;
  if (p.begin(NVS_NAMESPACE, false)) {
    p.clear();
    p.end();
  }
  memset(&s_crash, 0, sizeof(s_crash));
  s_crashNeedsUtc = false;
  DBG_INFO("Boot health: crash report erased");
}

void healthNoteFrame() {
  if (s_frames < 0xFFFFFFFFUL) s_frames++;
}

void healthTick(bool displayIdle) {
  if (s_crashNeedsUtc) {
    const time_t now = time(nullptr);
    if (now > 1700000000) {
      s_crash.seenUtc = (int64_t)now;
      s_crashNeedsUtc = false;
      saveCrash();
    }
  }
#if CONFIG_APP_ROLLBACK_ENABLE
  if (!s_pending) return;
#if defined(HEALTH_ROLLBACK_TEST)
  // Test image only, built with -DHEALTH_ROLLBACK_TEST: it dies before it can be
  // confirmed, so the bootloader has to put the previous image back.
  if (millis() > 20000UL) {
    DBG_ERROR("Boot health: HEALTH_ROLLBACK_TEST - aborting before the image is confirmed");
    Serial.flush();
    abort();
  }
  return;
#endif
  if (millis() < OTA_CONFIRM_AFTER_MS || WiFi.status() != WL_CONNECTED) return;
  if (!displayIdle && s_frames < OTA_CONFIRM_FRAMES) return;
  const esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
  s_pending = false;
  if (err == ESP_OK) {
    s_state = ESP_OTA_IMG_VALID;
    s_confirmedAtMs = millis();
    DBG_INFO("Boot health: OTA image confirmed after %us and %u frames - no rollback from here",
             (unsigned)(s_confirmedAtMs / 1000UL), (unsigned)s_frames);
  } else {
    s_confirmError = esp_err_to_name(err);
    DBG_ERROR("Boot health: could not confirm the OTA image: %s", s_confirmError);
  }
#else
  (void)displayIdle;
#endif
}

void healthInfoJson(JsonObject out) {
  JsonObject ota = out["ota"].to<JsonObject>();
  ota["partition"] = s_running ? String(s_running->label) : String();
  ota["state"] = stateName();
  if (s_invalid) ota["rolledBackFrom"] = String(s_invalid->label);
  if (s_confirmedAtMs) ota["confirmedAtS"] = s_confirmedAtMs / 1000UL;
  if (s_pending) {
    ota["confirmsAfterS"] = OTA_CONFIRM_AFTER_MS / 1000UL;
    ota["frames"] = s_frames;
  }
  if (s_confirmError) ota["error"] = s_confirmError;

  if (s_crash.magic != CRASH_MAGIC) return;
  JsonObject c = out["lastCrash"].to<JsonObject>();
  char hex[12];
  c["task"] = String(s_crash.task);
  c["cause"] = s_crash.cause;
  c["causeName"] = causeName(s_crash.cause);
  snprintf(hex, sizeof(hex), "0x%08x", (unsigned)s_crash.pc);
  c["pc"] = String(hex);
  snprintf(hex, sizeof(hex), "0x%08x", (unsigned)s_crash.vaddr);
  c["addr"] = String(hex);
  c["image"] = String(s_crash.sha);
  c["bootReason"] = s_crash.bootReason;
  c["thisBoot"] = s_crashThisBoot;
  if (s_crash.seenUtc) c["seenUtc"] = s_crash.seenUtc;
  JsonArray bt = c["backtrace"].to<JsonArray>();
  for (uint8_t i = 0; i < s_crash.depth; i++) {
    snprintf(hex, sizeof(hex), "0x%08x", (unsigned)s_crash.bt[i]);
    bt.add(String(hex));
  }
  if (s_crash.corrupted) c["backtraceCorrupted"] = true;
}
