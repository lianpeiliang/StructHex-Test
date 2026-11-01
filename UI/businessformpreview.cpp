#include "businessformpreview.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

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

BusinessFormPreviewDialog::BusinessFormPreviewDialog(const QJsonDocument &config, QWidget *parent)
    : QDialog(parent)
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
        }
    }
    layout->addLayout(form, 1);
    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    auto *close = new QPushButton(QStringLiteral("关闭"), this);
    buttons->addWidget(close);
    layout->addLayout(buttons);
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
        spin->setDecimals(int(configuredNumber(config, QStringLiteral("decimals"), 2.0)));
        spin->setRange(minimum, maximum);
        spin->setValue(configuredNumber(config, QStringLiteral("default"), 0.0));
        return spin;
    }
    if (widget == QLatin1String("combobox")) {
        auto *combo = new QComboBox(this);
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
