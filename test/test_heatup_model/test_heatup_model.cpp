#include <unity.h>
#include "HeatUpModel.h"

namespace {

HeatUpModel model;
constexpr uint32_t T0 = 100000UL;
constexpr uint32_t ONE_HOUR_MS = 3600000UL;

}  // namespace

void setUp(void) {
    model = HeatUpModel();
    model.begin(T0);
}

void tearDown(void) {}

// FR-48 & FR-67 fallback: 120 + 9 * (5 - Tout), clamped [45..240] min; 90 min on weather lost
void test_fr48_preheat_lead_formula_and_fallback(void) {
    TEST_ASSERT_EQUAL_UINT32(120, HeatUpModel::computePreHeatLeadMinutes(5.0f, true));
    TEST_ASSERT_EQUAL_UINT32(210, HeatUpModel::computePreHeatLeadMinutes(-5.0f, true));
    TEST_ASSERT_EQUAL_UINT32(240, HeatUpModel::computePreHeatLeadMinutes(-15.0f, true));
    TEST_ASSERT_EQUAL_UINT32(45, HeatUpModel::computePreHeatLeadMinutes(20.0f, true));
    TEST_ASSERT_EQUAL_UINT32(90, HeatUpModel::computePreHeatLeadMinutes(-5.0f, false));
}

// FR-67: default rate 0.5 C/h; learns only after >= 60 min with rise > 0.2 C, clamped [0.1..2.0], EMA 80/20
void test_fr67_learned_rate_and_ema_update(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, model.getZoneRateDegPerHour(0));
    TEST_ASSERT_FALSE(model.isZoneLearned(0));

    // Short run (30 min < 60 min) must not learn
    model.evaluateZone(0, true, true, true, 20.0f, T0);
    model.evaluateZone(0, true, true, false, 20.8f, T0 + 1800000UL);
    TEST_ASSERT_FALSE(model.isZoneLearned(0));
    TEST_ASSERT_EQUAL_UINT16(0, model.getZoneSamples(0));

    // First valid 2h run: +1.0 C in 2h -> 0.5 C/h... use +1.6 C in 2h -> 0.8 C/h
    uint32_t t1 = T0 + 2000000UL;
    model.evaluateZone(0, true, true, true, 20.0f, t1);
    model.evaluateZone(0, true, true, false, 21.6f, t1 + 2 * ONE_HOUR_MS);
    TEST_ASSERT_TRUE(model.isZoneLearned(0));
    TEST_ASSERT_EQUAL_UINT16(1, model.getZoneSamples(0));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.8f, model.getZoneRateDegPerHour(0));

    // Second valid 1h run: +3.0 C in 1h -> clamped to 2.0 C/h -> EMA = 0.8*0.8 + 0.2*2.0 = 1.04 C/h
    uint32_t t2 = t1 + 3 * ONE_HOUR_MS;
    model.evaluateZone(0, true, true, true, 19.0f, t2);
    model.evaluateZone(0, true, true, false, 22.0f, t2 + ONE_HOUR_MS);
    TEST_ASSERT_EQUAL_UINT16(2, model.getZoneSamples(0));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.04f, model.getZoneRateDegPerHour(0));
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_fr48_preheat_lead_formula_and_fallback);
    RUN_TEST(test_fr67_learned_rate_and_ema_update);
    return UNITY_END();
}
