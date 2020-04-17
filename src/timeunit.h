#ifndef TIMEUNIT_H
#define TIMEUNIT_H

#include <QString>
#include <QStringList>
#include <cstdint>

using std::int64_t;


class ReadableDuration
{
  public:
    enum TimeUnit {Milliseconds, Seconds, Minutes, Hours, Days, Unknown};

    static int64_t toMs(int64_t count, TimeUnit unitType);
    static std::pair<int64_t, TimeUnit> toHumanReadable(int64_t ms);

    static QStringList supportedUnits();
    static QString unitName(TimeUnit type);
    static TimeUnit unitType(const QString& unitName);
};

#endif // TIMEUNIT_H
