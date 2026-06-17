// esphome-modular-lvgl-buttons/ui/clawdmeter/clawdmeter_engine.h
#pragma once
#include <stdint.h>
#include <string.h>
#include <stdio.h>

// ---- monotonic time source -------------------------------------------------
// ESPHome already provides esphome::millis(); declaring our own global millis()
// makes the unqualified call ambiguous (main.cpp does `using namespace esphome`).
// Route every call through clawd_now() so the host test can still shim it.
#ifdef CLAWD_RATE_ONLY
extern "C" uint32_t millis();  // host test shim
static inline uint32_t clawd_now() { return millis(); }
#else
#include "esphome/core/hal.h"  // esphome::millis()
static inline uint32_t clawd_now() { return esphome::millis(); }
#endif

// ===========================================================================
// usage-rate state machine  (port of Clawdmeter firmware/src/usage_rate.cpp)
// ===========================================================================

#define CLAWD_RATE_THRESH_NORMAL  0.10f
#define CLAWD_RATE_THRESH_ACTIVE  0.20f
#define CLAWD_RATE_THRESH_HEAVY   0.33f
#define CLAWD_MIN_WINDOW_MS       120000UL   // 2 min: warm up faster after a reset
#define CLAWD_RING_SIZE           12         // ~24-36 min of history at HA's 2-3 min cadence
// Reconnect debounce: the HA `homeassistant` sensor re-publishes the current
// value on every API (re)connect, so a flapping connection delivers a BURST of
// identical-value samples within a few ms of each other. Left unchecked those
// bursts fill the ring with near-identical timestamps, collapsing the window
// below CLAWD_MIN_WINDOW_MS and flapping the creature back to "warming up".
// Ignore any sample that arrives within this gap of the last stored one.
#define CLAWD_MIN_SAMPLE_GAP_MS   15000UL

namespace clawd_detail {
struct Sample { uint32_t ms; float pct; };
static Sample  g_ring[CLAWD_RING_SIZE];
static uint8_t g_count = 0;
static uint8_t g_head  = 0;  // next write slot
inline uint8_t oldest_idx() { return (g_head + CLAWD_RING_SIZE - g_count) % CLAWD_RING_SIZE; }
inline void rate_reset() { g_count = 0; g_head = 0; }
}  // namespace clawd_detail

inline void clawd_usage_sample(float session_pct) {
  using namespace clawd_detail;
  uint32_t now = clawd_now();
  if (g_count > 0) {
    uint8_t latest = (g_head + CLAWD_RING_SIZE - 1) % CLAWD_RING_SIZE;
    if (session_pct + 5.0f < g_ring[latest].pct) {
      rate_reset();  // session reset: a real drop always restarts tracking
    } else if (now - g_ring[latest].ms < CLAWD_MIN_SAMPLE_GAP_MS) {
      return;        // reconnect burst: ignore samples that arrive too close together
    }
  }
  g_ring[g_head] = { now, session_pct };
  g_head = (g_head + 1) % CLAWD_RING_SIZE;
  if (g_count < CLAWD_RING_SIZE) g_count++;
}

inline int clawd_usage_group() {
  using namespace clawd_detail;
  if (g_count < 2) return 0;
  uint8_t o = oldest_idx();
  uint8_t l = (g_head + CLAWD_RING_SIZE - 1) % CLAWD_RING_SIZE;
  uint32_t dt = g_ring[l].ms - g_ring[o].ms;
  if (dt < CLAWD_MIN_WINDOW_MS) return 0;
  float dp = g_ring[l].pct - g_ring[o].pct;
  if (dp < 0.0f) dp = 0.0f;
  float rate = dp * 60000.0f / (float)dt;
  if (rate < CLAWD_RATE_THRESH_NORMAL) return 0;
  if (rate < CLAWD_RATE_THRESH_ACTIVE) return 1;
  if (rate < CLAWD_RATE_THRESH_HEAVY)  return 2;
  return 3;
}

// ---- Diagnostics: expose the raw figures behind clawd_usage_group() --------
// clawd_usage_group() collapses everything into a 0..3 bucket, so a reported 0
// is ambiguous (too few samples? window too short? rate below threshold?).
// These accessors surface the underlying numbers for the ESPHome dashboard.

// Current usage rate in %/min over the ring window, using the SAME maths as
// clawd_usage_group(). Returns -1.0f while there is not yet enough data to
// decide (fewer than 2 samples, or the window is shorter than CLAWD_MIN_WINDOW_MS).
inline float clawd_usage_rate_per_min() {
  using namespace clawd_detail;
  if (g_count < 2) return -1.0f;
  uint8_t o = oldest_idx();
  uint8_t l = (g_head + CLAWD_RING_SIZE - 1) % CLAWD_RING_SIZE;
  uint32_t dt = g_ring[l].ms - g_ring[o].ms;
  if (dt < CLAWD_MIN_WINDOW_MS) return -1.0f;
  float dp = g_ring[l].pct - g_ring[o].pct;
  if (dp < 0.0f) dp = 0.0f;
  return dp * 60000.0f / (float)dt;
}

// Number of usage samples currently held in the ring buffer (0..CLAWD_RING_SIZE).
inline int clawd_usage_sample_count() { return clawd_detail::g_count; }

// Milliseconds spanned by the samples in the ring (oldest..newest). 0 if <2.
inline uint32_t clawd_usage_window_ms() {
  using namespace clawd_detail;
  if (g_count < 2) return 0;
  uint8_t o = oldest_idx();
  uint8_t l = (g_head + CLAWD_RING_SIZE - 1) % CLAWD_RING_SIZE;
  return g_ring[l].ms - g_ring[o].ms;
}

// Human-readable name of a usage group (0..3), or "?" if out of range.
inline const char* clawd_usage_group_name(int g) {
  switch (g) {
    case 0: return "idle";
    case 1: return "normal";
    case 2: return "active";
    case 3: return "heavy";
    default: return "?";
  }
}

// ---- ISO-8601 timestamp -> "minutes from now" -----------------------------
// The HA Claude-usage reset_time sensors emit a UTC ISO-8601 string such as
// "2026-06-17T06:30:00+00:00". Parse it and return whole minutes from
// now_epoch until that instant, clamped at 0. Returns -1 when the string is
// empty / unparseable. The trailing zone offset is ignored: HA emits "+00:00"
// so the broken-down fields are treated as UTC, matching now_epoch (epoch UTC).
inline int clawd_iso_minutes_from(const char* iso, long long now_epoch) {
  if (!iso || !*iso) return -1;
  int Y = 0, Mo = 0, D = 0, H = 0, Mi = 0, S = 0;
  if (sscanf(iso, "%d-%d-%dT%d:%d:%d", &Y, &Mo, &D, &H, &Mi, &S) < 5) return -1;
  if (Mo < 1 || Mo > 12 || D < 1 || D > 31) return -1;
  // days_from_civil (Howard Hinnant), proleptic Gregorian, epoch 1970-01-01.
  int y = Y - (Mo <= 2);
  long era = (y >= 0 ? y : y - 399) / 400;
  unsigned yoe = (unsigned)(y - era * 400);
  unsigned doy = (153u * (unsigned)(Mo + (Mo > 2 ? -3 : 9)) + 2u) / 5u + (unsigned)D - 1u;
  unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
  long long days = era * 146097LL + (long long)doe - 719468LL;
  long long target = days * 86400LL + (long long)H * 3600LL + (long long)Mi * 60LL + (long long)S;
  long long diff = target - now_epoch;
  if (diff < 0) diff = 0;
  return (int)(diff / 60);
}

#ifndef CLAWD_RATE_ONLY  // ----- the rest needs LVGL + the data header -----
#include "esphome/components/lvgl/lvgl_proxy.h"   // pulls in lvgl.h types
#include "splash_animations.h"
#include <cstdlib>
// On ESP targets the canvas buffer is placed in PSRAM via the ESP-IDF heap API.
// The SDL/host build has neither esp_heap_caps.h nor PSRAM, so fall back to a
// plain malloc there. __has_include keeps this self-contained — no reliance on
// ESPHome's USE_* defines being visible at this include point.
#if defined(__has_include) && __has_include(<esp_heap_caps.h>)
#  include <esp_heap_caps.h>
#  define CLAWD_HAS_PSRAM 1
#endif

namespace clawd_detail {
#define CLAWD_GRID 20
#define CLAWD_COL_EMPTY 0x0000
#define CLAWD_ROTATE_MS 20000
#define CLAWD_GROUP_COUNT 4
#define CLAWD_GROUP_MAX   4

static lv_obj_t *g_canvas   = nullptr;
static uint16_t *g_canvasbuf = nullptr;
static uint16_t *g_rowbuf    = nullptr;
static int       g_cell = 24, g_cw = 480, g_ch = 480;
static uint16_t  g_cur_anim = 0, g_cur_frame = 0;
static uint32_t  g_frame_started = 0, g_last_pick = 0;
// Animation selection mode: -1 = automatic (driven by usage group/rate);
// 0..SPLASH_ANIM_COUNT-1 = forced to that specific animation index.
static int       g_anim_mode = -1;

static int8_t  g_group_lists[CLAWD_GROUP_COUNT][CLAWD_GROUP_MAX];
static uint8_t g_group_size[CLAWD_GROUP_COUNT] = {0};
static uint8_t g_group_rot[CLAWD_GROUP_COUNT]  = {0};

static const char* GROUP_NAMES[CLAWD_GROUP_COUNT][CLAWD_GROUP_MAX] = {
  { "expression sleep", "idle breathe", "idle blink", "expression wink" }, // 0 idle
  { "idle look around", "work think", "work coding", nullptr },            // 1 normal
  { "dance sway", "expression surprise", "dance bounce", nullptr },        // 2 active
  { "dance bounce dj", "dance sway dj", "dance djmix", nullptr },          // 3 heavy
};

inline void resolve_groups() {
  for (int g = 0; g < CLAWD_GROUP_COUNT; g++) {
    g_group_size[g] = 0;
    for (int s = 0; s < CLAWD_GROUP_MAX; s++) {
      g_group_lists[g][s] = -1;
      const char* want = GROUP_NAMES[g][s];
      if (!want) continue;
      for (int i = 0; i < SPLASH_ANIM_COUNT; i++)
        if (strcmp(splash_anims[i].name, want) == 0) {
          g_group_lists[g][g_group_size[g]++] = (int8_t)i; break;
        }
    }
  }
}

inline void render_frame(const uint8_t *cells, const uint16_t *palette) {
  if (!g_rowbuf || !g_canvasbuf) return;
  for (int gy = 0; gy < CLAWD_GRID; gy++) {
    for (int gx = 0; gx < CLAWD_GRID; gx++) {
      uint8_t code = cells[gy * CLAWD_GRID + gx];
      uint16_t color = (palette && code < SPLASH_PALETTE_SIZE) ? palette[code] : CLAWD_COL_EMPTY;
      uint16_t *p = &g_rowbuf[gx * g_cell];
      for (int i = 0; i < g_cell; i++) p[i] = color;
    }
    for (int dy = 0; dy < g_cell; dy++)
      memcpy(&g_canvasbuf[(gy * g_cell + dy) * g_cw], g_rowbuf, g_cw * 2);
  }
  if (g_canvas) lv_obj_invalidate(g_canvas);
}

inline void pick_for_rate() {
  if (SPLASH_ANIM_COUNT == 0) return;
  // Manual override: hold the chosen animation. Refresh g_last_pick so the
  // 20s auto-rotate never fires, and only (re)start the clip when it changes
  // so a running animation isn't reset to frame 0 every rotate interval.
  if (g_anim_mode >= 0 && g_anim_mode < SPLASH_ANIM_COUNT) {
    g_last_pick = clawd_now();
    if (g_cur_anim != (uint16_t) g_anim_mode) {
      g_cur_anim = (uint16_t) g_anim_mode;
      g_cur_frame = 0;
      g_frame_started = clawd_now();
      const splash_anim_def_t *a = &splash_anims[g_cur_anim];
      render_frame(a->frames[0], a->palette);
    }
    return;
  }
  int g = clawd_usage_group();
  if (g < 0 || g >= CLAWD_GROUP_COUNT) g = 0;
  if (g_group_size[g] == 0) return;
  uint8_t slot = g_group_rot[g] % g_group_size[g];
  g_group_rot[g]++;
  int8_t idx = g_group_lists[g][slot];
  if (idx < 0) return;
  g_cur_anim = (uint16_t)idx;
  g_cur_frame = 0;
  g_frame_started = clawd_now();
  g_last_pick = g_frame_started;
  const splash_anim_def_t *a = &splash_anims[g_cur_anim];
  render_frame(a->frames[0], a->palette);
}
}  // namespace clawd_detail

// Create the creature canvas and fit it into `parent`. The canvas is a square
// of CLAWD_GRID*g_cell px, sized from the parent's *measured* content box so the
// creature scales to whatever region the YAML layout hands it — works on every
// supported resolution/orientation without hardcoded pixels. screen_w/screen_h
// are only a fallback for the rare case the layout isn't resolved yet.
inline lv_obj_t* clawd_init(lv_obj_t* parent, int screen_w, int screen_h) {
  using namespace clawd_detail;
  // Resolve the layout so the parent has a real size, then measure it.
  lv_obj_update_layout(lv_obj_get_screen(parent));
  int pw = lv_obj_get_content_width(parent);
  int ph = lv_obj_get_content_height(parent);
  if (pw <= 0) pw = screen_w;
  if (ph <= 0) ph = screen_h;
  int min_dim = (pw < ph) ? pw : ph;
  if (min_dim <= 0) min_dim = (screen_w < screen_h) ? screen_w : screen_h;
  g_cell = min_dim / CLAWD_GRID;
  if (g_cell < 4) g_cell = 4;
  g_cw = CLAWD_GRID * g_cell;
  g_ch = CLAWD_GRID * g_cell;
#ifdef CLAWD_HAS_PSRAM
  g_canvasbuf = (uint16_t*)heap_caps_malloc(g_cw * g_ch * 2, MALLOC_CAP_SPIRAM);
  g_rowbuf    = (uint16_t*)heap_caps_malloc(g_cw * 2,        MALLOC_CAP_SPIRAM);
#else
  // SDL/host build: no PSRAM, plain heap is fine for the small canvas buffer.
  g_canvasbuf = (uint16_t*)malloc(g_cw * g_ch * 2);
  g_rowbuf    = (uint16_t*)malloc(g_cw * 2);
#endif
  if (!g_canvasbuf || !g_rowbuf) return nullptr;
  g_canvas = lv_canvas_create(parent);
  lv_canvas_set_buffer(g_canvas, g_canvasbuf, g_cw, g_ch, LV_COLOR_FORMAT_RGB565);
  lv_obj_center(g_canvas);
  resolve_groups();
  if (SPLASH_ANIM_COUNT > 0) {
    const splash_anim_def_t *a = &splash_anims[0];
    render_frame(a->frames[0], a->palette);
    g_frame_started = clawd_now();
    g_last_pick = g_frame_started;
  }
  return g_canvas;
}

inline void clawd_tick() {
  using namespace clawd_detail;
  if (!g_canvas || SPLASH_ANIM_COUNT == 0) return;
  if (clawd_now() - g_last_pick >= CLAWD_ROTATE_MS) pick_for_rate();
  const splash_anim_def_t *a = &splash_anims[g_cur_anim];
  if (a->frame_count == 0) return;
  uint16_t hold = a->holds[g_cur_frame];
  if (clawd_now() - g_frame_started >= hold) {
    g_cur_frame = (g_cur_frame + 1) % a->frame_count;
    g_frame_started = clawd_now();
    render_frame(a->frames[g_cur_frame], a->palette);
  }
}

// Feed the rate sampler. Only session_pct drives the animation group; the
// other fields are displayed directly from the HA sensors in YAML, so they are
// accepted for call-site symmetry but intentionally unused here.
// NOTE: do not add accessor functions named after the HA sensor IDs
// (clawd_session_pct, clawd_weekly_pct, …) — ESPHome generates C++ globals with
// those exact names, which would collide.
inline void clawd_set_usage(float session_pct, int session_reset_mins,
                            float weekly_pct, int weekly_reset_mins) {
  (void) session_reset_mins; (void) weekly_pct; (void) weekly_reset_mins;
  clawd_usage_sample(session_pct);
}

// ---- Animation selection API (manual override / auto) ----------------------
// Set the animation mode: -1 = automatic (usage-driven), or a 0-based index
// into splash_anims[] to force a specific animation. Applies immediately.
inline void clawd_set_anim_mode(int mode) {
  using namespace clawd_detail;
  if (mode < -1 || mode >= SPLASH_ANIM_COUNT) mode = -1;
  g_anim_mode = mode;
  pick_for_rate();  // apply now (auto re-picks by rate; forced jumps to clip)
}

// Current mode: -1 = auto, else the forced animation index.
inline int clawd_get_anim_mode() { return clawd_detail::g_anim_mode; }

// Total number of selectable animations.
inline int clawd_anim_count() { return SPLASH_ANIM_COUNT; }

// Name of the animation at index i (nullptr if out of range).
inline const char* clawd_anim_name_at(int i) {
  if (i < 0 || i >= SPLASH_ANIM_COUNT) return nullptr;
  return splash_anims[i].name;
}

// Name of the animation currently on screen (for status/exposed text_sensor).
inline const char* clawd_current_anim_name() {
  return splash_anims[clawd_detail::g_cur_anim].name;
}

#endif  // CLAWD_RATE_ONLY
