#include <time.h>
#include <unity.h>
#include "../sim/SimulatedHouse.h"

namespace {

constexpr uint32_t DAY_SEC = 86400UL;

}  // namespace

void setUp(void) {}
void tearDown(void) {}

// I-3: 7-day winter run completes in < 5 s on PC and keeps room within NFR-4.1 (+/-0.3 C) after day 1
void test_sim_7day_winter_steady_nfr4_1(void) {
    sim::SimRoom room;
    room.roomTemp = 18.0f;
    room.screedTemp = 18.0f;
    room.targetTemp = 22.0f;
    room.lastReportedTemp = 18.0f;

    ZoneController zc;
    sim::MemLearnedStore store;

    clock_t startClk = clock();
    sim::SimResult res = sim::runSimulation(
        7UL * DAY_SEC, room, zc, store,
        [](uint32_t sec, sim::SimRoom&, float& outdoorTemp, bool& weatherValid) {
            // Diurnal winter swing between -1 C and +5 C
            float phase = (float)(sec % DAY_SEC) / (float)DAY_SEC;
            outdoorTemp = 2.0f + 3.0f * sinf(phase * 6.2831853f);
            weatherValid = true;
        });
    float elapsedSec = (float)(clock() - startClk) / (float)CLOCKS_PER_SEC;

    TEST_ASSERT_LESS_THAN_FLOAT(5.0f, elapsedSec);
    TEST_ASSERT_LESS_OR_EQUAL_FLOAT(0.3f, res.maxAbsErrorAfterDay1);
}

// I-3 helper: cold start heats room up smoothly from 14 C toward 22 C
void test_sim_cold_start_convergence(void) {
    sim::SimRoom room;
    room.roomTemp = 14.0f;
    room.screedTemp = 14.0f;
    room.targetTemp = 22.0f;
    room.lastReportedTemp = 14.0f;

    ZoneController zc;
    sim::MemLearnedStore store;

    sim::SimResult res = sim::runSimulation(
        2UL * DAY_SEC, room, zc, store,
        [](uint32_t, sim::SimRoom&, float& outdoorTemp, bool& weatherValid) {
            outdoorTemp = 0.0f;
            weatherValid = true;
        });

    TEST_ASSERT_GREATER_THAN_FLOAT(21.7f, res.finalTemp);
    TEST_ASSERT_LESS_OR_EQUAL_FLOAT(0.3f, res.maxAbsErrorAfterDay1);
}

// I-3 helper: silent sensor keeps fallback heating active (no room freeze)
void test_sim_sensor_silent_keeps_heating(void) {
    sim::SimRoom room;
    room.roomTemp = 22.0f;
    room.screedTemp = 23.0f;
    room.targetTemp = 22.0f;
    room.lastReportedTemp = 22.0f;

    ZoneController zc;
    sim::MemLearnedStore store;

    sim::SimResult res = sim::runSimulation(
        6UL * 3600UL, room, zc, store,
        [](uint32_t sec, sim::SimRoom& r, float& outdoorTemp, bool& weatherValid) {
            outdoorTemp = 0.0f;
            weatherValid = true;
            r.sensorSilent = (sec >= 3600UL);
        });

    TEST_ASSERT_GREATER_THAN_UINT32(3000UL, res.valveOpenSec);
    TEST_ASSERT_GREATER_THAN_FLOAT(19.0f, res.minTempOverall);
}

// I-3 helper: open window cools room sharply (> 1 C in 10 min) during steady winter operation
void test_sim_open_window_drop(void) {
    sim::SimRoom room;
    room.roomTemp = 22.0f;
    room.screedTemp = 23.0f;
    room.targetTemp = 22.0f;
    room.lastReportedTemp = 22.0f;

    ZoneController zc;
    sim::MemLearnedStore store;

    float tempBeforeWindow = 0.0f;
    float tempAtWindowEnd = 0.0f;

    sim::runSimulation(
        3UL * 3600UL, room, zc, store,
        [&](uint32_t sec, sim::SimRoom& r, float& outdoorTemp, bool& weatherValid) {
            outdoorTemp = 0.0f;
            weatherValid = true;
            if (sec == 7200UL) tempBeforeWindow = r.roomTemp;
            r.windowOpen = (sec >= 7200UL && sec < 7800UL); // 10 min open window
            if (sec == 7800UL) tempAtWindowEnd = r.roomTemp;
        });

    TEST_ASSERT_GREATER_THAN_FLOAT(1.0f, tempBeforeWindow - tempAtWindowEnd);
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_sim_7day_winter_steady_nfr4_1);
    RUN_TEST(test_sim_cold_start_convergence);
    RUN_TEST(test_sim_sensor_silent_keeps_heating);
    RUN_TEST(test_sim_open_window_drop);
    return UNITY_END();
}
