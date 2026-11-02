#include "businesscodec.h"

#include <QJsonArray>
#include <cmath>

namespace {

QString jsonValueText(const QJsonValue &value)
{
    if (value.isString()) return value.toString();
    if (value.isBool()) return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    if (value.isDouble()) return QString::number(value.toDouble(), 'g', 15);
    return QString();
}

} // namespace

bool parseBusinessNumber(const QString &text, long double &number)
{
    const QString value = text.trimmed();
    if (value.isEmpty()) return false;
    if (value.compare(QLatin1String("true"), Qt::CaseInsensitive) == 0) {
        number = 1.0L;
        return true;
    }
    if (value.compare(QLatin1String("false"), Qt::CaseInsensitive) == 0) {
        number = 0.0L;
        return true;
    }
    bool ok = false;
    if (value.startsWith(QLatin1String("-0x"), Qt::CaseInsensitive)) {
        const qulonglong magnitude = value.mid(3).toULongLong(&ok, 16);
        if (ok) number = -static_cast<long double>(magnitude);
        return ok;
    }
    if (value.startsWith(QLatin1String("0x"), Qt::CaseInsensitive)) {
        const qulonglong parsed = value.mid(2).toULongLong(&ok, 16);
        if (ok) number = static_cast<long double>(parsed);
        return ok;
    }
    const double parsed = value.toDouble(&ok);
    if (ok) number = static_cast<long double>(parsed);
    return ok;
}

bool businessOptionMatches(const QJsonValue &optionValue, const QString &rawValue)
{
    const QString optionText = jsonValueText(optionValue);
    long double optionNumber = 0.0L, rawNumber = 0.0L;
    if (parseBusinessNumber(optionText, optionNumber)
        && parseBusinessNumber(rawValue, rawNumber))
        return optionNumber == rawNumber;
    return optionText.compare(rawValue, Qt::CaseInsensitive) == 0;
}

QString businessDisplayValue(const QString &rawValue, const QJsonObject &config,
                             QString &extraNote)
{
    const QString widget = config.value(QStringLiteral("widget")).toString();
    if (rawValue == QLatin1String("分组") || rawValue == QLatin1String("原始字节"))
        return rawValue;

    if (widget == QLatin1String("combobox")) {
        const QJsonArray options = config.value(QStringLiteral("options")).toArray();
        for (const QJsonValue &value : options) {
            const QJsonObject option = value.toObject();
            if (businessOptionMatches(option.value(QStringLiteral("value")), rawValue))
                return option.value(QStringLiteral("label")).toString();
        }
        extraNote = QStringLiteral("该原始值未配置对应的下拉选项。 ");
        return QStringLiteral("未配置选项（%1）").arg(rawValue);
    }
    if (widget == QLatin1String("checkbox"))
        return (rawValue == QLatin1String("true") || rawValue == QLatin1String("1"))
            ? QStringLiteral("已选中") : QStringLiteral("未选中");
    if (widget == QLatin1String("spinbox") || widget == QLatin1String("doublespinbox")) {
        long double rawNumber = 0.0L;
        if (!parseBusinessNumber(rawValue, rawNumber)) return rawValue;
        const QJsonValue scaleValue = config.value(QStringLiteral("scale"));
        const double scale = scaleValue.isDouble() ? scaleValue.toDouble()
            : scaleValue.toString().toDouble();
        const long double display = rawNumber * (scale > 0.0 && std::isfinite(scale) ? scale : 1.0);
        const int decimals = config.value(QStringLiteral("decimals")).toInt(-1);
        QString text = decimals >= 0
            ? QString::number(static_cast<double>(display), 'f', decimals)
            : QString::number(static_cast<double>(display), 'g', 15);
        const QString unit = config.value(QStringLiteral("unit")).toString().trimmed();
        if (!unit.isEmpty()) text += QLatin1Char(' ') + unit;
        return text;
    }
    return rawValue;
}
