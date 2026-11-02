#ifndef BUSINESSCODEC_H
#define BUSINESSCODEC_H

#include <QJsonObject>
#include <QJsonValue>
#include <QString>

// Shared business-value rules. These functions contain no widget dependencies.
bool parseBusinessNumber(const QString &text, long double &number);
bool businessOptionMatches(const QJsonValue &optionValue, const QString &rawValue);
QString businessDisplayValue(const QString &rawValue, const QJsonObject &config,
                             QString &extraNote);

#endif
