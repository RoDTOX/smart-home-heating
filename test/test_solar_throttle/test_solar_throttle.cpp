#include <unity.h>
#include "SolarThrottle.h"

namespace {

SolarThrottle solar;

}  // namespace

void setUp(void) {
    solar = SolarThrottle();
    solar.begin(100000UL);
}

void tearDown(void) {}

// FR-70: throttling activates only when solarGain=true, weatherValid=true, and irradiance >= solarLimit
void test_fr70_throttling_conditions(void) {
    TEST_ASSERT_FALSE(SolarThrottle::computeThrottlingActive(false, true, 500.0f, 300.0f));
    TEST_ASSERT_FALSE(SolarThrottle::computeThrottlingActive(true, false, 500.0f, 300.0f)); // Fallback: no forecast -> no reduction
    TEST_ASSERT_FALSE(SolarThrottle::computeThrottlingActive(true, true, 299.0f, 300.0f));
    TEST_ASSERT_TRUE(SolarThrottle::computeThrottlingActive(true, true, 300.0f, 300.0f));
    TEST_ASSERT_TRUE(SolarThrottle::computeThrottlingActive(true, true, 650.0f, 300.0f));
}

// FR-70: throttle factor = 1 - reduction/100, floored at 0.10
void test_fr70_throttle_factor_and_time_update(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, SolarThrottle::computeThrottleFactor(true, false, 500.0f, 300.0f, 30.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.7f, SolarThrottle::computeThrottleFactor(true, true, 450.0f, 300.0f, 30.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.1f, SolarThrottle::computeThrottleFactor(true, true, 450.0f, 300.0f, 95.0f));

    solar.setForecastIrradiance(420.0f);
    solar.update(110000UL);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 420.0f, solar.getForecastIrradiance());
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_fr70_throttling_conditions);
    RUN_TEST(test_fr70_throttle_factor_and_time_update);
    return UNITY_END();
}
