// ui/clawdmeter/tests/test_usage_rate.cpp
// Host build: g++ -std=c++17 ui/clawdmeter/tests/test_usage_rate.cpp -o /tmp/urt
#include <cstdio>
#include <cstdint>

// --- host shims so the engine's rate section compiles standalone ---
static uint32_t g_now = 0;
extern "C" uint32_t millis() { return g_now; }

// Pull in ONLY the rate state machine via the guard the engine exposes.
#define CLAWD_RATE_ONLY
#include "../clawdmeter_engine.h"

static int fails = 0;
#define EXPECT_EQ(a,b) do{ if((a)!=(b)){ \
  printf("FAIL %s:%d  %s == %d, got %d\n",__FILE__,__LINE__,#a,(int)(b),(int)(a)); fails++; } }while(0)

int main() {
  // Fewer than 2 samples => idle (0)
  g_now = 0; clawd_usage_sample(10.0f);
  EXPECT_EQ(clawd_usage_group(), 0);

  // Two samples but window < 4 min => still idle
  g_now = 60000; clawd_usage_sample(15.0f);
  EXPECT_EQ(clawd_usage_group(), 0);

  // Span >= 4 min, +1%/min over the window => heavy (>=0.33). 10->50 in 300s = 8 %/min
  g_now = 300000; clawd_usage_sample(50.0f);
  EXPECT_EQ(clawd_usage_group(), 3);

  // Session reset detection: pct drops > 5 => tracking restarts => idle
  g_now = 360000; clawd_usage_sample(2.0f);
  EXPECT_EQ(clawd_usage_group(), 0);

  // Rebuild a slow trend: +0.15 %/min => normal (>=0.10, <0.20)
  g_now = 360000; clawd_usage_sample(2.0f);
  g_now = 660000; clawd_usage_sample(2.0f + 0.15f*5); // +0.75% over 5 min = 0.15 %/min
  EXPECT_EQ(clawd_usage_group(), 1);

  // Reconnect-burst dedup: a flapping HA connection re-publishes the SAME value
  // many times within milliseconds. Those bursts must NOT collapse the window.
  // Start fresh, lay a clean 5-min span, then slam in a burst at the end.
  clawd_detail::rate_reset();
  g_now = 1000000; clawd_usage_sample(10.0f);
  g_now = 1300000; clawd_usage_sample(10.30f);
  EXPECT_EQ(clawd_usage_sample_count(), 2);
  // 4 identical-value samples ~1 ms apart simulate an API reconnect storm.
  g_now = 1300001; clawd_usage_sample(10.30f);
  g_now = 1300002; clawd_usage_sample(10.30f);
  g_now = 1300003; clawd_usage_sample(10.30f);
  g_now = 1300004; clawd_usage_sample(10.30f);
  // All four were debounced away: still 2 samples, window still the full 5 min.
  EXPECT_EQ(clawd_usage_sample_count(), 2);
  EXPECT_EQ((int)(clawd_usage_window_ms() / 1000), 300);

  // ---- weekly mode: mood from the weekly pace, "working" after a rise ----
  // Fresh state: no rise yet, no pace yet -> idle.
  clawd_detail::rate_reset();
  clawd_detail::g_prev_pct = -1.0f;
  clawd_detail::g_rise = false;
  clawd_set_week_pace(NAN);
  EXPECT_EQ(clawd_weekly_group(), 0);
  // Pace bands: < -20 idle, < 0 normal, < 10 active, else heavy.
  clawd_set_week_pace(-43.4f); EXPECT_EQ(clawd_weekly_group(), 0);
  clawd_set_week_pace(-5.0f);  EXPECT_EQ(clawd_weekly_group(), 1);
  clawd_set_week_pace(4.0f);   EXPECT_EQ(clawd_weekly_group(), 2);
  clawd_set_week_pace(15.0f);  EXPECT_EQ(clawd_weekly_group(), 3);
  // A rise in usage lifts an idle pace to "normal" for 30 min, then lets go.
  clawd_set_week_pace(-43.4f);
  g_now = 5000000; clawd_usage_sample(43.0f);   // first sample: no rise
  EXPECT_EQ(clawd_weekly_group(), 0);
  g_now = 5100000; clawd_usage_sample(44.0f);   // +1 % -> working
  EXPECT_EQ(clawd_weekly_group(), 1);
  g_now = 5100000 + 1800000 - 1; EXPECT_EQ(clawd_weekly_group(), 1);
  g_now = 5100000 + 1800000;     EXPECT_EQ(clawd_weekly_group(), 0);
  // A rise never lowers a busier pace band.
  clawd_set_week_pace(15.0f); EXPECT_EQ(clawd_weekly_group(), 3);
  // Same value again (reconnect republish) is not a rise.
  clawd_set_week_pace(-43.4f);
  g_now = 9000000; clawd_usage_sample(44.0f);
  EXPECT_EQ(clawd_weekly_group(), 0);

  // ---- duration formatting ----
  char big[12], small[12], line[24];
  clawd_fmt_duration(45, "min", big, sizeof(big), small, sizeof(small));
  EXPECT_EQ(strcmp(big, "45"), 0);  EXPECT_EQ(strcmp(small, "min"), 0);
  clawd_fmt_duration(1239, "min", big, sizeof(big), small, sizeof(small));
  EXPECT_EQ(strcmp(big, "20h"), 0); EXPECT_EQ(strcmp(small, "39m"), 0);
  clawd_fmt_duration(12001, "min", big, sizeof(big), small, sizeof(small));
  EXPECT_EQ(strcmp(big, "8d"), 0);  EXPECT_EQ(strcmp(small, "08h"), 0);
  clawd_fmt_duration(-5, "min", big, sizeof(big), small, sizeof(small));
  EXPECT_EQ(strcmp(big, "0"), 0);
  clawd_fmt_duration(2147483647, "min", big, sizeof(big), small, sizeof(small));
  EXPECT_EQ(strcmp(big, "99d"), 0); EXPECT_EQ(strcmp(small, "23h"), 0);
  clawd_fmt_duration_line(10612, "min", line, sizeof(line));
  EXPECT_EQ(strcmp(line, "7d 08h"), 0);
  clawd_fmt_duration_line(59, "min", line, sizeof(line));
  EXPECT_EQ(strcmp(line, "59 min"), 0);
  clawd_fmt_duration_line(60, "min", line, sizeof(line));
  EXPECT_EQ(strcmp(line, "1h 00m"), 0);

  // ---- model store ----
  EXPECT_EQ(std::isnan(clawd_model_usage()), 1);
  EXPECT_EQ(strcmp(clawd_model_label(), ""), 0);
  clawd_set_model_usage(7.0f); clawd_set_model_allowance(50.0f); clawd_set_model_label("Fable");
  EXPECT_EQ((int) clawd_model_usage(), 7);
  EXPECT_EQ((int) clawd_model_allowance(), 50);
  EXPECT_EQ(strcmp(clawd_model_label(), "Fable"), 0);
  clawd_set_model_label("A-very-long-model-name-that-overflows");  // truncated, no overrun
  EXPECT_EQ((int) strlen(clawd_model_label()), 23);

  EXPECT_EQ(clawd_first_int("in 12001 min"), 12001);
  EXPECT_EQ(clawd_first_int("unknown"), -1);
  EXPECT_EQ(clawd_first_int(""), -1);

  if (fails) { printf("%d test(s) failed\n", fails); return 1; }
  printf("OK: usage_rate state machine, weekly mode, duration format\n"); return 0;
}
