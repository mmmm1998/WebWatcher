#include "timeunit.h"

#include <cassert>

#include <QMap>
#include <QObject>

using namespace std;

// second == 1'000 ms
// minute = 60'000 ms
// hour == 3'600'000 ms
// day == 86'400'000 ms
static const QMap<ReadableDuration::TimeUnit, pair<QString, int64_t>> UNITS{
    {ReadableDuration::Milliseconds, {QObject::tr("ms"), 1}},
    {ReadableDuration::Seconds, {QObject::tr("seconds"), 1000}},
    {ReadableDuration::Minutes, {QObject::tr("minutes"), 60000}},
    {ReadableDuration::Hours, {QObject::tr("hours"), 3600000}},
    {ReadableDuration::Days, {QObject::tr("days"), 86400000}},
};


int64_t ReadableDuration::toMs(int64_t count, TimeUnit unitType)
{
    assert(UNITS.contains(unitType));
    return count * UNITS[unitType].second;
}

std::pair<int64_t, ReadableDuration::TimeUnit> ReadableDuration::toHumanReadable(int64_t ms)
{
    TimeUnit bestType = ReadableDuration::Milliseconds;
    // That in meaning minimization unit counts via increasing unit level
    // for example, from ms to seconds, 1000 ms -> 1 second
    int64_t bestUnitCount = ms;
    for (TimeUnit unit : UNITS.keys())
    {
        int64_t msInUnit = UNITS[unit].second;
        if (ms % msInUnit == 0 && ms / msInUnit < bestUnitCount)
        {
            bestType = unit;
            bestUnitCount = ms / msInUnit;
        }
    }

    return make_pair(bestUnitCount, bestType);
}

QStringList ReadableDuration::supportedUnits()
{
    QStringList units;
    units << QObject::tr("ms") << QObject::tr("seconds") << QObject::tr("minutes") << QObject::tr("hours") << QObject::tr("days");
    return units;
}

QString ReadableDuration::unitName(ReadableDuration::TimeUnit type)
{
    assert(UNITS.contains(type));
    return UNITS[type].first;
}

ReadableDuration::TimeUnit ReadableDuration::unitType(const QString& unitName)
{
    for (TimeUnit type : UNITS.keys())
    {
        if (UNITS[type].first == unitName)
            return type;
    }
    return TimeUnit::Unknown;
}
