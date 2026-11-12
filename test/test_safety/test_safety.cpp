#include <unity.h>
#include "SafetySupervisor.h"
#include "ZoneController.h"
#include "../sim/SimulatedHouse.h"

namespace {

constexpr uint32_t INTERLOCK_MS = 180000UL;   // FR-2 default 180 s
constexpr uint32_t OVERRUN_MS = 180000UL;     // FR-3 default 180 s
constexpr float FROST_TEMP = 5.0f;            // FR-16.1 default 5.0 C
constexpr uint32_t T0 = 10000UL;

// Simulation-only boiler/burner driver model (NOT firmware BoilerRadioTrigger; FR-4 is tested on the board).
struct SimBurnerModel {
    static constexpr uint32_t MODEL_MIN_RUN_MS = 180000UL;
    static constexpr uint32_t MODEL_MIN_PAUSE_MS = 180000UL;
    bool boilerOn = false;
    bool boilerDemand = false;
    uint32_t lastOnMs = 0;
    uint32_t lastOffMs = T0;
    bool pendingOn = false;
    bool pendingOff = false;

    bool canSwitchOn(uint32_t now, const SafetySupervisor& sup, bool autoMode, bool openLoopBypass) const {
        if (boilerOn) return false;
        if (!sup.boilerOnPermitted(now, autoMode, openLoopBypass)) return false;
        return (uint32_t)(now - lastOffMs) >= MODEL_MIN_PAUSE_MS;
    }

    bool canSwitchOff(uint32_t now) const {
        if (!boilerOn) return false;
        return (uint32_t)(now - lastOnMs) >= MODEL_MIN_RUN_MS;
    }

    void step(uint32_t now, bool wantOn, const SafetySupervisor& sup, bool autoMode, bool openLoopBypass) {

        if (wantOn) {
            boilerDemand = true;
            pendingOff = false; // FR-32
            if (!boilerOn) {
                if (canSwitchOn(now, sup, autoMode, openLoopBypass)) {
                    boilerOn = true;
                    lastOnMs = now;
                    pendingOn = false;
                } else {
                    pendingOn = true;
                }
            }
        } else {
            if (boilerDemand || boilerOn) {
                boilerDemand = false;
                if (boilerOn) {
                    if (canSwitchOff(now)) {
                        boilerOn = false;
                        lastOffMs = now;
                        pendingOff = false;
                    } else {
                        pendingOff = true;
                    }
                } else {
                    // FR-3.2: demand dropped before ignition -> cancel pending ON cleanly
                    pendingOn = false;
                }
            }
        }

        // Service pending actions once guard timer expires
        if (pendingOn && !boilerOn && canSwitchOn(now, sup, autoMode, openLoopBypass)) {
            boilerOn = true;
            lastOnMs = now;
            pendingOn = false;
        }
        if (pendingOff && boilerOn && canSwitchOff(now)) {
            boilerOn = false;
            lastOffMs = now;
            pendingOff = false;
        }
    }
};

SafetySupervisor sup;

}  // namespace

void setUp(void) {
    sup = SafetySupervisor();
    sup.begin(INTERLOCK_MS, OVERRUN_MS, FROST_TEMP);
}

void tearDown(void) {}

// FR-2 & FR-1.3: boiler stays blocked before valve energised >= interlockDelay, except MANUAL openLoopBypass
void test_fr2_interlock_and_fr1_3_bypass(void) {
    TEST_ASSERT_FALSE(sup.boilerOnPermitted(T0, true, false));

    bool demand[2] = {true, false};
    bool frost[2] = {false, false};
    SafetySupervisor::ValveAction acts[2];
    bool anyReady = false, anyFrost = false;

    // t = T0: valve energises, NOT open yet
    sup.arbitrateValves(T0, false, demand, frost, 2, anyReady, anyFrost, acts);
    TEST_ASSERT_TRUE(sup.isValveEnergised(0));
    TEST_ASSERT_TRUE(acts[0].has(SafetySupervisor::VA_OPEN_CHANGED) && acts[0].has(SafetySupervisor::VA_OPEN_TARGET));
    TEST_ASSERT_FALSE(anyReady);
    TEST_ASSERT_FALSE(sup.boilerOnPermitted(T0, true, false));

    // t = T0 + 179s: still blocked
    sup.arbitrateValves(T0 + 179000UL, false, demand, frost, 2, anyReady, anyFrost, acts);
    TEST_ASSERT_FALSE(anyReady);
    TEST_ASSERT_FALSE(sup.boilerOnPermitted(T0 + 179000UL, true, false));

    // t = T0 + 180s: valve open -> boiler ON permitted
    sup.arbitrateValves(T0 + INTERLOCK_MS, false, demand, frost, 2, anyReady, anyFrost, acts);
    TEST_ASSERT_TRUE(anyReady);
    TEST_ASSERT_TRUE(acts[0].has(SafetySupervisor::VA_INTERLOCK_DONE));
    TEST_ASSERT_TRUE(sup.boilerOnPermitted(T0 + INTERLOCK_MS, true, false));

    // FR-1.3: MANUAL with openLoopBypass permits boiler without an open valve
    TEST_ASSERT_TRUE(sup.boilerOnPermitted(T0, false, true));
}

// FR-3 & FR-3.2: last open valve stays held while boiler ON/PENDING_OFF and for full overrun after flame OFF;
// cancelled start before ignition never triggers overrun.
void test_fr3_overrun_hold_and_fr3_2_cancelled_start(void) {
    bool demand[1] = {true};
    bool frost[1] = {false};
    SafetySupervisor::ValveAction acts[1];
    bool anyReady = false, anyFrost = false;

    // Start cycle with no lit flame (FR-3.2: demand drops before ignition)
    sup.trackBoilerState(T0, false);
    sup.arbitrateValves(T0, false, demand, frost, 1, anyReady, anyFrost, acts);
    sup.arbitrateValves(T0 + INTERLOCK_MS, false, demand, frost, 1, anyReady, anyFrost, acts);
    demand[0] = false;
    auto tr = sup.trackBoilerState(T0 + INTERLOCK_MS + 1000UL, false);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)SafetySupervisor::OverrunTransition::None, (uint8_t)tr);
    TEST_ASSERT_FALSE(sup.isOverrunActive());
    sup.arbitrateValves(T0 + INTERLOCK_MS + 1000UL, false, demand, frost, 1, anyReady, anyFrost, acts);
    TEST_ASSERT_FALSE(sup.isValveEnergised(0)); // Closes immediately since flame was never lit

    // Now simulate lit flame turning OFF -> overrun starts and keeper valve stays open
    demand[0] = true;
    uint32_t t1 = T0 + 500000UL;
    sup.arbitrateValves(t1, false, demand, frost, 1, anyReady, anyFrost, acts);
    sup.arbitrateValves(t1 + INTERLOCK_MS, false, demand, frost, 1, anyReady, anyFrost, acts);
    sup.trackBoilerState(t1 + INTERLOCK_MS, true); // Flame lit

    // Demand drops while boiler still ON (e.g. PENDING_OFF) -> valve MUST stay held open
    demand[0] = false;
    uint32_t tDrop = t1 + INTERLOCK_MS + 10000UL;
    sup.arbitrateValves(tDrop, true, demand, frost, 1, anyReady, anyFrost, acts);
    TEST_ASSERT_TRUE(sup.isValveEnergised(0));
    TEST_ASSERT_TRUE(acts[0].has(SafetySupervisor::VA_HOLD_STARTED));

    // Flame turns OFF -> overrun active for 180s -> keeper remains energised until tOff + 180s
    uint32_t tOff = tDrop + 10000UL;
    tr = sup.trackBoilerState(tOff, false);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)SafetySupervisor::OverrunTransition::Started, (uint8_t)tr);
    TEST_ASSERT_TRUE(sup.isOverrunActive());

    sup.arbitrateValves(tOff + OVERRUN_MS - 1000UL, false, demand, frost, 1, anyReady, anyFrost, acts);
    TEST_ASSERT_TRUE(sup.isValveEnergised(0));

    tr = sup.trackBoilerState(tOff + OVERRUN_MS, false);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)SafetySupervisor::OverrunTransition::Finished, (uint8_t)tr);
    TEST_ASSERT_FALSE(sup.isOverrunActive());
    sup.arbitrateValves(tOff + OVERRUN_MS, false, demand, frost, 1, anyReady, anyFrost, acts);
    TEST_ASSERT_FALSE(sup.isValveEnergised(0));
    TEST_ASSERT_TRUE(acts[0].has(SafetySupervisor::VA_OPEN_CHANGED) &&
                     !acts[0].has(SafetySupervisor::VA_OPEN_TARGET) &&
                     acts[0].has(SafetySupervisor::VA_OVERRUN_END));
}

// FR-16.1: frost guard activates < 5.0 C and releases >= 6.5 C (hysteresis +1.5 C)
void test_fr16_1_frost_hysteresis(void) {
    auto r0 = sup.evaluateFrost(false, true, 5.0f);
    TEST_ASSERT_FALSE(r0.frostActive);

    auto r1 = sup.evaluateFrost(false, true, 4.9f);
    TEST_ASSERT_TRUE(r1.frostActive);
    TEST_ASSERT_TRUE(r1.becameActive);

    // Still active at 6.4 C (< 5.0 + 1.5)
    auto r2 = sup.evaluateFrost(true, true, 6.4f);
    TEST_ASSERT_TRUE(r2.frostActive);
    TEST_ASSERT_FALSE(r2.becameInactive);

    // Releases at 6.5 C
    auto r3 = sup.evaluateFrost(true, true, 6.5f);
    TEST_ASSERT_FALSE(r3.frostActive);
    TEST_ASSERT_TRUE(r3.becameInactive);
}

// Multi-zone overrun keeper handoff, onZoneRemoved compaction, and millis() wrap-around safety (FR-2, FR-3)
void test_multizone_keeper_delete_and_wraparound(void) {
    bool demand[3] = {true, true, false};
    bool frost[3] = {false, false, false};
    SafetySupervisor::ValveAction acts[3];
    bool anyReady = false, anyFrost = false;

    // Cross UINT32_MAX during interlockDelay (start at 0xFFFF0000 -> +180s wraps past 0)
    const uint32_t tWrap0 = 0xFFFF0000UL;
    sup.arbitrateValves(tWrap0, false, demand, frost, 3, anyReady, anyFrost, acts);
    TEST_ASSERT_FALSE(anyReady);

    const uint32_t tAfter100s = tWrap0 + 100000UL;  // wrapped past 0!
    sup.arbitrateValves(tAfter100s, false, demand, frost, 3, anyReady, anyFrost, acts);
    TEST_ASSERT_FALSE(anyReady);

    const uint32_t tOpen = tWrap0 + INTERLOCK_MS;
    sup.arbitrateValves(tOpen, false, demand, frost, 3, anyReady, anyFrost, acts);
    TEST_ASSERT_TRUE(anyReady);
    TEST_ASSERT_EQUAL_INT8(1, sup.getOverrunKeeperZone());

    // Flame lit; Z0 drops demand while Z1 stays open -> Z0 MUST close immediately (not held), Z1 stays open
    sup.trackBoilerState(tOpen, true);
    demand[0] = false;
    sup.arbitrateValves(tOpen + 5000UL, true, demand, frost, 3, anyReady, anyFrost, acts);
    TEST_ASSERT_FALSE(sup.isValveEnergised(0));
    TEST_ASSERT_TRUE(sup.isValveEnergised(1));
    TEST_ASSERT_EQUAL_INT8(1, sup.getOverrunKeeperZone());

    // Zone 0 deleted -> keeper index 1 shifts to 0 cleanly and stays valid
    sup.onZoneRemoved(0);
    TEST_ASSERT_EQUAL_INT8(0, sup.getOverrunKeeperZone());
    TEST_ASSERT_TRUE(sup.isValveEnergised(0));

    // Remaining zone (now index 0) drops demand -> flame OFF triggers wrap-safe overrun hold
    bool demand2[2] = {false, false};
    bool frost2[2] = {false, false};
    SafetySupervisor::ValveAction acts2[2];
    const uint32_t tFlameOff = 0xFFFFFF80UL; // wraps past 0 during overrun
    sup.trackBoilerState(tFlameOff, false);
    TEST_ASSERT_TRUE(sup.isOverrunActive());
    sup.arbitrateValves(tFlameOff, false, demand2, frost2, 2, anyReady, anyFrost, acts2);
    TEST_ASSERT_TRUE(sup.isValveEnergised(0));
    sup.arbitrateValves(tFlameOff + OVERRUN_MS - 1000UL, false, demand2, frost2, 2, anyReady, anyFrost, acts2);
    TEST_ASSERT_TRUE(sup.isValveEnergised(0));

    sup.trackBoilerState(tFlameOff + OVERRUN_MS, false);
    TEST_ASSERT_FALSE(sup.isOverrunActive());
    sup.arbitrateValves(tFlameOff + OVERRUN_MS, false, demand2, frost2, 2, anyReady, anyFrost, acts2);
    TEST_ASSERT_FALSE(sup.isValveEnergised(0));
}

// I-4 requirement: check safety invariants (I-1, I-2, I-3, I-7, FR-16.1, FR-51) on EVERY simulated second
void test_safety_invariants_every_simulated_second(void) {
    sim::SimRoom rooms[2];
    rooms[0].roomTemp = 16.0f; // Cold winter start -> TPI transitions through demand/no-demand/fault
    rooms[0].screedTemp = 16.0f;
    rooms[0].targetTemp = 22.0f;
    rooms[0].lastReportedTemp = 16.0f;

    rooms[1].roomTemp = 3.5f;  // Below FR-16.1 frostTemp 5.0 C -> exercises frost hysteresis (<5.0 ON -> >=6.5 OFF)
    rooms[1].screedTemp = 4.0f;
    rooms[1].targetTemp = 5.0f; // Would NOT call normal heat above frost release unless frost guard holds it
    rooms[1].lastReportedTemp = 3.5f;

    ZoneController zc;
    sim::MemLearnedStore store;
    zc.begin(&store);

    sup.begin(INTERLOCK_MS, OVERRUN_MS, FROST_TEMP);

    SimBurnerModel guard;

    bool prevBoilerOn = false;
    bool frostState[2] = {false, false};
    bool sawFrostEntered = false, sawFrostExited = false, sawFaultDutyDemand = false;

    // Simulate 24 hours at 1-second resolution including a sensor-loss window on zone 0 (hours 12..15)
    for (uint32_t sec = 0; sec < 86400UL; ++sec) {
        uint32_t nowMs = T0 + sec * 1000UL;
        float outdoorTemp = -2.0f;
        bool sensorLost0 = (sec >= 43200UL && sec < 54000UL);

        bool demand[2] = {false, false};
        if (sensorLost0) {
            demand[0] = zc.evaluateFaultDuty(0, nowMs, outdoorTemp, true, true, 20.0f, 5.0f);
            if (demand[0]) sawFaultDutyDemand = true;
        } else {
            demand[0] = zc.evaluateTpi(0, rooms[0].lastReportedTemp, rooms[0].targetTemp, nowMs, outdoorTemp, true, 0.3f);
        }
        // Zone 1 only calls normal heat after hour 1 (frost warms it first)
        if (sec >= 3600UL) {
            demand[1] = zc.evaluateTpi(1, rooms[1].lastReportedTemp, rooms[1].targetTemp, nowMs, outdoorTemp, true, 0.3f);
        }

        // Persistent FR-16.1 state per zone
        bool frost[2] = {false, false};
        for (size_t z = 0; z < 2; ++z) {
            bool valid = (z == 0) ? !sensorLost0 : true;
            auto fr = sup.evaluateFrost(frostState[z], valid, rooms[z].lastReportedTemp);
            frostState[z] = fr.frostActive;
            frost[z] = fr.frostActive;
            if (fr.becameActive) sawFrostEntered = true;
            if (fr.becameInactive) sawFrostExited = true;
            if (frost[z]) demand[z] = true;
        }

        sup.trackBoilerState(nowMs, guard.boilerOn);
        bool anyReady = false, anyFrost = false;
        SafetySupervisor::ValveAction acts[2];
        sup.arbitrateValves(nowMs, guard.boilerOn, demand, frost, 2, anyReady, anyFrost, acts);

        for (size_t z = 0; z < 2; ++z) {
            // Invariant I-3 (FR-37, FR-3): valve energised ONLY on demand or overrun keeper hold
            if (sup.isValveEnergised(z)) {
                bool overrunHold = (sup.getOverrunKeeperZone() == (int8_t)z) && (guard.boilerOn || sup.isOverrunActive());
                TEST_ASSERT_TRUE(demand[z] || overrunHold);
            }
        }

        // Invariant FR-3 / S5: at least one valve stays energised whenever boiler is ON
        if (guard.boilerOn) {
            TEST_ASSERT_TRUE(sup.isAnyValveEnergised());
        }

        guard.step(nowMs, anyReady, sup, true, false);

        // Invariant I-1 (FR-2): boiler never turns ON before at least one valve is open >= interlockDelay
        if (guard.boilerOn && !prevBoilerOn) {
            TEST_ASSERT_TRUE(sup.anyValveOpen(nowMs));
        }
        prevBoilerOn = guard.boilerOn;

        for (size_t z = 0; z < 2; ++z) {
            rooms[z].valveOpen = sup.isValveEnergised(z);
            rooms[z].sensorSilent = (z == 0) && sensorLost0;
            rooms[z].step1s(outdoorTemp);
        }
    }

    TEST_ASSERT_TRUE(sawFrostEntered);
    TEST_ASSERT_TRUE(sawFrostExited);
    TEST_ASSERT_TRUE(sawFaultDutyDemand);
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_fr2_interlock_and_fr1_3_bypass);
    RUN_TEST(test_fr3_overrun_hold_and_fr3_2_cancelled_start);
    RUN_TEST(test_fr16_1_frost_hysteresis);
    RUN_TEST(test_multizone_keeper_delete_and_wraparound);
    RUN_TEST(test_safety_invariants_every_simulated_second);
    return UNITY_END();
}
