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

  if (fails) { printf("%d test(s) failed\n", fails); return 1; }
  printf("OK: usage_rate state machine\n"); return 0;
}
