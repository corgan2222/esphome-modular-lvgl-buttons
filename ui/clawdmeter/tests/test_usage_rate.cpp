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

  if (fails) { printf("%d test(s) failed\n", fails); return 1; }
  printf("OK: usage_rate state machine\n"); return 0;
}
