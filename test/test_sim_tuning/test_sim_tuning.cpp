// Comfort and overshoot of the zone control law on the simulated house (FR-47, FR-48, NFR-4.1).
// Each scenario runs the real ZoneController against the 2-node room of test/sim/SimulatedHouse.h and prints one
// line of figures (pio test -e native -f test_sim_tuning -v); the asserts hold only what a requirement says.
#include <stdio.h>
#include <unity.h>
#include "../sim/SimulatedHouse.h"
#include "HeatUpModel.h"

namespace {

constexpr uint32_t DAY = 86400UL;
constexpr uint32_t H = 3600UL;

struct House {
    const char* name;
    float floorAirTauSec, houseLossTauSec, screedCapRatio, powerDegPerHour;
};
// reference: the room of SimulatedHouse.h; light: thin screed, weak insulation; heavy: thick screed, good insulation
const House REFERENCE = {"reference", 3600.0f, 72000.0f, 3.0f, 3.6f};
const House LIGHT = {"light", 1800.0f, 40000.0f, 1.5f, 4.5f};
const House HEAVY = {"heavy", 7200.0f, 110000.0f, 6.0f, 3.0f};

struct Metrics {
    float steadyOver = 0.0f;    // max room - target, 3 h or more after the last target change (NFR-4.1)
    float steadyUnder = 0.0f;   // max target - room, same window
    float riseOver = 0.0f;      // max room - target in the 3 h after a target rise (overshoot of a heat-up)
    float peakOver = 0.0f;      // max room - target over the whole run, first day included (cold start)
    float atMoment = -99.0f;    // room - target at the moment the schedule asks for the higher target (last day)
    float dutyPct = 0.0f;       // valve open share after day 1
    float switchesPerDay = 0.0f;
    uint32_t lateMin = 0;       // minutes after the moment until the room is within 0.3 C of the target (last day)
};

// How the controller starts early for a higher target: not at all, by the FR-48 formula (what the firmware does
// today), or by the heat-up rate that HeatUpModel learned for the zone (FR-67: lead = missing degrees / rate)
enum PreHeat { NONE, FORMULA, LEARNED };

// targetAt(sec) gives the comfort target the schedule asks for
template <typename OutFn, typename TargetFn>
Metrics run(const House& h, uint32_t days, float startTemp, OutFn outdoorAt, TargetFn targetAt, PreHeat preHeat) {
    sim::SimRoom room;
    room.floorAirTauSec = h.floorAirTauSec;
    room.houseLossTauSec = h.houseLossTauSec;
    room.screedCapRatio = h.screedCapRatio;
    room.maxHeatPowerPerSec = h.powerDegPerHour / 3600.0f;
    room.roomTemp = room.screedTemp = room.lastReportedTemp = startTemp;
    ZoneController zc;
    sim::MemLearnedStore store;
    zc.begin(&store);

    Metrics m;
    constexpr uint32_t T0 = 10000UL;
    float prevSched = targetAt(0);
    uint32_t lastChange = 0, lastRise = 0, openSec = 0, switches = 0, total = days * DAY;
    bool rose = false, prevValve = false, waiting = false, coasting = false, early = false;
    HeatUpModel hum;
    hum.begin(T0);
    for (uint32_t sec = 0; sec < total; ++sec) {
        float out = outdoorAt(sec);
        float sched = targetAt(sec);
        // the controller starts heating for the next higher target the pre-heat lead before its moment
        float ctl = sched;
        if (preHeat != NONE) {
            uint32_t lead = HeatUpModel::computePreHeatLeadMinutes(out, true);
            float ahead = sched;
            uint32_t minsTo = 0;
            for (uint32_t mnt = 1; mnt <= 240 && ahead <= sched; mnt++) { ahead = targetAt(sec + mnt * 60UL); minsTo = mnt; }
            if (preHeat == LEARNED && hum.isZoneLearned(0) && ahead > sched) {
                float need = (ahead - room.lastReportedTemp) / hum.getZoneRateDegPerHour(0) * 60.0f;
                lead = need <= 0.0f ? 0 : (need > 240.0f ? 240 : (uint32_t)need);
            }
            if (ahead > sched && (early || minsTo <= lead)) { ctl = ahead; early = true; }
        }
        if (sec % 10 == 0) hum.evaluateZone(0, true, true, room.valveOpen, room.lastReportedTemp, T0 + sec * 1000UL);
        if (sched != prevSched) {
            lastChange = sec;
            early = false;
            coasting = sched < prevSched;  // after a lower target the room cools by itself: no control error
            if (sched > prevSched) {
                lastRise = sec; rose = true;
                if (sec >= total - DAY) { m.atMoment = room.roomTemp - sched; waiting = true; }
            }
            prevSched = sched;
        }
        room.targetTemp = ctl;
        room.valveOpen = zc.evaluateTpi(0, room.lastReportedTemp, ctl, T0 + sec * 1000UL, out, true, 0.3f);
        room.step1s(out);

        if (waiting) {
            if (room.roomTemp >= sched - 0.3f) waiting = false;
            else if (sec % 60 == 0) m.lateMin++;
        }
        if (coasting && room.roomTemp - sched <= 0.3f) coasting = false;
        if (!coasting && room.roomTemp - sched > m.peakOver) m.peakOver = room.roomTemp - sched;
        if (sec < DAY) continue;
        if (room.valveOpen) openSec++;
        if (room.valveOpen != prevValve) switches++;
        prevValve = room.valveOpen;
        float err = room.roomTemp - sched;
        bool steady = sec - lastChange >= 3 * H && ctl == sched && !coasting;
        if (steady) {
            if (err > m.steadyOver) m.steadyOver = err;
            if (-err > m.steadyUnder) m.steadyUnder = -err;
        }
        if (rose && sec - lastRise < 3 * H && err > m.riseOver) m.riseOver = err;
    }
    m.dutyPct = 100.0f * (float)openSec / (float)(total - DAY);
    m.switchesPerDay = (float)switches / (float)(days - 1);
    return m;
}

void report(const char* scenario, const House& h, const Metrics& m) {
    printf("SIM|%s|%s|steadyOver=%.2f|steadyUnder=%.2f|riseOver=%.2f|peakOver=%.2f|atMoment=%.2f|lateMin=%u|duty=%.0f|switches=%.1f\n",
           scenario, h.name, m.steadyOver, m.steadyUnder, m.riseOver, m.peakOver, m.atMoment, (unsigned)m.lateMin,
           m.dutyPct, m.switchesPerDay);
}

float constTarget(uint32_t) { return 22.0f; }
// a boost of FR-1.2: 22 C, raised by 2 C for 2 h every day at 18:00
float boostTarget(uint32_t sec) {
    uint32_t t = sec % DAY;
    return (t >= 18 * H && t < 20 * H) ? 24.0f : 22.0f;
}
// night 20 C from 22:30 to 06:30, day 23 C
float setbackTarget(uint32_t sec) {
    uint32_t t = sec % DAY;
    return (t >= 6 * H + 1800 && t < 22 * H + 1800) ? 23.0f : 20.0f;
}

const House* const HOUSES[] = {&REFERENCE, &LIGHT, &HEAVY};

}  // namespace

void setUp(void) {}
void tearDown(void) {}

// NFR-4.1: steady state within +/-0.3 C of the target, at a constant outdoor temperature in each outdoor band
void test_sim_steady_each_outdoor_band_nfr4_1(void) {
    const float outdoor[] = {-12.0f, -7.0f, -3.0f, 2.0f, 7.0f, 12.0f};
    for (float o : outdoor) {
        char name[32];
        snprintf(name, sizeof name, "steady%+.0f", o);
        Metrics m = run(REFERENCE, 6, 22.0f, [o](uint32_t) { return o; }, constTarget, NONE);
        report(name, REFERENCE, m);
        TEST_ASSERT_LESS_OR_EQUAL_FLOAT_MESSAGE(0.3f, m.steadyOver, name);
        TEST_ASSERT_LESS_OR_EQUAL_FLOAT_MESSAGE(0.3f, m.steadyUnder, name);
    }
}

// NFR-4.1 on other buildings (FR-68: no manual tuning after commissioning): figures printed for the report
void test_sim_steady_other_buildings(void) {
    for (const House* h : HOUSES) {
        Metrics m = run(*h, 6, 22.0f, [](uint32_t sec) {
            return 2.0f + 3.0f * sinf((float)(sec % DAY) / (float)DAY * 6.2831853f);
        }, constTarget, NONE);
        report("diurnal", *h, m);
        TEST_ASSERT_LESS_OR_EQUAL_FLOAT_MESSAGE(0.3f, m.steadyOver, h->name);
        TEST_ASSERT_LESS_OR_EQUAL_FLOAT_MESSAGE(0.3f, m.steadyUnder, h->name);
    }
}

// Overshoot of a heat-up: a cold house (14 C -> 22 C) and a daily 2 C boost; steady state after it holds NFR-4.1
void test_sim_cold_start_and_boost_overshoot(void) {
    for (const House* h : HOUSES) {
        Metrics cold = run(*h, 4, 14.0f, [](uint32_t) { return 0.0f; }, constTarget, NONE);
        report("coldstart", *h, cold);
        // a heavy house is still heating up after the first day, so only the overshoot side is a steady-state figure
        TEST_ASSERT_LESS_OR_EQUAL_FLOAT_MESSAGE(0.3f, cold.steadyOver, h->name);
        TEST_ASSERT_LESS_OR_EQUAL_FLOAT_MESSAGE(0.3f, cold.peakOver, h->name);
        report("boost", *h, run(*h, 4, 22.0f, [](uint32_t) { return 0.0f; }, boostTarget, NONE));
    }
}

// FR-48: night setback 20 C -> day 23 C at 06:30, with and without the pre-heat lead; figures printed for the report
void test_sim_setback_with_and_without_preheat(void) {
    const float outdoor[] = {-8.0f, 0.0f, 8.0f};
    for (const House* h : HOUSES) {
        for (float o : outdoor) {
            char name[40];
            snprintf(name, sizeof name, "setback%+.0f", o);
            report(name, *h, run(*h, 6, 21.0f, [o](uint32_t) { return o; }, setbackTarget, NONE));
            snprintf(name, sizeof name, "formula%+.0f", o);
            report(name, *h, run(*h, 6, 21.0f, [o](uint32_t) { return o; }, setbackTarget, FORMULA));
            snprintf(name, sizeof name, "learned%+.0f", o);
            report(name, *h, run(*h, 6, 21.0f, [o](uint32_t) { return o; }, setbackTarget, LEARNED));
        }
    }
    TEST_PASS();
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_sim_steady_each_outdoor_band_nfr4_1);
    RUN_TEST(test_sim_steady_other_buildings);
    RUN_TEST(test_sim_cold_start_and_boost_overshoot);
    RUN_TEST(test_sim_setback_with_and_without_preheat);
    return UNITY_END();
}
