#include <unity.h>
#include "SmartWindow.h"

namespace {

SmartWindow sw;

}  // namespace

void setUp(void) {
    sw = SmartWindow();
    sw.begin(100000UL);
}

void tearDown(void) {}

// FR-40.2: Magnus formula AH = (6.112 * exp((17.67 * T) / (T + 243.5)) * RH * 2.1674) / (273.15 + T)
void test_fr40_2_absolute_humidity_magnus(void) {
    // At 20 C, 50% RH: P_sat ~ 23.38 hPa -> AH ~ 8.64 g/m3
    float ah = SmartWindow::calculateAbsoluteHumidity(20.0f, 50.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.15f, 8.64f, ah);

    // Zero / negative humidity returns 0
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, SmartWindow::calculateAbsoluteHumidity(20.0f, 0.0f));
}

// FR-40.2: detection suspended when weather missing or (room - outdoor) < 5.0 C
void test_fr40_2_suspension_delta_t_rule(void) {
    TEST_ASSERT_TRUE(SmartWindow::isDetectionSuspended(false, 22.0f, 5.0f));
    TEST_ASSERT_TRUE(SmartWindow::isDetectionSuspended(true, 22.0f, 18.0f));  // dT = 4.0 < 5.0
    TEST_ASSERT_FALSE(SmartWindow::isDetectionSuspended(true, 22.0f, 17.0f)); // dT = 5.0
    TEST_ASSERT_FALSE(SmartWindow::isDetectionSuspended(true, 22.0f, 2.0f));  // dT = 20.0

    sw.update(110000UL);
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_fr40_2_absolute_humidity_magnus);
    RUN_TEST(test_fr40_2_suspension_delta_t_rule);
    return UNITY_END();
}
