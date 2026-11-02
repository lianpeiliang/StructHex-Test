#include "businessformpreview.h"
#include "business/businessformservice.h"
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
#include <QPushButton>
#include <QMessageBox>
#include <QSpinBox>
#include <QVBoxLayout>
#include <cmath>

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
    QHash<QString, QString> inputValues;
    QJsonObject fieldConfigs;
    for (auto it = m_fieldWidgets.constBegin(); it != m_fieldWidgets.constEnd(); ++it) {
        const QString path = it.key();
        const QWidget *editor = it.value();
        const QString widgetType = m_fieldConfigs.value(path).value(QStringLiteral("widget")).toString();
        fieldConfigs.insert(path, m_fieldConfigs.value(path));
        QString input;

        if (widgetType == QLatin1String("spinbox")) {
            input = QString::number(qobject_cast<const QSpinBox *>(editor)->value());
        } else if (widgetType == QLatin1String("doublespinbox")) {
            input = QString::number(qobject_cast<const QDoubleSpinBox *>(editor)->value(), 'g', 17);
        } else if (widgetType == QLatin1String("combobox")) {
            const QComboBox *combo = qobject_cast<const QComboBox *>(editor);
            input = combo->currentData().toString();
        } else if (widgetType == QLatin1String("checkbox")) {
            const bool checked = qobject_cast<const QCheckBox *>(editor)->isChecked();
            input = checked ? QStringLiteral("true") : QStringLiteral("false");
        } else if (widgetType == QLatin1String("lineedit")) {
            input = qobject_cast<const QLineEdit *>(editor)->text();
        } else {
            continue;
        }
        inputValues.insert(path, input);
    }

    const BusinessFormResult result = encodeBusinessForm(m_structure, fieldConfigs, inputValues);
    if (!result.success) {
        QMessageBox::warning(this, QStringLiteral("无法生成十六进制"),
                             result.diagnostics.join(QLatin1Char('\n')));
        return;
    }
    HexDialogs hexDialogs(this);
    hexDialogs.showPreview(m_structure, result.bytes, m_structureDefinitionPath);
}
