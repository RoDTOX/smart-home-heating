#include <unity.h>
#include "ModeManager.h"

void setUp() {}
void tearDown() {}

// FR-52: MANUAL mode expires back to AUTO after manualTimeout; manualRemainingSec counts down.
void test_fr52_manual_timeout_and_remaining() {
    ModeManager mm;
    mm.loadState(true, 1000, true, 0, 0, 0);
    TEST_ASSERT_TRUE(mm.isAutoMode());
    TEST_ASSERT_EQUAL_UINT32(0, mm.manualRemainingSec(5000, 14400));

    TEST_ASSERT_TRUE(mm.setAutoMode(false, 10000));
    TEST_ASSERT_FALSE(mm.isAutoMode());
    TEST_ASSERT_EQUAL_UINT32(14400, mm.manualRemainingSec(10000, 14400));
    TEST_ASSERT_EQUAL_UINT32(14390, mm.manualRemainingSec(20000, 14400));

    ModeTickResult r1 = mm.tick({10000 + 14399UL * 1000UL, 14400, 0, false});
    TEST_ASSERT_FALSE(r1.manualTimedOut);
    TEST_ASSERT_FALSE(mm.isAutoMode());

    ModeTickResult r2 = mm.tick({10000 + 14400UL * 1000UL, 14400, 0, false});
    TEST_ASSERT_TRUE(r2.manualTimedOut);
    TEST_ASSERT_TRUE(mm.isAutoMode());
    TEST_ASSERT_EQUAL_UINT32(0, mm.manualRemainingSec(10000 + 14400UL * 1000UL, 14400));
}

// FR-64: effectiveMode combines autoMode and seasonHeating (Manual / Auto / Off).
void test_fr64_effective_mode_auto_manual_off() {
    ModeManager mm;
    mm.loadState(true, 0, true, 0, 0, 0);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)EffMode::Auto, (uint8_t)mm.effectiveMode());

    mm.setSeasonMode(2);  // force SUMMER -> global OFF
    TEST_ASSERT_FALSE(mm.isSeasonHeating());
    TEST_ASSERT_EQUAL_UINT8((uint8_t)EffMode::Off, (uint8_t)mm.effectiveMode());

    mm.setAutoMode(false, 1000);  // MANUAL overrides global OFF for manual actions
    TEST_ASSERT_EQUAL_UINT8((uint8_t)EffMode::Manual, (uint8_t)mm.effectiveMode());

    mm.setSeasonMode(1);  // force HEATING
    TEST_ASSERT_TRUE(mm.isSeasonHeating());
    mm.setAutoMode(true, 2000);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)EffMode::Auto, (uint8_t)mm.effectiveMode());
}

// FR-12.2: Season switching on consecutive cold/warm days, override modes, and 12h safety net.
void test_fr12_2_season_days_and_safety_net() {
    ModeManager mm;
    mm.loadState(true, 0, false, 0, 0, 0);  // start SUMMER, seasonMode=0 (AUTO)

    // Need 3 consecutive days < 15.0 C (heatLimit); a warm day resets streak.
    TEST_ASSERT_FALSE(mm.seasonNewDay(14.5f, 0, 15.0f, 3, 5).changed);
    TEST_ASSERT_EQUAL_UINT8(1, mm.daysOn());
    TEST_ASSERT_FALSE(mm.seasonNewDay(15.2f, 0, 15.0f, 3, 5).changed);
    TEST_ASSERT_EQUAL_UINT8(0, mm.daysOn());

    TEST_ASSERT_FALSE(mm.seasonNewDay(14.0f, 0, 15.0f, 3, 5).changed);
    TEST_ASSERT_FALSE(mm.seasonNewDay(13.5f, 0, 15.0f, 3, 5).changed);
    SeasonChange scOn = mm.seasonNewDay(13.0f, 0, 15.0f, 3, 5);
    TEST_ASSERT_TRUE(scOn.changed);
    TEST_ASSERT_TRUE(scOn.heating);
    TEST_ASSERT_TRUE(mm.isSeasonHeating());
    TEST_ASSERT_EQUAL_UINT8(0, mm.daysOn());

    // Need 5 consecutive days > heatLimit + 2.0 (17.0 C) to switch back to SUMMER.
    TEST_ASSERT_FALSE(mm.seasonNewDay(16.9f, 0, 15.0f, 3, 5).changed);
    TEST_ASSERT_EQUAL_UINT8(0, mm.daysOff());
    for (int d = 0; d < 4; d++) {
        TEST_ASSERT_FALSE(mm.seasonNewDay(17.5f, 0, 15.0f, 3, 5).changed);
    }
    SeasonChange scOff = mm.seasonNewDay(18.0f, 0, 15.0f, 3, 5);
    TEST_ASSERT_TRUE(scOff.changed);
    TEST_ASSERT_FALSE(scOff.heating);
    TEST_ASSERT_FALSE(mm.isSeasonHeating());

    // seasonMode != 0 ignores weather.
    TEST_ASSERT_FALSE(mm.seasonNewDay(5.0f, 2, 15.0f, 1, 5).changed);
    TEST_ASSERT_FALSE(mm.isSeasonHeating());

    // 12h safety net while in SUMMER + seasonMode=0: any zone below night level for 12h turns HEATING on.
    ModeTickResult rStart = mm.tick({1000, 14400, 0, true});
    TEST_ASSERT_FALSE(rStart.seasonChanged.changed);
    ModeTickResult rMid = mm.tick({1000 + 11UL * 3600UL * 1000UL, 14400, 0, true});
    TEST_ASSERT_FALSE(rMid.seasonChanged.changed);
    ModeTickResult r12h = mm.tick({1000 + 12UL * 3600UL * 1000UL, 14400, 0, true});
    TEST_ASSERT_TRUE(r12h.seasonChanged.changed);
    TEST_ASSERT_TRUE(r12h.seasonChanged.heating);
    TEST_ASSERT_TRUE(mm.isSeasonHeating());
}

// FR-39: Boost ends after boostDuration seconds regardless of wall-clock sync.
void test_fr39_boost_expiration() {
    TEST_ASSERT_FALSE(ModeManager::isBoostExpired(false, 1000, 100000, 3600));
    TEST_ASSERT_FALSE(ModeManager::isBoostExpired(true, 1000, 1000 + 3599UL * 1000UL, 3600));
    TEST_ASSERT_TRUE(ModeManager::isBoostExpired(true, 1000, 1000 + 3600UL * 1000UL, 3600));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_fr52_manual_timeout_and_remaining);
    RUN_TEST(test_fr64_effective_mode_auto_manual_off);
    RUN_TEST(test_fr12_2_season_days_and_safety_net);
    RUN_TEST(test_fr39_boost_expiration);
    return UNITY_END();
}
