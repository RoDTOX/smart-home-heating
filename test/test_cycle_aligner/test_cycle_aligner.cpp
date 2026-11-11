#include <unity.h>
#include "CycleAligner.h"

namespace {

CycleAligner aligner;

}  // namespace

void setUp(void) {
    aligner = CycleAligner();
    aligner.begin(100000UL);
}

void tearDown(void) {}

// FR-63: cycle length is 3600 s; elapsed syncs to top-of-hour epoch when NTP valid, else nowMs / 1000 % 3600
void test_fr63_cycle_elapsed_epoch_and_millis_fallback(void) {
    TEST_ASSERT_EQUAL_UINT32(3600UL, aligner.getCycleLengthSec());
    TEST_ASSERT_EQUAL_UINT32(125UL, CycleAligner::computeCycleElapsedSec(999999UL, 1700000000LL + 125LL - (1700000000LL % 3600LL)));
    // When NTP epoch is unsynced (<= 100000), fallback uses passed nowMs without calling millis()
    TEST_ASSERT_EQUAL_UINT32(900UL, CycleAligner::computeCycleElapsedSec(4500000UL, 0));
    TEST_ASSERT_EQUAL_UINT32(600UL, aligner.getCycleElapsedSec(4200000UL));
}

// FR-63: active zones with circuits < minLoops (default 3) are extended/grouped; >= minLoops or inactive are not
void test_fr63_zone_extended_and_alignment(void) {
    TEST_ASSERT_TRUE(aligner.isZoneAligned(0));
    TEST_ASSERT_FALSE(CycleAligner::computeZoneExtended(false, 2, 3));
    TEST_ASSERT_TRUE(CycleAligner::computeZoneExtended(true, 2, 3));
    TEST_ASSERT_FALSE(CycleAligner::computeZoneExtended(true, 3, 3));
    aligner.update(200000UL);
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_fr63_cycle_elapsed_epoch_and_millis_fallback);
    RUN_TEST(test_fr63_zone_extended_and_alignment);
    return UNITY_END();
}
