#include <unity.h>
#include "PlantMonitor.h"

namespace {

PlantMonitor pm;
constexpr uint32_t T0 = 100000UL;
constexpr uint32_t OVERHEAT_MS = 60UL * 60UL * 1000UL;   // FR-56: 60 min default
constexpr uint32_t FLOOR_MS = 12600000UL;                // FR-57: 3.5 h for underfloor
constexpr uint32_t RAD_MS = 2700000UL;                   // FR-57: 45 min for radiators

}  // namespace

void setUp(void) {
    pm = PlantMonitor();
    pm.begin(T0);
}

void tearDown(void) {}

// FR-56: valve closed, temp > target + margin (2 C) and rising >= 0.1 C for overheatTime (60 min) -> DTC-203; clears at <= target
void test_fr56_overheat_raise_and_clear(void) {
    auto ev0 = pm.evaluateZone(0, T0, true, true, 24.5f, 22.0f, false, 0.0f, true, 2.0f, OVERHEAT_MS, 0.5f, 3UL * 3600000UL, 0.2f);
    TEST_ASSERT_FALSE(ev0.raiseOverheat);
    TEST_ASSERT_FALSE(pm.isZoneOverheating(0));

    // 30 min in (< 60 min): not yet
    auto ev1 = pm.evaluateZone(0, T0 + 1800000UL, true, true, 24.8f, 22.0f, false, 0.0f, true, 2.0f, OVERHEAT_MS, 0.5f, 3UL * 3600000UL, 0.2f);
    TEST_ASSERT_FALSE(ev1.raiseOverheat);

    // 60 min in and still rising (+0.3 C >= 0.1 C) -> raises overheat
    auto ev2 = pm.evaluateZone(0, T0 + OVERHEAT_MS, true, true, 24.8f, 22.0f, false, 0.0f, true, 2.0f, OVERHEAT_MS, 0.5f, 3UL * 3600000UL, 0.2f);
    TEST_ASSERT_TRUE(ev2.raiseOverheat);
    TEST_ASSERT_TRUE(pm.isZoneOverheating(0));

    // Recovers down to target -> clears overheat
    auto ev3 = pm.evaluateZone(0, T0 + OVERHEAT_MS + 60000UL, true, true, 22.0f, 22.0f, false, 0.0f, true, 2.0f, OVERHEAT_MS, 0.5f, 3UL * 3600000UL, 0.2f);
    TEST_ASSERT_TRUE(ev3.clearOverheat);
    TEST_ASSERT_FALSE(pm.isZoneOverheating(0));
}

// FR-57: floor enforces >= 3.5 h (12600000 ms) vs radiators >= 45 min (2700000 ms), clears when rise >= noEffectRise (0.2 C)
void test_fr57_no_effect_floor_vs_radiators(void) {
    // Verify thresholds: even if noEffectTime configured to 1h (3600000 ms), floor keeps 3.5h and radiators keep 45m
    TEST_ASSERT_EQUAL_UINT32(FLOOR_MS, PlantMonitor::effectiveNoEffectTimeMs(true, 3600000UL));
    TEST_ASSERT_EQUAL_UINT32(RAD_MS, PlantMonitor::effectiveNoEffectTimeMs(false, 1800000UL));

    // Floor zone 0 with 80% duty (>= 50%), stagnant (+0.1 C < 0.2 C): must NOT raise at 3h, MUST raise at 3.5h
    pm.evaluateZone(0, T0, true, true, 20.0f, 22.0f, true, 0.8f, true, 2.0f, OVERHEAT_MS, 0.5f, 3UL * 3600000UL, 0.2f);
    auto at3h = pm.evaluateZone(0, T0 + 3UL * 3600000UL, true, true, 20.1f, 22.0f, true, 0.8f, true, 2.0f, OVERHEAT_MS, 0.5f, 3UL * 3600000UL, 0.2f);
    TEST_ASSERT_FALSE(at3h.raiseNoEffect);
    TEST_ASSERT_FALSE(pm.isZoneNoEffect(0));

    auto at3h30 = pm.evaluateZone(0, T0 + FLOOR_MS, true, true, 20.1f, 22.0f, true, 0.8f, true, 2.0f, OVERHEAT_MS, 0.5f, 3UL * 3600000UL, 0.2f);
    TEST_ASSERT_TRUE(at3h30.raiseNoEffect);
    TEST_ASSERT_TRUE(pm.isZoneNoEffect(0));

    // Radiator zone 1 raises after 45 min
    pm.evaluateZone(1, T0, true, true, 20.0f, 22.0f, true, 0.8f, false, 2.0f, OVERHEAT_MS, 0.5f, 1800000UL, 0.2f);
    auto radAt45m = pm.evaluateZone(1, T0 + RAD_MS, true, true, 20.0f, 22.0f, true, 0.8f, false, 2.0f, OVERHEAT_MS, 0.5f, 1800000UL, 0.2f);
    TEST_ASSERT_TRUE(radAt45m.raiseNoEffect);
    TEST_ASSERT_TRUE(pm.isZoneNoEffect(1));
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_fr56_overheat_raise_and_clear);
    RUN_TEST(test_fr57_no_effect_floor_vs_radiators);
    return UNITY_END();
}
