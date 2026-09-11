#include "core/TestLoader.h"

#include <QFile>
#include <cstdio>

static int expect(bool condition, const char* message) {
    if (condition) return 0;
    std::fprintf(stderr, "FAIL: %s\n", message);
    return 1;
}

int main(int argc, char** argv) {
    if (argc > 1) {
        QFile input(QString::fromLocal8Bit(argv[1]));
        if (!input.open(QIODevice::ReadOnly | QIODevice::Text)) return 2;
        const QVector<TestCase> cases =
            TestLoader::parseGTestListTests(QString::fromLocal8Bit(input.readAll()));
        for (const auto& tc : cases)
            std::printf("%s\n", tc.fullName().toUtf8().constData());
        return 0;
    }

    const QString output =
        "HuaweiModels/FixtureTestTopology.\n"
        "  CheckTopoValidity_Neg1_YW\n"
        "  CheckTopoValidity_Neg2_YW # comment\n"
        "========== External analysis report ==========\n"
        "  this is report text\n"
        "GoogleTestVerification.\n"
        "  UninstantiatedParameterizedTestSuite<FixtureTestTopology>\n"
        "TypedFixture/0. # TypeParam = int\n"
        "  HandlesValue/0 # GetParam() = 42\n"
        "FixtureTestTopology.\n"
        "  FixShape_Neg1_YW\n";

    const QVector<TestCase> cases = TestLoader::parseGTestListTests(output);
    int failures = 0;
    failures += expect(cases.size() == 4, "only legal gtest cases are parsed");
    failures += expect(cases.value(0).fullName() ==
                       "HuaweiModels/FixtureTestTopology.CheckTopoValidity_Neg1_YW",
                       "suite and first case are preserved");
    failures += expect(cases.value(1).fullName() ==
                       "HuaweiModels/FixtureTestTopology.CheckTopoValidity_Neg2_YW",
                       "gtest comments are removed");
    failures += expect(cases.value(2).fullName() == "TypedFixture/0.HandlesValue/0",
                       "typed and parameterized gtest names are preserved");
    failures += expect(cases.value(3).fullName() ==
                       "FixtureTestTopology.FixShape_Neg1_YW",
                       "a later valid suite is still parsed");
    return failures == 0 ? 0 : 1;
}
