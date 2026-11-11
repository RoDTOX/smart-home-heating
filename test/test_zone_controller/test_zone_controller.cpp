#include <unity.h>
#include "ZoneController.h"

namespace {

class FakeLearnedStore final : public ILearnedDutyStore {
public:
    int16_t values[ZoneController::MAX_ZONES] = {0};
    int saveCount = 0;
    size_t lastSavedIndex = 0;
    int16_t lastSavedVal = 0;

    int16_t learned(size_t index) const override {
        return index < ZoneController::MAX_ZONES ? values[index] : 0;
    }

    void saveLearned(size_t index, int16_t perMille) override {
        if (index < ZoneController::MAX_ZONES) {
            values[index] = perMille;
        }
        saveCount++;
        lastSavedIndex = index;
        lastSavedVal = perMille;
    }
};

constexpr uint32_t TPI_CYCLE_MS = 3600000UL;   // FR-47: 60 min TPI cycle
constexpr uint32_t MIN_VALVE_MS = 600000UL;    // FR-47: 10 min min ON / OFF
constexpr uint32_t T0 = 1000000UL;             // non-zero start time so cycleStartMs != 0

FakeLearnedStore fakeStore;
ZoneController zc;

}  // namespace

void setUp(void) {
    fakeStore = FakeLearnedStore();
    zc = ZoneController();
    zc.begin(&fakeStore);
}

void tearDown(void) {}

// AR-12: 7 outdoor temperature bands of 5 C: < -10, [-10,-5), [-5,0), [0,5), [5,10), [10,15), >= 15
void test_outdoor_band_boundaries_ar12(void) {
    TEST_ASSERT_EQUAL_UINT8(0, ZoneController::getOutdoorBand(-10.1f));
    TEST_ASSERT_EQUAL_UINT8(1, ZoneController::getOutdoorBand(-10.0f));
    TEST_ASSERT_EQUAL_UINT8(1, ZoneController::getOutdoorBand(-5.1f));
    TEST_ASSERT_EQUAL_UINT8(2, ZoneController::getOutdoorBand(-5.0f));
    TEST_ASSERT_EQUAL_UINT8(2, ZoneController::getOutdoorBand(-0.1f));
    TEST_ASSERT_EQUAL_UINT8(3, ZoneController::getOutdoorBand(0.0f));
    TEST_ASSERT_EQUAL_UINT8(3, ZoneController::getOutdoorBand(4.9f));
    TEST_ASSERT_EQUAL_UINT8(4, ZoneController::getOutdoorBand(5.0f));
    TEST_ASSERT_EQUAL_UINT8(4, ZoneController::getOutdoorBand(9.9f));
    TEST_ASSERT_EQUAL_UINT8(5, ZoneController::getOutdoorBand(10.0f));
    TEST_ASSERT_EQUAL_UINT8(5, ZoneController::getOutdoorBand(14.9f));
    TEST_ASSERT_EQUAL_UINT8(6, ZoneController::getOutdoorBand(15.0f));
    TEST_ASSERT_EQUAL_UINT8(6, ZoneController::getOutdoorBand(25.0f));
}

// FR-47: when (target - room) >= 0.4 C, duty = 100% (continuous heating, full 60 min cycle)
void test_fr47_deficit_rule_full_duty(void) {
    bool demand = zc.evaluateTpi(0, 21.59f, 22.0f, T0, 15.0f, true, 0.3f);
    TEST_ASSERT_TRUE(demand);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, zc.getDuty(0));
    TEST_ASSERT_EQUAL_UINT32(TPI_CYCLE_MS, zc.getOnMs(0));
}

// FR-47: duty = err / 1.5 + outdoorComp + baseDuty; outdoorComp = min(0.30, (15 - outdoor) * 0.015)
void test_fr47_proportional_plus_outdoor_comp(void) {
    // err = 0.3 C -> 0.3 / 1.5 = 0.20; outdoor = 5 C -> (15 - 5)*0.015 = 0.15; total = 0.35 (21 min)
    bool demand = zc.evaluateTpi(0, 21.7f, 22.0f, T0, 5.0f, true, 0.3f);
    TEST_ASSERT_TRUE(demand);
    TEST_ASSERT_FLOAT_WITHIN(0.005f, 0.35f, zc.getDuty(0));
    TEST_ASSERT_UINT32_WITHIN(5UL, (uint32_t)(0.35f * TPI_CYCLE_MS), zc.getOnMs(0));

    // Still ON before onMs, OFF once elapsed >= onMs within the 60 min cycle
    uint32_t onMs = zc.getOnMs(0);
    TEST_ASSERT_TRUE(zc.evaluateTpi(0, 21.7f, 22.0f, T0 + onMs - 1, 5.0f, true, 0.3f));
    TEST_ASSERT_FALSE(zc.evaluateTpi(0, 21.7f, 22.0f, T0 + onMs, 5.0f, true, 0.3f));
}

// FR-47: outdoor compensation clamped at 0.30 for very cold temperatures (< -5 C)
void test_fr47_outdoor_comp_clamped_and_missing_weather(void) {
    // outdoor = -15 C -> (15 - (-15))*0.015 = 0.45 -> clamped to 0.30; err = 0.0 -> duty = 0.30
    bool d1 = zc.evaluateTpi(0, 22.0f, 22.0f, T0, -15.0f, true, 0.3f);
    TEST_ASSERT_TRUE(d1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.30f, zc.getDuty(0));

    // When weatherValid = false, outdoorComp = 0 -> err = 0.0 -> duty = 0.0
    bool d2 = zc.evaluateTpi(1, 22.0f, 22.0f, T0, -15.0f, false, 0.3f);
    TEST_ASSERT_FALSE(d2);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, zc.getDuty(1));
}

// FR-47: minimum valve ON time and OFF time 10 min each
void test_fr47_min_valve_on_and_off_clamps(void) {
    // duty = 0.15 / 1.5 = 0.10 (6 min < 10 min) -> clamped to onMs = 0
    bool dSmall = zc.evaluateTpi(0, 21.85f, 22.0f, T0, 15.0f, true, 0.3f);
    TEST_ASSERT_FALSE(dSmall);
    TEST_ASSERT_EQUAL_UINT32(0, zc.getOnMs(0));

    // err = 0.3 C (0.20) + outdoor = -15 C (0.30) + learned 0.40 = 0.90 (54 min, OFF time 6 min < 10 min) -> 100% (60 min)
    zc.setLearnedDuty(1, 0, 0.40f);
    bool dHigh = zc.evaluateTpi(1, 21.7f, 22.0f, T0, -15.0f, true, 0.3f);
    TEST_ASSERT_TRUE(dHigh);
    TEST_ASSERT_EQUAL_UINT32(TPI_CYCLE_MS, zc.getOnMs(1));
}

// FR-47: overshoot cut stops active heating when room >= target + hysteresis
void test_fr47_overshoot_cut_stops_valve(void) {
    TEST_ASSERT_TRUE(zc.evaluateTpi(0, 21.7f, 22.0f, T0, 5.0f, true, 0.3f));
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(MIN_VALVE_MS, zc.getOnMs(0));

    // Room reaches target + hysteresis (22.3 C) 5 minutes into cycle -> cut immediately
    bool afterCut = zc.evaluateTpi(0, 22.3f, 22.0f, T0 + 300000UL, 5.0f, true, 0.3f);
    TEST_ASSERT_FALSE(afterCut);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, zc.getDuty(0));
}

// FR-47: new cycle starts when target changes by > 0.4 C; small <= 0.4 C change keeps running cycle
void test_fr47_target_shift_restarts_cycle(void) {
    // Keep room = 22.2 C so err stays < 0.4 C (avoids deficit rule masking target-shift check)
    zc.evaluateTpi(0, 22.2f, 22.0f, T0, 5.0f, true, 0.3f);
    uint32_t t1 = T0 + 60000UL;
    // Target moves by 0.3 C (<= 0.4 C): keeps same cycleStartMs (elapsed = 60000 ms)
    zc.evaluateTpi(0, 22.2f, 22.3f, t1, 5.0f, true, 0.3f);
    TEST_ASSERT_EQUAL_UINT32(60000UL, zc.getCycleElapsedMs(0, t1));

    // Target moves by 0.5 C from cycle's target 22.0 (> 0.4 C): restarts cycle (elapsed = 0)
    uint32_t t2 = T0 + 120000UL;
    zc.evaluateTpi(0, 22.2f, 22.5f, t2, 5.0f, true, 0.3f);
    TEST_ASSERT_EQUAL_UINT32(0UL, zc.getCycleElapsedMs(0, t2));
}

// FR-47 & NFR-2.9: learned base duty bounded 0..60%, saved to store only when delta >= 5%
void test_fr47_learned_duty_nvs_write_on_5pct_change(void) {
    fakeStore.values[0] = 200;  // 0.20 (20%) saved in store
    zc.begin(&fakeStore);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.20f, zc.getLearnedDuty(0, 3));

    // Start cycle with outdoor 20 C (band 6), err = 0.3 C (< 1.0 C)
    zc.evaluateTpi(0, 21.7f, 22.0f, T0, 20.0f, true, 0.3f);
    TEST_ASSERT_EQUAL_INT(0, fakeStore.saveCount);

    // Cycle 1 completes with err = 0.3 C -> delta = 0.05 * 0.3 = 0.015 (< 0.05) -> no NVS save
    zc.evaluateTpi(0, 21.7f, 22.0f, T0 + TPI_CYCLE_MS, 20.0f, true, 0.3f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.215f, zc.getLearnedDuty(0, 6));
    TEST_ASSERT_EQUAL_INT(0, fakeStore.saveCount);

    // Three more cycles bring learnedBands[6] to 0.260 (delta >= 0.05 from saved 0.20) -> 1 save
    zc.evaluateTpi(0, 21.7f, 22.0f, T0 + 2 * TPI_CYCLE_MS, 20.0f, true, 0.3f);
    zc.evaluateTpi(0, 21.7f, 22.0f, T0 + 3 * TPI_CYCLE_MS, 20.0f, true, 0.3f);
    TEST_ASSERT_EQUAL_INT(0, fakeStore.saveCount);  // 0.245 < 0.250
    zc.evaluateTpi(0, 21.7f, 22.0f, T0 + 4 * TPI_CYCLE_MS, 20.0f, true, 0.3f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.260f, zc.getLearnedDuty(0, 6));
    TEST_ASSERT_EQUAL_INT(1, fakeStore.saveCount);
    TEST_ASSERT_EQUAL_INT16(260, fakeStore.lastSavedVal);
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_outdoor_band_boundaries_ar12);
    RUN_TEST(test_fr47_deficit_rule_full_duty);
    RUN_TEST(test_fr47_proportional_plus_outdoor_comp);
    RUN_TEST(test_fr47_outdoor_comp_clamped_and_missing_weather);
    RUN_TEST(test_fr47_min_valve_on_and_off_clamps);
    RUN_TEST(test_fr47_overshoot_cut_stops_valve);
    RUN_TEST(test_fr47_target_shift_restarts_cycle);
    RUN_TEST(test_fr47_learned_duty_nvs_write_on_5pct_change);
    return UNITY_END();
}
