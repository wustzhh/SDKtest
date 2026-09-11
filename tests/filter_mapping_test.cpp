#include "core/FilterMapping.h"

#include <cstdio>

static int expect(bool condition, const char* message) {
    if (condition) return 0;
    std::fprintf(stderr, "FAIL: %s\n", message);
    return 1;
}

int main() {
    int failures = 0;
    failures += expect(
        FilterMapping::hasAvailableMatch({"SuiteA.Alpha"}, {"SuiteA.Alpha", "SuiteB.Beta"}),
        "a mapping matching a tree case is usable");
    failures += expect(
        !FilterMapping::hasAvailableMatch({"OldSuite.OldCase"}, {"SuiteA.Alpha", "SuiteB.Beta"}),
        "a stale mapping with no tree intersection is not usable");
    failures += expect(
        !FilterMapping::hasAvailableMatch({}, {"SuiteA.Alpha"}),
        "an empty mapping is not usable");
    return failures == 0 ? 0 : 1;
}
