#include "businessformservice.h"

#include "binaryencoder.h"
#include "fieldvalue.h"
#include "businesscodec.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonValue>
#include <cmath>
#include <limits>

namespace {

double configuredNumber(const QJsonObject &config, const QString &key, double fallback)
{
    const QJsonValue value = config.value(key);
    if (value.isDouble())
        return value.toDouble();
    if (value.isString()) {
        bool ok = false;
        const double parsed = value.toString().toDouble(&ok);
        if (ok)
            return parsed;
    }
    return fallback;
}

bool configuredScale(const QJsonObject &config, double &scale, QString &error)
{
    if (!config.contains(QStringLiteral("scale"))) {
        scale = 1.0;
    } else {
        const QJsonValue value = config.value(QStringLiteral("scale"));
        bool ok = value.isDouble();
        scale = value.toDouble();
        if (value.isString())
            scale = value.toString().toDouble(&ok);
        if (!ok) {
            error = QStringLiteral("换算比例必须是数字。");
            return false;
        }
    }
    if (!std::isfinite(scale) || scale <= 0.0) {
        error = QStringLiteral("换算比例必须是大于 0 的有限数字。");
        return false;
    }
    return true;
}

void collectFields(const QVector<FieldInfo> &fields, const QString &parentPath,
                   QHash<QString, FieldInfo> &output)
{
    for (const FieldInfo &field : fields) {
        const QString path = parentPath.isEmpty()
            ? field.name : parentPath + QLatin1Char('.') + field.name;
        output.insert(path, field);
        if (!field.children.isEmpty())
            collectFields(field.children, path, output);
    }
}

BusinessFormResult failure(const QString &message)
{
    BusinessFormResult result;
    result.diagnostics.append(message);
    return result;
}

} // namespace

QString defaultBusinessWidget(FieldValueKind kind)
{
    switch (kind) {
    case FieldValueKind::SignedInteger:
    case FieldValueKind::UnsignedInteger: return QStringLiteral("spinbox");
    case FieldValueKind::Float:
    case FieldValueKind::Double:
    case FieldValueKind::LongDouble: return QStringLiteral("doublespinbox");
    case FieldValueKind::Boolean: return QStringLiteral("checkbox");
    case FieldValueKind::Record: return QStringLiteral("group");
    default: return QStringLiteral("readonly");
    }
}

QJsonValue businessNumberOrString(const QString &text)
{
    bool ok = false;
    const double value = text.trimmed().toDouble(&ok);
    if (!ok || !std::isfinite(value))
        return text;
    return QJsonValue(value);
}

bool validateBusinessFieldConfig(const BusinessFieldConfig &field, QString &error)
{
    if (field.label.isEmpty()) {
        error = QStringLiteral("%1：显示名称不能为空。").arg(field.path);
        return false;
    }
    const QStringList widgetTypes = QStringList() << QStringLiteral("spinbox")
        << QStringLiteral("doublespinbox") << QStringLiteral("combobox")
        << QStringLiteral("checkbox") << QStringLiteral("lineedit")
        << QStringLiteral("group") << QStringLiteral("readonly");
    if (!widgetTypes.contains(field.widget)) {
        error = QStringLiteral("%1：UI 控件类型不受支持。").arg(field.path);
        return false;
    }
    const bool integerInput = field.widget == QLatin1String("spinbox");
    const bool realInput = field.widget == QLatin1String("doublespinbox");
    const bool numericInput = integerInput || realInput;
    const bool comboInput = field.widget == QLatin1String("combobox");
    const bool checkboxInput = field.widget == QLatin1String("checkbox");
    const auto parseFinite = [](const QString &text, double &value) {
        bool ok = false;
        value = text.toDouble(&ok);
        return ok && std::isfinite(value);
    };
    double minimum = 0.0, maximum = 0.0, scale = 0.0;
    if (numericInput && !field.minimum.isEmpty() && !parseFinite(field.minimum, minimum)) {
        error = QStringLiteral("%1：最小值必须是有限数字。").arg(field.path);
        return false;
    }
    if (numericInput && !field.maximum.isEmpty() && !parseFinite(field.maximum, maximum)) {
        error = QStringLiteral("%1：最大值必须是有限数字。").arg(field.path);
        return false;
    }
    if (numericInput && !field.minimum.isEmpty() && !field.maximum.isEmpty() && minimum > maximum) {
        error = QStringLiteral("%1：最小值不能大于最大值。").arg(field.path);
        return false;
    }
    if (numericInput && !field.scale.isEmpty() && (!parseFinite(field.scale, scale) || scale <= 0.0)) {
        error = QStringLiteral("%1：换算比例必须是大于 0 的有限数字。").arg(field.path);
        return false;
    }
    if (realInput && !field.decimals.isEmpty()) {
        bool ok = false;
        const int decimals = field.decimals.toInt(&ok);
        if (!ok || decimals < 0 || decimals > 15) {
            error = QStringLiteral("%1：小数位数需为 0 到 15 的整数。").arg(field.path);
            return false;
        }
    }
    if (numericInput && !field.defaultValue.isEmpty()) {
        double value = 0.0;
        if (!parseFinite(field.defaultValue, value)) {
            error = QStringLiteral("%1：默认值目前需为有限数字。").arg(field.path);
            return false;
        }
        if ((!field.minimum.isEmpty() && value < minimum)
            || (!field.maximum.isEmpty() && value > maximum)) {
            error = QStringLiteral("%1：默认值必须位于最小值和最大值范围内。").arg(field.path);
            return false;
        }
    }
    if (checkboxInput && !field.defaultValue.isEmpty()
        && field.defaultValue != QLatin1String("true") && field.defaultValue != QLatin1String("false")) {
        error = QStringLiteral("%1：复选框默认值应为 true 或 false。").arg(field.path);
        return false;
    }

    QJsonArray options;
    if (comboInput) {
        QJsonParseError parseError;
        const QJsonDocument optionsDocument = QJsonDocument::fromJson(field.options.toUtf8(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !optionsDocument.isArray()) {
            error = QStringLiteral("%1：下拉选项必须是有效的 JSON 数组。").arg(field.path);
            return false;
        }
        options = optionsDocument.array();
    }
    if (comboInput && options.isEmpty()) {
        error = QStringLiteral("%1：组合框至少需要一个选项。").arg(field.path);
        return false;
    }
    for (int i = 0; comboInput && i < options.size(); ++i) {
        const QJsonObject option = options.at(i).toObject();
        if (!options.at(i).isObject() || !option.value(QStringLiteral("label")).isString()
            || !(option.value(QStringLiteral("value")).isDouble()
                 || option.value(QStringLiteral("value")).isString())) {
            error = QStringLiteral("%1：每个选项需包含 label 字符串和 value 数字或字符串。")
                .arg(field.path);
            return false;
        }
        if (option.value(QStringLiteral("label")).toString().trimmed().isEmpty()) {
            error = QStringLiteral("%1：选项显示标签不能为空。").arg(field.path);
            return false;
        }
    }
    if (comboInput && !field.defaultValue.isEmpty()) {
        const QJsonValue defaultValue = field.defaultIsString
            ? QJsonValue(field.defaultValue) : businessNumberOrString(field.defaultValue);
        bool foundDefault = false;
        for (const QJsonValue &option : options) {
            if (option.toObject().value(QStringLiteral("value")) == defaultValue) {
                foundDefault = true;
                break;
            }
        }
        if (!foundDefault) {
            error = QStringLiteral("%1：默认值必须对应一个下拉选项的 value。").arg(field.path);
            return false;
        }
    }
    error.clear();
    return true;
}

QJsonObject businessFieldConfigObject(const BusinessFieldConfig &field)
{
    QJsonObject item{
        {QStringLiteral("label"), field.label},
        {QStringLiteral("widget"), field.widget}
    };
    const bool numericInput = field.widget == QLatin1String("spinbox")
        || field.widget == QLatin1String("doublespinbox");
    if (numericInput && !field.unit.isEmpty()) item.insert(QStringLiteral("unit"), field.unit);
    if (numericInput && !field.minimum.isEmpty())
        item.insert(QStringLiteral("min"), businessNumberOrString(field.minimum));
    if (numericInput && !field.maximum.isEmpty())
        item.insert(QStringLiteral("max"), businessNumberOrString(field.maximum));
    if (numericInput && !field.scale.isEmpty())
        item.insert(QStringLiteral("scale"), businessNumberOrString(field.scale));
    if (field.widget == QLatin1String("doublespinbox") && !field.decimals.isEmpty())
        item.insert(QStringLiteral("decimals"), field.decimals.toInt());
    const bool hasDefault = numericInput || field.widget == QLatin1String("combobox")
        || field.widget == QLatin1String("checkbox") || field.widget == QLatin1String("lineedit");
    if (hasDefault && !field.defaultValue.isEmpty()) {
        if (field.widget == QLatin1String("checkbox"))
            item.insert(QStringLiteral("default"), field.defaultValue == QLatin1String("true"));
        else if (field.widget == QLatin1String("lineedit")
                 || (field.widget == QLatin1String("combobox") && field.defaultIsString))
            item.insert(QStringLiteral("default"), field.defaultValue);
        else
            item.insert(QStringLiteral("default"), businessNumberOrString(field.defaultValue));
    }
    if (field.widget == QLatin1String("combobox")) {
        QJsonParseError parseError;
        const QJsonDocument optionsDocument = QJsonDocument::fromJson(field.options.toUtf8(), &parseError);
        if (parseError.error == QJsonParseError::NoError && optionsDocument.isArray())
            item.insert(QStringLiteral("options"), optionsDocument.array());
    }
    return item;
}

BusinessValueResult businessRawToInputValue(const FieldInfo &field,
                                           const QJsonObject &fieldConfig,
                                           const QString &rawValue)
{
    BusinessValueResult result;
    const QString widget = fieldConfig.value(QStringLiteral("widget")).toString();
    if (widget == QLatin1String("spinbox") || widget == QLatin1String("doublespinbox")) {
        long double rawNumber = 0.0L;
        if (!parseBusinessNumber(rawValue, rawNumber)) {
            result.success = true;
            result.value = rawValue;
            return result;
        }
        double scale = 1.0;
        if (!configuredScale(fieldConfig, scale, result.error))
            return result;
        long double display = rawNumber * static_cast<long double>(scale);
        if (!std::isfinite(display)) {
            result.error = QStringLiteral("按换算比例计算后超出数值范围。");
            return result;
        }
        if (widget == QLatin1String("spinbox"))
            display = std::round(display);
        result.value = QString::number(static_cast<double>(display), 'g', 17);
    } else if (widget == QLatin1String("checkbox")) {
        result.value = rawValue == QLatin1String("true") || rawValue == QLatin1String("1")
            ? QStringLiteral("true") : QStringLiteral("false");
    } else {
        result.value = rawValue;
    }
    result.success = true;
    return result;
}

BusinessValueResult businessInputToRawValue(const FieldInfo &field,
                                           const QJsonObject &fieldConfig,
                                           const QString &inputValue)
{
    BusinessValueResult result;
    const QString widget = fieldConfig.value(QStringLiteral("widget")).toString();
    if (widget == QLatin1String("spinbox") || widget == QLatin1String("doublespinbox")) {
        long double displayed = 0.0L;
        if (!parseBusinessNumber(inputValue, displayed)) {
            result.error = QStringLiteral("输入值不是有效数字。");
            return result;
        }
        double scale = 1.0;
        if (!configuredScale(fieldConfig, scale, result.error))
            return result;
        const long double raw = displayed / static_cast<long double>(scale);
        if (!std::isfinite(raw)) {
            result.error = QStringLiteral("按换算比例还原后超出数值范围。");
            return result;
        }

        if (field.valueKind == FieldValueKind::SignedInteger
            || field.valueKind == FieldValueKind::UnsignedInteger
            || field.valueKind == FieldValueKind::Boolean) {
            const long double rounded = std::round(raw);
            const long double tolerance = 1e-9L * (std::fabs(raw) > 1.0L ? std::fabs(raw) : 1.0L);
            if (std::fabs(raw - rounded) > tolerance) {
                result.error = QStringLiteral("按换算比例还原后不是整数。");
                return result;
            }
            if (field.valueKind == FieldValueKind::Boolean) {
                if (rounded != 0.0L && rounded != 1.0L) {
                    result.error = QStringLiteral("布尔字段换算结果只能为 0 或 1。");
                    return result;
                }
                result.value = rounded == 1.0L ? QStringLiteral("true") : QStringLiteral("false");
            } else if (field.valueKind == FieldValueKind::SignedInteger) {
                const long double limit = std::ldexp(1.0L, 63);
                if (rounded < -limit || rounded >= limit) {
                    result.error = QStringLiteral("换算后的整数超出范围。");
                    return result;
                }
                result.value = QString::number(static_cast<qint64>(rounded));
            } else {
                const long double limit = std::ldexp(1.0L, 64);
                if (rounded < 0.0L || rounded >= limit) {
                    result.error = QStringLiteral("换算后的无符号整数超出范围。");
                    return result;
                }
                result.value = QString::number(static_cast<quint64>(rounded));
            }
        } else {
            result.value = QString::number(static_cast<double>(raw), 'g', 17);
        }
    } else if (widget == QLatin1String("checkbox")) {
        const bool checked = inputValue.compare(QLatin1String("true"), Qt::CaseInsensitive) == 0
            || inputValue == QLatin1String("1");
        result.value = field.valueKind == FieldValueKind::Boolean
            ? (checked ? QStringLiteral("true") : QStringLiteral("false"))
            : (checked ? QStringLiteral("1") : QStringLiteral("0"));
    } else if (widget == QLatin1String("combobox")) {
        if (inputValue.isEmpty()) {
            result.error = QStringLiteral("请先选择一个下拉选项。");
            return result;
        }
        result.value = inputValue;
        if (field.valueKind == FieldValueKind::Boolean) {
            if (result.value == QLatin1String("0")) result.value = QStringLiteral("false");
            else if (result.value == QLatin1String("1")) result.value = QStringLiteral("true");
        }
    } else if (widget == QLatin1String("lineedit")) {
        result.value = inputValue;
    } else {
        result.error = QStringLiteral("该字段没有可编辑的业务控件。");
        return result;
    }

    result.success = true;
    return result;
}

BusinessFormResult encodeBusinessForm(const StructInfo &structure,
                                      const QJsonObject &fieldConfigs,
                                      const QHash<QString, QString> &inputValues,
                                      const QByteArray &baseBytes)
{
    QHash<QString, FieldInfo> fields;
    collectFields(structure.fields, QString(), fields);

    QHash<QString, QString> rawValues;
    for (auto it = inputValues.constBegin(); it != inputValues.constEnd(); ++it) {
        if (!fields.contains(it.key()))
            return failure(QStringLiteral("业务配置中包含无效字段：%1").arg(it.key()));
        const FieldInfo field = fields.value(it.key());
        if (!isEditableField(field))
            continue;
        if (!fieldConfigs.value(it.key()).isObject())
            return failure(QStringLiteral("业务配置缺少字段：%1").arg(it.key()));

        const BusinessValueResult converted = businessInputToRawValue(
            field, fieldConfigs.value(it.key()).toObject(), it.value());
        if (!converted.success)
            return failure(QStringLiteral("%1：%2").arg(it.key(), converted.error));
        rawValues.insert(it.key(), converted.value);
    }

    const BinaryResult encoded = baseBytes.isEmpty()
        ? encodeStructure(structure, rawValues)
        : encodeStructure(structure, rawValues, baseBytes);
    if (!encoded.success) {
        BusinessFormResult result;
        result.diagnostics = encoded.diagnostics;
        return result;
    }

    const DecodeResult decoded = decodeStructure(structure, encoded.bytes);
    if (!decoded.success) {
        BusinessFormResult result;
        result.diagnostics = decoded.diagnostics;
        return result;
    }

    BusinessFormResult result;
    result.success = true;
    result.bytes = encoded.bytes;
    result.decoded = decoded;
    return result;
}
