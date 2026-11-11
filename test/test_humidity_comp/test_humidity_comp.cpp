#include <unity.h>
#include "HumidityComp.h"

namespace {

HumidityComp hum;

}  // namespace

void setUp(void) {
    hum = HumidityComp();
    hum.begin(100000UL);
}

void tearDown(void) {}

// FR-71: opt-in per zone; at RH >= 60% reduces target proportionally up to 0.5 C at 80% RH
void test_fr71_humidity_compensation_curve_and_fallbacks(void) {
    // Opt-out or invalid sensor -> 0.0 C reduction (fallback)
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, HumidityComp::computeTargetReduction(false, true, false, 75.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, HumidityComp::computeTargetReduction(true, false, false, 75.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, HumidityComp::computeTargetReduction(true, true, true, 75.0f));

    // Below 60% RH -> 0.0 C
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, HumidityComp::computeTargetReduction(true, true, false, 59.9f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, HumidityComp::computeTargetReduction(true, true, false, 60.0f));

    // 70% RH -> (70 - 60) * 0.025 = 0.25 C; 80% -> 0.50 C; 95% -> clamped 0.50 C
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.25f, HumidityComp::computeTargetReduction(true, true, false, 70.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.50f, HumidityComp::computeTargetReduction(true, true, false, 80.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.50f, HumidityComp::computeTargetReduction(true, true, false, 95.0f));

    hum.update(110000UL);
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_fr71_humidity_compensation_curve_and_fallbacks);
    return UNITY_END();
}
