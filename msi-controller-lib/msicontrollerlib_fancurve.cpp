// =============================================================================
// Fan curve validation and parsing.
// =============================================================================

#include "msicontrollerlib.h"

using namespace MsiControllerLib;

bool MsiControllerLib::isValidCurve(const QString &fanCurve, int entriesCount) {
    // A fan curve is a space-separated list of 2*n-1 integers alternating
    // between fan speed % (even indices) and temperature (odd indices):
    //   s0 t1 s1 t2 … s(n-1) t(n) s(n)
    // t0 is implicit - s0 applies until t1 is reached.
    //
    // Constraints:
    //  - Temperatures: strictly increasing, in (0, 100]
    //  - Fan speeds:   non-decreasing, in [0, 150]
    //  - Last fan speed must be >= 85 (safety floor)

    if (entriesCount <= 0)
        return false;

    const QStringList tokens = fanCurve.split(u' ', Qt::SkipEmptyParts);

    const int expectedLen = 2 * entriesCount - 1;
    if (tokens.length() != expectedLen)
        return false;

    // expectedLen is always odd because 2n-1 is odd for any integer n>=1,
    // so no separate even/odd sanity check is needed here.

    QList<int> values;
    values.reserve(expectedLen);
    for (const QString &token : tokens) {
        bool ok = false;
        const int v = token.toInt(&ok);
        if (!ok)
            return false;
        values.append(v);
    }

    // Validate temperatures at odd indices.
    int previousTemp = 0;
    for (int i = 1; i < values.length(); i += 2) {
        if (values[i] <= previousTemp || values[i] > 100)
            return false;
        previousTemp = values[i];
    }

    // Validate fan speeds at even indices (non-decreasing, <= 150).
    int previousSpeed = 0;
    for (int i = 0; i < values.length(); i += 2) {
        if (values[i] < previousSpeed || values[i] > 150)
            return false;
        previousSpeed = values[i];
    }

    // Safety floor: the last speed entry must be >= 85.
    if (values.last() < 85)
        return false;

    return true;
}

int MsiControllerLib::getCurveLength(const QString &curve) {
    // Token count = 2n-1  ->  n = (tokens + 1) / 2
    return (curve.trimmed().split(u' ', Qt::SkipEmptyParts).length() + 1) / 2;
}
