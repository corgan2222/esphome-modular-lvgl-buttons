// esphome-modular-lvgl-buttons/ui/clawdmeter/clawdmeter_engine.h
#pragma once
#include <stdint.h>
#include <string.h>

// ===========================================================================
// usage-rate state machine  (port of Clawdmeter firmware/src/usage_rate.cpp)
// ===========================================================================
extern "C" uint32_t millis();  // provided by ESPHome / host test shim

#define CLAWD_RATE_THRESH_NORMAL  0.10f
#define CLAWD_RATE_THRESH_ACTIVE  0.20f
#define CLAWD_RATE_THRESH_HEAVY   0.33f
#define CLAWD_MIN_WINDOW_MS       240000UL
#define CLAWD_RING_SIZE           6

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
  uint32_t now = millis();
  if (g_count > 0) {
    uint8_t latest = (g_head + CLAWD_RING_SIZE - 1) % CLAWD_RING_SIZE;
    if (session_pct + 5.0f < g_ring[latest].pct) rate_reset();  // session reset
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

#ifndef CLAWD_RATE_ONLY  // ----- the rest needs LVGL + the data header -----
#include "esphome/components/lvgl/lvgl_proxy.h"   // pulls in lvgl.h types
#include "splash_animations.h"
#include <esp_heap_caps.h>

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
  int g = clawd_usage_group();
  if (g < 0 || g >= CLAWD_GROUP_COUNT) g = 0;
  if (g_group_size[g] == 0) return;
  uint8_t slot = g_group_rot[g] % g_group_size[g];
  g_group_rot[g]++;
  int8_t idx = g_group_lists[g][slot];
  if (idx < 0) return;
  g_cur_anim = (uint16_t)idx;
  g_cur_frame = 0;
  g_frame_started = millis();
  g_last_pick = g_frame_started;
  const splash_anim_def_t *a = &splash_anims[g_cur_anim];
  render_frame(a->frames[0], a->palette);
}
}  // namespace clawd_detail

inline lv_obj_t* clawd_init(lv_obj_t* parent, int screen_w, int screen_h) {
  using namespace clawd_detail;
  int min_dim = (screen_w < screen_h) ? screen_w : screen_h;
  g_cell = min_dim / CLAWD_GRID;
  if (g_cell < 4) g_cell = 4;
  g_cw = CLAWD_GRID * g_cell;
  g_ch = CLAWD_GRID * g_cell;
  g_canvasbuf = (uint16_t*)heap_caps_malloc(g_cw * g_ch * 2, MALLOC_CAP_SPIRAM);
  g_rowbuf    = (uint16_t*)heap_caps_malloc(g_cw * 2,        MALLOC_CAP_SPIRAM);
  if (!g_canvasbuf || !g_rowbuf) return nullptr;
  g_canvas = lv_canvas_create(parent);
  lv_canvas_set_buffer(g_canvas, g_canvasbuf, g_cw, g_ch, LV_COLOR_FORMAT_RGB565);
  lv_obj_center(g_canvas);
  resolve_groups();
  if (SPLASH_ANIM_COUNT > 0) {
    const splash_anim_def_t *a = &splash_anims[0];
    render_frame(a->frames[0], a->palette);
    g_frame_started = millis();
    g_last_pick = g_frame_started;
  }
  return g_canvas;
}

inline void clawd_tick() {
  using namespace clawd_detail;
  if (!g_canvas || SPLASH_ANIM_COUNT == 0) return;
  if (millis() - g_last_pick >= CLAWD_ROTATE_MS) pick_for_rate();
  const splash_anim_def_t *a = &splash_anims[g_cur_anim];
  if (a->frame_count == 0) return;
  uint16_t hold = a->holds[g_cur_frame];
  if (millis() - g_frame_started >= hold) {
    g_cur_frame = (g_cur_frame + 1) % a->frame_count;
    g_frame_started = millis();
    render_frame(a->frames[g_cur_frame], a->palette);
  }
}

// Display-field cache for YAML lambdas (text + arc). Also feeds the sampler.
namespace clawd_detail {
static float g_sess_pct = 0, g_week_pct = 0;
static int   g_sess_reset = 0, g_week_reset = 0;
}
inline void clawd_set_usage(float session_pct, int session_reset_mins,
                            float weekly_pct, int weekly_reset_mins) {
  clawd_detail::g_sess_pct   = session_pct;
  clawd_detail::g_sess_reset = session_reset_mins;
  clawd_detail::g_week_pct   = weekly_pct;
  clawd_detail::g_week_reset = weekly_reset_mins;
  clawd_usage_sample(session_pct);
}
inline float clawd_session_pct()    { return clawd_detail::g_sess_pct; }
inline int   clawd_session_reset()  { return clawd_detail::g_sess_reset; }
inline float clawd_weekly_pct()     { return clawd_detail::g_week_pct; }
inline int   clawd_weekly_reset()   { return clawd_detail::g_week_reset; }

#endif  // CLAWD_RATE_ONLY
