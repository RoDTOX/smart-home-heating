#include <unity.h>
#include "Schedule.h"

void setUp(void) {}
void tearDown(void) {}

// FR-29: factory comfort levels, bounds, slot parsing & sorting
void test_fr29_factory_levels_and_slots_parsing(void) {
    Schedule sch;
    TEST_ASSERT_TRUE(sch.isEnabled());
    TEST_ASSERT_TRUE(sch.isPreHeatEnabled());
    TEST_ASSERT_FALSE(sch.isAway());
    TEST_ASSERT_EQUAL_UINT8(SCHED_MODE_5_2, sch.getScheduleMode());

    const ScheduleConfig& cfg = sch.getConfig();
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 23.0f, cfg.levelTemps[LEVEL_DAY][0]);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 21.0f, cfg.levelTemps[LEVEL_NIGHT][0]);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 18.0f, cfg.levelTemps[LEVEL_AWAY][0]);

    // C-36 cap at TARGET_MAX_C (28.0 C) and floor at 5.0 C
    TEST_ASSERT_FALSE(sch.setLevelTemp(0, 3, LEVEL_DAY, 28.5f));
    TEST_ASSERT_FALSE(sch.setLevelTemp(0, 3, LEVEL_DAY, 4.5f));
    TEST_ASSERT_TRUE(sch.setLevelTemp(0, 3, LEVEL_DAY, 24.5f));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 24.5f, sch.getConfig().levelTemps[LEVEL_DAY][0]);

    // Unsorted slots are sorted chronologically
    TEST_ASSERT_TRUE(sch.setSlots(false, "22:30=night,06:30=day,12:00=away"));
    char buf[96];
    sch.formatSlots(false, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("06:30=day,12:00=away,22:30=night", buf);

    // Invalid slot specs rejected
    TEST_ASSERT_FALSE(sch.setSlots(false, "25:00=day"));
    TEST_ASSERT_FALSE(sch.setSlots(false, "06:00=bogus"));
}

// FR-30: slot evaluation across midnight wrap and slotChanged transition
void test_fr30_slot_evaluation_midnight_wrap_and_transition(void) {
    Schedule sch;
    // Before 06:30 on weekday -> carries over night (22:30 -> 06:30)
    ScheduleEvalInput in1{true, false, 5, 30, 1, 90};
    ScheduleEvalResult r1 = sch.evaluate(in1);
    TEST_ASSERT_TRUE(r1.evaluated);
    TEST_ASSERT_TRUE(r1.slotChanged);
    TEST_ASSERT_EQUAL_UINT8(LEVEL_NIGHT, sch.getActiveLevel());
    TEST_ASSERT_EQUAL_UINT8(LEVEL_DAY, sch.getNextLevel());
    TEST_ASSERT_EQUAL_UINT32(60, sch.getMinutesToNextSlot());

    // Same slot again -> no transition
    ScheduleEvalInput in2{true, false, 6, 0, 1, 90};
    ScheduleEvalResult r2 = sch.evaluate(in2);
    TEST_ASSERT_FALSE(r2.slotChanged);
    TEST_ASSERT_EQUAL_UINT32(30, sch.getMinutesToNextSlot());

    // At 06:30 -> transitions to DAY
    ScheduleEvalInput in3{true, false, 6, 30, 1, 90};
    ScheduleEvalResult r3 = sch.evaluate(in3);
    TEST_ASSERT_TRUE(r3.slotChanged);
    TEST_ASSERT_EQUAL_UINT8(6, r3.activeHour);
    TEST_ASSERT_EQUAL_UINT8(30, r3.activeMinute);
    TEST_ASSERT_EQUAL_UINT8(LEVEL_DAY, sch.getActiveLevel());
    TEST_ASSERT_EQUAL_UINT8(LEVEL_NIGHT, sch.getNextLevel());
    TEST_ASSERT_EQUAL_UINT32(16 * 60, sch.getMinutesToNextSlot());
}

// FR-31: 4-moment matrix (5+2, unified, 7day, constant) and setMoment / setMomentForGroup
void test_fr31_moment_matrix_and_modes(void) {
    Schedule sch;
    float temps[3] = {22.5f, 21.0f, 20.0f};
    TEST_ASSERT_TRUE(sch.setMomentForGroup("weekday", 1, 9, 0, temps, 3));

    // At 10:00 on Wednesday (dayOfWeek = 3), active moment is 1 (09:00)
    sch.setScheduleMode(SCHED_MODE_5_2);
    sch.evaluate({true, false, 10, 0, 3, 60});
    TEST_ASSERT_EQUAL_UINT8(1, sch.getActiveMomentIdx());
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 22.5f, sch.getTargetTemp(0, true, 3, 19.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 21.0f, sch.getTargetTemp(1, true, 3, 19.0f));

    // Constant mode always uses moment 0
    sch.setScheduleMode(SCHED_MODE_CONSTANT);
    sch.evaluate({true, false, 10, 0, 3, 60});
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 23.0f, sch.getTargetTemp(0, true, 3, 19.0f));

    // 7day mode distinguishes Sunday (0) vs Saturday (6)
    float satTemps[3] = {24.0f, 24.0f, 24.0f};
    TEST_ASSERT_TRUE(sch.setMoment(6, 1, 11, 0, satTemps, 3));
    sch.setScheduleMode(SCHED_MODE_7DAY);
    sch.evaluate({true, true, 12, 0, 6, 60});
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 24.0f, sch.getTargetTemp(0, true, 6, 19.0f));
}

// FR-48: predictive pre-heat activates inside lead window when next level is warmer
void test_fr48_preheat_activation_and_target(void) {
    Schedule sch;
    sch.setScheduleMode(SCHED_MODE_5_2);
    // Weekday 05:15 -> 75 min before 06:30 DAY (23.0 C > 21.0 C NIGHT)
    // With lead = 90 min, preHeat must activate and target the next moment/level (23.0 C)
    sch.evaluate({true, false, 5, 15, 2, 90});
    TEST_ASSERT_TRUE(sch.isPreHeatActive());
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 23.0f, sch.getTargetTemp(0, true, 2, 19.0f));

    // With lead = 60 min (75 > 60), preHeat stays OFF and target stays NIGHT (21.0 C)
    sch.evaluate({true, false, 5, 15, 2, 60});
    TEST_ASSERT_FALSE(sch.isPreHeatActive());
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 21.0f, sch.getTargetTemp(0, true, 2, 19.0f));
}

// FR-61: without synced time or when disabled, fallback to staticTarget; away overrides slots
void test_fr61_fallback_without_time_and_away_override(void) {
    Schedule sch;
    ScheduleEvalResult rNoTime = sch.evaluate({false, false, 10, 0, 1, 90});
    TEST_ASSERT_FALSE(rNoTime.evaluated);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 20.5f, sch.getTargetTemp(0, false, 1, 20.5f));

    // Schedule disabled -> staticTarget even with synced time
    sch.setEnabled(false);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 19.5f, sch.getTargetTemp(0, true, 1, 19.5f));

    // Away override -> awayTemp (18.0 C) regardless of enabled/time
    sch.setAway(true);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 18.0f, sch.getTargetTemp(0, false, 1, 19.5f));
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_fr29_factory_levels_and_slots_parsing);
    RUN_TEST(test_fr30_slot_evaluation_midnight_wrap_and_transition);
    RUN_TEST(test_fr31_moment_matrix_and_modes);
    RUN_TEST(test_fr48_preheat_activation_and_target);
    RUN_TEST(test_fr61_fallback_without_time_and_away_override);
    return UNITY_END();
}
