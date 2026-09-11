#pragma once

#include "models/TestResult.h"

namespace FilterMapping {

// filter set signature
QString filterSetsSignature(const QVector<FilterSet>& sets);

QMap<QString, QStringList> computeMappings(const QVector<FilterSet>& sets, const QVector<TestRunResult>& results);

bool matches(const FilterSet& fs, const TestRunResult& r);

// A persisted mapping is usable only when it still intersects the current tree.
bool hasAvailableMatch(const QStringList& mappedNames, const QStringList& availableNames);

} // namespace FilterMapping
