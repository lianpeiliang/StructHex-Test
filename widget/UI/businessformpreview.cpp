#include "businessformpreview.h"
#include "binaryencoder.h"
#include "hexdialogs.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QMessageBox>
#include <QSpinBox>
#include <QVBoxLayout>
#include <cmath>
#include <limits>

namespace {

QString valueText(const QJsonValue &value)
{
    if (value.isString())
        return value.toString();
    if (value.isDouble())
        return QString::number(value.toDouble(), 'g', 15);
    if (value.isBool())
        return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    return QString();
}

double configuredNumber(const QJsonObject &config, const QString &key, double fallback)
{
    const QJsonValue value = config.value(key);
    return value.isDouble() ? value.toDouble() : fallback;
}

bool encodeDisplayedNumber(double displayed, double scale, const FieldInfo &field,
                           QString &value, QString &error)
{
    if (!std::isfinite(displayed) || !std::isfinite(scale) || scale <= 0.0) {
        error = QStringLiteral("显示值和换算比例必须是有限数字，且换算比例大于 0。");
        return false;
    }
    const long double raw = static_cast<long double>(displayed) / static_cast<long double>(scale);
    if (!std::isfinite(raw)) {
        error = QStringLiteral("按换算比例计算后超出数值范围。");
        return false;
    }
    if (field.valueKind == FieldValueKind::SignedInteger
        || field.valueKind == FieldValueKind::UnsignedInteger
        || field.valueKind == FieldValueKind::Boolean) {
        const long double rounded = std::round(raw);
        const long double tolerance = 1e-9L * (std::fabs(raw) > 1.0L ? std::fabs(raw) : 1.0L);
        if (std::fabs(raw - rounded) > tolerance) {
            error = QStringLiteral("按换算比例还原后不是整数。");
            return false;
        }
        if (field.valueKind == FieldValueKind::Boolean) {
            if (rounded != 0.0L && rounded != 1.0L) {
                error = QStringLiteral("布尔字段换算结果只能为 0 或 1。");
                return false;
            }
            value = rounded == 1.0L ? QStringLiteral("true") : QStringLiteral("false");
        } else if (field.valueKind == FieldValueKind::SignedInteger) {
            if (rounded < static_cast<long double>(std::numeric_limits<qint64>::min())
                || rounded > static_cast<long double>(std::numeric_limits<qint64>::max())) {
                error = QStringLiteral("换算后的整数超出支持范围。");
                return false;
            }
            value = QString::number(static_cast<qint64>(rounded));
        } else {
            if (rounded < 0.0L
                || rounded > static_cast<long double>(std::numeric_limits<quint64>::max())) {
                error = QStringLiteral("换算后的无符号整数超出支持范围。");
                return false;
            }
            value = QString::number(static_cast<quint64>(rounded));
        }
    } else {
        value = QString::number(static_cast<double>(raw), 'g', 17);
    }
    return true;
}

} // namespace

BusinessFormPreviewDialog::BusinessFormPreviewDialog(const QJsonDocument &config, const StructInfo &structure,
                                                     const QString &structureDefinitionPath, QWidget *parent)
    : QDialog(parent), m_structure(structure), m_structureDefinitionPath(structureDefinitionPath)
{
    const QJsonObject root = config.object();
    const QString title = root.value(QStringLiteral("title")).toString();
    setWindowTitle(title.isEmpty() ? QStringLiteral("业务表单预览") : title);
    resize(720, 600);
    setMinimumSize(520, 360);
    addFieldInfo(structure.fields, QString());

    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout;
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    const QJsonObject fields = root.value(QStringLiteral("fields")).toObject();
    for (auto it = fields.constBegin(); it != fields.constEnd(); ++it) {
        if (!it.value().isObject())
            continue;
        const QJsonObject field = it.value().toObject();
        const QString labelText = field.value(QStringLiteral("label")).toString(it.key());
        const QString unit = field.value(QStringLiteral("unit")).toString();
        const QString formLabel = unit.isEmpty()
            ? labelText + QLatin1Char(':')
            : labelText + QLatin1Char('(') + unit + QStringLiteral("):");
        QWidget *editor = createEditor(it.key(), field);
        if (editor) {
            editor->setObjectName(it.key());
            form->addRow(formLabel, editor);
            m_fieldWidgets.insert(it.key(), editor);
            m_fieldConfigs.insert(it.key(), field);
        }
    }
    layout->addLayout(form, 1);
    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    auto *hex = new QPushButton(QStringLiteral("生成十六进制"), this);
    hex->setEnabled(!m_structureDefinitionPath.isEmpty());
    hex->setToolTip(m_structureDefinitionPath.isEmpty()
        ? QStringLiteral("请先在设计器中选择原始结构 JSON。")
        : QStringLiteral("根据当前表单值和结构体内存布局生成小端十六进制。"));
    buttons->addWidget(hex);
    auto *close = new QPushButton(QStringLiteral("关闭"), this);
    buttons->addWidget(close);
    layout->addLayout(buttons);
    connect(hex, &QPushButton::clicked, this, &BusinessFormPreviewDialog::generateHex);
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
}

void BusinessFormPreviewDialog::addFieldInfo(const QVector<FieldInfo> &fields, const QString &parentPath)
{
    for (const FieldInfo &field : fields) {
        const QString path = parentPath.isEmpty() ? field.name : parentPath + QLatin1Char('.') + field.name;
        m_fieldInfo.insert(path, field);
        if (!field.children.isEmpty())
            addFieldInfo(field.children, path);
    }
}

QWidget *BusinessFormPreviewDialog::createEditor(const QString &, const QJsonObject &config)
{
    const QString widget = config.value(QStringLiteral("widget")).toString();
    const QJsonValue defaultValue = config.value(QStringLiteral("default"));
    const double minimum = configuredNumber(config, QStringLiteral("min"), 0.0);
    const double maximum = configuredNumber(config, QStringLiteral("max"), 99.0);

    if (widget == QLatin1String("spinbox")) {
        auto *spin = new QSpinBox(this);
        spin->setRange(int(minimum), int(maximum));
        spin->setValue(int(configuredNumber(config, QStringLiteral("default"), 0.0)));
        return spin;
    }
    if (widget == QLatin1String("doublespinbox")) {
        auto *spin = new QDoubleSpinBox(this);
        const int decimals = int(configuredNumber(config, QStringLiteral("decimals"), 2.0));
        spin->setDecimals(decimals);
        spin->setSingleStep(std::pow(10.0, -decimals));
        spin->setRange(minimum, maximum);
        spin->setValue(configuredNumber(config, QStringLiteral("default"), 0.0));
        return spin;
    }
    if (widget == QLatin1String("combobox")) {
        auto *combo = new QComboBox(this);
        combo->setObjectName(QStringLiteral("businessFormPreviewCombo"));
        combo->setStyleSheet(QStringLiteral(
            "QComboBox#businessFormPreviewCombo { padding-top: 2px; padding-bottom: 2px; min-height: 0px; }"));
        const QJsonArray options = config.value(QStringLiteral("options")).toArray();
        int defaultIndex = -1;
        for (int i = 0; i < options.size(); ++i) {
            const QJsonObject option = options.at(i).toObject();
            const QJsonValue optionValue = option.value(QStringLiteral("value"));
            combo->addItem(option.value(QStringLiteral("label")).toString(), optionValue.toVariant());
            if (!defaultValue.isUndefined() && optionValue == defaultValue)
                defaultIndex = i;
        }
        if (defaultIndex >= 0)
            combo->setCurrentIndex(defaultIndex);
        return combo;
    }
    if (widget == QLatin1String("checkbox")) {
        auto *checkbox = new QCheckBox(this);
        checkbox->setChecked(defaultValue.toBool() || (defaultValue.isDouble() && defaultValue.toDouble() != 0.0));
        return checkbox;
    }
    if (widget == QLatin1String("lineedit")) {
        auto *line = new QLineEdit(this);
        line->setText(valueText(defaultValue));
        return line;
    }
    if (widget == QLatin1String("group")) {
        auto *group = new QGroupBox(this);
        group->setMinimumHeight(20);
        return group;
    }
    auto *readOnly = new QLabel(valueText(defaultValue), this);
    readOnly->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return readOnly;
}

void BusinessFormPreviewDialog::generateHex()
{
    QHash<QString, QString> values;
    for (auto it = m_fieldWidgets.constBegin(); it != m_fieldWidgets.constEnd(); ++it) {
        const QString path = it.key();
        const QWidget *editor = it.value();
        const QString widgetType = m_fieldConfigs.value(path).value(QStringLiteral("widget")).toString();
        const FieldInfo field = m_fieldInfo.value(path);
        QString value;

        if (widgetType == QLatin1String("spinbox") || widgetType == QLatin1String("doublespinbox")) {
            const double displayed = widgetType == QLatin1String("spinbox")
                ? qobject_cast<const QSpinBox *>(editor)->value()
                : qobject_cast<const QDoubleSpinBox *>(editor)->value();
            const double scale = configuredNumber(m_fieldConfigs.value(path), QStringLiteral("scale"), 1.0);
            QString error;
            if (!encodeDisplayedNumber(displayed, scale, field, value, error)) {
                QMessageBox::warning(this, QStringLiteral("无法生成十六进制"),
                    QStringLiteral("%1：%2").arg(path, error));
                return;
            }
        } else if (widgetType == QLatin1String("combobox")) {
            const QComboBox *combo = qobject_cast<const QComboBox *>(editor);
            value = combo->currentData().toString();
            if (field.valueKind == FieldValueKind::Boolean) {
                if (value == QLatin1String("0")) value = QStringLiteral("false");
                else if (value == QLatin1String("1")) value = QStringLiteral("true");
            }
        } else if (widgetType == QLatin1String("checkbox")) {
            const bool checked = qobject_cast<const QCheckBox *>(editor)->isChecked();
            value = field.valueKind == FieldValueKind::Boolean
                ? (checked ? QStringLiteral("true") : QStringLiteral("false"))
                : (checked ? QStringLiteral("1") : QStringLiteral("0"));
        } else if (widgetType == QLatin1String("lineedit")) {
            value = qobject_cast<const QLineEdit *>(editor)->text();
        } else {
            continue;
        }
        values.insert(path, value);
    }

    const BinaryResult result = encodeStructure(m_structure, values);
    if (!result.success) {
        QMessageBox::warning(this, QStringLiteral("无法生成十六进制"), result.diagnostics.join(QLatin1Char('\n')));
        return;
    }
    HexDialogs hexDialogs(this);
    hexDialogs.showPreview(m_structure, result.bytes, m_structureDefinitionPath);
}
