#include "businessconfigeditor.h"
#include "businessformpreview.h"

#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QFormLayout>
#include <QGroupBox>
#include <QHash>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSaveFile>
#include <QScrollArea>
#include <QTableWidget>
#include <QHeaderView>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <cmath>

namespace {

QString defaultWidget(FieldValueKind kind)
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

QJsonValue numberOrString(const QString &text)
{
    bool ok = false;
    const double value = text.trimmed().toDouble(&ok);
    if (!ok || !std::isfinite(value))
        return text;
    return QJsonValue(value);
}

} // namespace

BusinessConfigEditorDialog::BusinessConfigEditorDialog(const StructInfo &structure, QWidget *parent)
    : QDialog(parent), m_structure(structure)
{
    setWindowTitle(QStringLiteral("业务配置 JSON 设计器 · %1").arg(structure.name));
    resize(1120, 720);
    setMinimumSize(900, 600);

    auto *root = new QVBoxLayout(this);
    auto *header = new QFormLayout;
    m_title = new QLineEdit(structure.name, this);
    header->addRow(QStringLiteral("表单标题："), m_title);
    root->addLayout(header);

    auto *body = new QHBoxLayout;
    m_fieldTree = new QTreeWidget(this);
    m_fieldTree->setHeaderLabels(QStringList() << QStringLiteral("字段") << QStringLiteral("C++ 类型"));
    m_fieldTree->setMinimumWidth(330);
    m_fieldTree->setColumnWidth(0, 190);
    body->addWidget(m_fieldTree, 1);

    auto *editorScroll = new QScrollArea(this);
    editorScroll->setWidgetResizable(true);
    editorScroll->setFrameShape(QFrame::NoFrame);
    auto *editor = new QWidget;
    editorScroll->setWidget(editor);
    auto *editorLayout = new QVBoxLayout(editor);
    editorLayout->setContentsMargins(0, 0, 0, 0);
    auto *displayForm = new QFormLayout;
    m_label = new QLineEdit(editor);
    displayForm->addRow(QStringLiteral("显示名称："), m_label);
    editorLayout->addLayout(displayForm);

    auto *controlGroup = new QGroupBox(QStringLiteral("UI 控件配置"), editor);
    auto *form = new QFormLayout(controlGroup);
    m_widget = new QComboBox(editor);
    m_widget->addItems(QStringList() << QStringLiteral("spinbox") << QStringLiteral("doublespinbox")
                                     << QStringLiteral("combobox") << QStringLiteral("checkbox")
                                     << QStringLiteral("lineedit") << QStringLiteral("group")
                                     << QStringLiteral("readonly"));
    form->addRow(QStringLiteral("UI控件："), m_widget);
    connect(m_widget, &QComboBox::currentTextChanged, this, [this](const QString &newWidget) {
        if (m_selectedIndex >= 0 && m_selectedIndex < m_fields.size()) {
            FieldConfig &field = m_fields[m_selectedIndex];
            const QString oldWidget = field.widget;
            const bool oldNumeric = oldWidget == QLatin1String("spinbox")
                || oldWidget == QLatin1String("doublespinbox");
            const bool newNumeric = newWidget == QLatin1String("spinbox")
                || newWidget == QLatin1String("doublespinbox");
            const bool sameDefaultKind = (oldNumeric && newNumeric) || oldWidget == newWidget;
            if (!sameDefaultKind) {
                field.defaultValue.clear();
                field.defaultIsString = false;
                m_defaultValue->clear();
            }
            field.widget = newWidget;
        }
        updatePropertyVisibility();
    });
    m_unit = new QLineEdit(editor);
    form->addRow(QStringLiteral("单位："), m_unit);
    m_unitLabel = form->labelForField(m_unit);
    m_minimum = new QLineEdit(editor);
    form->addRow(QStringLiteral("最小值："), m_minimum);
    m_minimumLabel = form->labelForField(m_minimum);
    m_maximum = new QLineEdit(editor);
    form->addRow(QStringLiteral("最大值："), m_maximum);
    m_maximumLabel = form->labelForField(m_maximum);
    auto *rangeHint = new QLabel(
        QStringLiteral("最小值和最大值按界面显示值填写，不是结构体原始值。例：m_Start 填 -90～90°、换算比例 0.01，对应原始值 -9000～9000。"),
        editor);
    rangeHint->setWordWrap(true);
    rangeHint->setObjectName(QStringLiteral("businessConfigRangeHint"));
    m_rangeHint = rangeHint;
    form->addRow(rangeHint);
    m_scale = new QLineEdit(editor);
    auto *conversionGroup = new QGroupBox(QStringLiteral("数值换算"), editor);
    m_conversionGroup = conversionGroup;
    auto *conversionForm = new QFormLayout(conversionGroup);
    auto *scalePanel = new QWidget(editor);
    auto *scaleLayout = new QVBoxLayout(scalePanel);
    scaleLayout->setContentsMargins(0, 0, 0, 0);
    scaleLayout->addWidget(m_scale);
    auto *scaleHint = new QLabel(
        QStringLiteral("界面显示值 = 结构体原始值 × 换算比例。例如 m_Start：1234 × 0.01 = 12.34°；界面输入 12.34° 时会换算回原始值 1234。"),
        scalePanel);
    scaleHint->setWordWrap(true);
    scaleHint->setObjectName(QStringLiteral("businessConfigScaleHint"));
    scaleLayout->addWidget(scaleHint);
    conversionForm->addRow(QStringLiteral("换算比例："), scalePanel);
    m_decimals = new QLineEdit(editor);
    form->addRow(QStringLiteral("小数位数："), m_decimals);
    m_decimalsLabel = form->labelForField(m_decimals);
    m_defaultValue = new QLineEdit(editor);
    form->addRow(QStringLiteral("默认值："), m_defaultValue);
    m_defaultLabel = form->labelForField(m_defaultValue);
    auto *optionsPanel = new QWidget(editor);
    m_optionsPanel = optionsPanel;
    auto *optionsLayout = new QVBoxLayout(optionsPanel);
    optionsLayout->setContentsMargins(0, 0, 0, 0);
    m_options = new QTableWidget(0, 2, optionsPanel);
    m_options->setHorizontalHeaderLabels(QStringList() << QStringLiteral("显示标签") << QStringLiteral("选项值"));
    m_options->horizontalHeader()->setStretchLastSection(true);
    m_options->setMinimumHeight(180);
    m_options->setMaximumHeight(260);
    optionsLayout->addWidget(m_options);
    auto *optionButtons = new QHBoxLayout;
    auto *addOption = new QPushButton(QStringLiteral("添加选项"), optionsPanel);
    auto *removeOption = new QPushButton(QStringLiteral("删除选项"), optionsPanel);
    optionButtons->addWidget(addOption);
    optionButtons->addWidget(removeOption);
    optionButtons->addStretch();
    optionsLayout->addLayout(optionButtons);
    form->addRow(QStringLiteral("下拉选项："), optionsPanel);
    m_optionsLabel = form->labelForField(optionsPanel);
    connect(addOption, &QPushButton::clicked, this, &BusinessConfigEditorDialog::addOptionRow);
    connect(removeOption, &QPushButton::clicked, this, &BusinessConfigEditorDialog::removeOptionRow);
    auto *hint = new QLabel(QStringLiteral("默认值也按界面显示值填写。"), editor);
    m_defaultHint = hint;
    hint->setWordWrap(true);
    form->addRow(hint);
    editorLayout->addWidget(controlGroup, 1);
    editorLayout->addWidget(conversionGroup);
    body->addWidget(editorScroll, 2);
    root->addLayout(body, 1);

    auto *buttons = new QHBoxLayout;
    m_status = new QLabel(QStringLiteral("基于当前结构定义创建业务配置。"), this);
    m_status->setTextInteractionFlags(Qt::TextSelectableByMouse);
    root->addWidget(m_status);
    auto *open = new QPushButton(QStringLiteral("打开业务配置 JSON…"), this);
    auto *preview = new QPushButton(QStringLiteral("预览表单"), this);
    auto *save = new QPushButton(QStringLiteral("保存"), this);
    auto *saveAs = new QPushButton(QStringLiteral("另存为…"), this);
    auto *close = new QPushButton(QStringLiteral("关闭"), this);
    buttons->addWidget(open);
    buttons->addStretch();
    buttons->addWidget(preview);
    buttons->addWidget(save);
    buttons->addWidget(saveAs);
    buttons->addWidget(close);
    root->addLayout(buttons);

    addFields(structure.fields, QString(), nullptr);
    m_defaultFields = m_fields;
    connect(m_fieldTree, &QTreeWidget::currentItemChanged, this,
            [this](QTreeWidgetItem *current, QTreeWidgetItem *) {
        saveField();
        selectField(current ? current->data(0, Qt::UserRole).toInt() : -1);
    });
    connect(open, &QPushButton::clicked, this, &BusinessConfigEditorDialog::openConfig);
    connect(preview, &QPushButton::clicked, this, &BusinessConfigEditorDialog::previewForm);
    connect(save, &QPushButton::clicked, this, &BusinessConfigEditorDialog::saveConfig);
    connect(saveAs, &QPushButton::clicked, this, &BusinessConfigEditorDialog::saveConfigAs);
    connect(close, &QPushButton::clicked, this, &QDialog::reject);
    if (!m_fields.isEmpty())
        m_fieldTree->setCurrentItem(m_fieldTree->topLevelItem(0));
}

void BusinessConfigEditorDialog::addFields(const QVector<FieldInfo> &fields, const QString &parentPath,
                                           QTreeWidgetItem *parentItem)
{
    for (const FieldInfo &field : fields) {
        const QString path = parentPath.isEmpty() ? field.name : parentPath + QLatin1Char('.') + field.name;
        FieldConfig config;
        config.path = path;
        config.label = field.name;
        config.widget = field.isArray ? QStringLiteral("readonly") : defaultWidget(field.valueKind);
        const int index = m_fields.size();
        m_fields.append(config);

        auto *item = new QTreeWidgetItem(QStringList() << path << field.typeName);
        item->setData(0, Qt::UserRole, index);
        if (parentItem)
            parentItem->addChild(item);
        else
            m_fieldTree->addTopLevelItem(item);
        if (!field.children.isEmpty())
            addFields(field.children, path, item);
    }
}

void BusinessConfigEditorDialog::selectField(int index)
{
    m_selectedIndex = index;
    const bool selected = index >= 0 && index < m_fields.size();
    const QList<QWidget *> widgets = {m_label, m_widget, m_unit, m_minimum, m_maximum,
                                      m_scale, m_decimals, m_defaultValue, m_options};
    for (QWidget *widget : widgets)
        widget->setEnabled(selected);
    if (!selected)
        return;
    const FieldConfig &field = m_fields.at(index);
    m_label->setText(field.label);
    m_widget->setCurrentText(field.widget);
    m_unit->setText(field.unit);
    m_minimum->setText(field.minimum);
    m_maximum->setText(field.maximum);
    m_scale->setText(field.scale);
    m_decimals->setText(field.decimals);
    m_defaultValue->setText(field.defaultValue);
    QJsonParseError parseError;
    const QJsonDocument optionsDoc = QJsonDocument::fromJson(field.options.toUtf8(), &parseError);
    m_options->setRowCount(0);
    if (parseError.error == QJsonParseError::NoError && optionsDoc.isArray()) {
        const QJsonArray options = optionsDoc.array();
        for (const QJsonValue &value : options) {
            const QJsonObject option = value.toObject();
            const int row = m_options->rowCount();
            m_options->insertRow(row);
            m_options->setItem(row, 0, new QTableWidgetItem(option.value(QStringLiteral("label")).toString()));
            const QJsonValue optionValue = option.value(QStringLiteral("value"));
            auto *valueItem = new QTableWidgetItem(optionValue.isString()
                ? optionValue.toString() : QString::number(optionValue.toDouble(), 'g', 15));
            valueItem->setData(Qt::UserRole, optionValue.isString());
            m_options->setItem(row, 1, valueItem);
        }
    }
    updatePropertyVisibility();
}

void BusinessConfigEditorDialog::updatePropertyVisibility()
{
    const bool selected = m_selectedIndex >= 0 && m_selectedIndex < m_fields.size();
    const QString widget = selected ? m_widget->currentText() : QString();
    const bool integerInput = widget == QLatin1String("spinbox");
    const bool realInput = widget == QLatin1String("doublespinbox");
    const bool numericInput = integerInput || realInput;
    const bool comboInput = widget == QLatin1String("combobox");
    const bool valueInput = numericInput || comboInput || widget == QLatin1String("checkbox")
        || widget == QLatin1String("lineedit");

    m_unit->setVisible(selected && numericInput);
    m_unitLabel->setVisible(selected && numericInput);
    m_minimum->setVisible(selected && numericInput);
    m_minimumLabel->setVisible(selected && numericInput);
    m_maximum->setVisible(selected && numericInput);
    m_maximumLabel->setVisible(selected && numericInput);
    m_rangeHint->setVisible(selected && numericInput);
    m_conversionGroup->setVisible(selected && numericInput);
    m_decimals->setVisible(selected && realInput);
    m_decimalsLabel->setVisible(selected && realInput);
    m_defaultValue->setVisible(selected && valueInput);
    m_defaultLabel->setVisible(selected && valueInput);
    m_defaultHint->setVisible(selected && valueInput);
    m_optionsPanel->setVisible(selected && comboInput);
    m_optionsLabel->setVisible(selected && comboInput);
}

void BusinessConfigEditorDialog::saveField()
{
    if (m_selectedIndex < 0 || m_selectedIndex >= m_fields.size())
        return;
    FieldConfig &field = m_fields[m_selectedIndex];
    field.label = m_label->text().trimmed();
    field.widget = m_widget->currentText();
    field.unit = m_unit->text().trimmed();
    field.minimum = m_minimum->text().trimmed();
    field.maximum = m_maximum->text().trimmed();
    field.scale = m_scale->text().trimmed();
    field.decimals = m_decimals->text().trimmed();
    field.defaultValue = m_defaultValue->text().trimmed();
    field.defaultIsString = false;
    if (field.widget == QLatin1String("combobox")) {
        for (int row = 0; row < m_options->rowCount(); ++row) {
            const QTableWidgetItem *valueItem = m_options->item(row, 1);
            if (valueItem && valueItem->text().trimmed() == field.defaultValue
                && valueItem->data(Qt::UserRole).toBool()) {
                field.defaultIsString = true;
                break;
            }
        }
    }
    QJsonArray options;
    for (int row = 0; row < m_options->rowCount(); ++row) {
        const QString label = m_options->item(row, 0) ? m_options->item(row, 0)->text().trimmed() : QString();
        const QString valueText = m_options->item(row, 1) ? m_options->item(row, 1)->text().trimmed() : QString();
        bool numeric = false;
        const double numericValue = valueText.toDouble(&numeric);
        const bool forceString = m_options->item(row, 1)
            && m_options->item(row, 1)->data(Qt::UserRole).toBool();
        options.append(QJsonObject{
            {QStringLiteral("label"), label},
            {QStringLiteral("value"), numeric && !forceString ? QJsonValue(numericValue) : QJsonValue(valueText)}
        });
    }
    field.options = QString::fromUtf8(QJsonDocument(options).toJson(QJsonDocument::Compact));
}

void BusinessConfigEditorDialog::addOptionRow()
{
    const int row = m_options->rowCount();
    m_options->insertRow(row);
    m_options->setItem(row, 0, new QTableWidgetItem);
    m_options->setItem(row, 1, new QTableWidgetItem);
    m_options->setCurrentCell(row, 0);
    m_options->editItem(m_options->item(row, 0));
}

void BusinessConfigEditorDialog::removeOptionRow()
{
    const int row = m_options->currentRow();
    if (row >= 0)
        m_options->removeRow(row);
}

bool BusinessConfigEditorDialog::validateField(const FieldConfig &field, QString &error) const
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
        double ignored = 0.0;
        if (!parseFinite(field.defaultValue, ignored)) {
            error = QStringLiteral("%1：默认值目前需为有限数字。").arg(field.path);
            return false;
        }
        if ((!field.minimum.isEmpty() && ignored < minimum)
            || (!field.maximum.isEmpty() && ignored > maximum)) {
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
        const QJsonDocument optionsDoc = QJsonDocument::fromJson(field.options.toUtf8(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !optionsDoc.isArray()) {
            error = QStringLiteral("%1：下拉选项必须是有效的 JSON 数组。").arg(field.path);
            return false;
        }
        options = optionsDoc.array();
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
            error = QStringLiteral("%1：每个选项需包含 label 字符串和 value 数字或字符串。").arg(field.path);
            return false;
        }
        if (option.value(QStringLiteral("label")).toString().trimmed().isEmpty()) {
            error = QStringLiteral("%1：选项显示标签不能为空。").arg(field.path);
            return false;
        }
    }
    if (comboInput && !field.defaultValue.isEmpty()) {
        const QJsonValue defaultValue = field.defaultIsString
            ? QJsonValue(field.defaultValue) : numberOrString(field.defaultValue);
        bool foundDefault = false;
        for (const QJsonValue &optionValue : options) {
            if (optionValue.toObject().value(QStringLiteral("value")) == defaultValue) {
                foundDefault = true;
                break;
            }
        }
        if (!foundDefault) {
            error = QStringLiteral("%1：默认值必须对应一个下拉选项的 value。").arg(field.path);
            return false;
        }
    }
    return true;
}

QJsonDocument BusinessConfigEditorDialog::configDocument() const
{
    QJsonObject fieldsObject;
    for (const FieldConfig &field : m_fields) {
        QJsonObject item{
            {QStringLiteral("label"), field.label},
            {QStringLiteral("widget"), field.widget}
        };
        const bool numericInput = field.widget == QLatin1String("spinbox")
            || field.widget == QLatin1String("doublespinbox");
        if (numericInput && !field.unit.isEmpty()) item.insert(QStringLiteral("unit"), field.unit);
        if (numericInput && !field.minimum.isEmpty()) item.insert(QStringLiteral("min"), numberOrString(field.minimum));
        if (numericInput && !field.maximum.isEmpty()) item.insert(QStringLiteral("max"), numberOrString(field.maximum));
        if (numericInput && !field.scale.isEmpty()) item.insert(QStringLiteral("scale"), numberOrString(field.scale));
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
                item.insert(QStringLiteral("default"), numberOrString(field.defaultValue));
        }
        if (field.widget == QLatin1String("combobox")) {
            QJsonParseError parseError;
            const QJsonDocument optionsDoc = QJsonDocument::fromJson(field.options.toUtf8(), &parseError);
            if (parseError.error == QJsonParseError::NoError && optionsDoc.isArray())
                item.insert(QStringLiteral("options"), optionsDoc.array());
        }
        fieldsObject.insert(field.path, item);
    }
    QJsonObject root{
        {QStringLiteral("struct"), m_structure.name},
        {QStringLiteral("title"), m_title->text().trimmed()},
        {QStringLiteral("layout"), QStringLiteral("form")},
        {QStringLiteral("fields"), fieldsObject}
    };
    return QJsonDocument(root);
}

void BusinessConfigEditorDialog::saveConfig()
{
    saveField();
    const QString title = m_title->text().trimmed();
    if (title.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("无法保存"), QStringLiteral("表单标题不能为空。"));
        return;
    }
    for (const FieldConfig &field : m_fields) {
        QString error;
        if (!validateField(field, error)) {
            QMessageBox::warning(this, QStringLiteral("配置有误"), error);
            return;
        }
    }
    if (m_currentConfigPath.isEmpty()) {
        saveConfigAs();
        return;
    }
    writeConfig(m_currentConfigPath);
}

void BusinessConfigEditorDialog::saveConfigAs()
{
    saveField();
    if (m_title->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("无法保存"), QStringLiteral("表单标题不能为空。"));
        return;
    }
    for (const FieldConfig &field : m_fields) {
        QString error;
        if (!validateField(field, error)) {
            QMessageBox::warning(this, QStringLiteral("配置有误"), error);
            return;
        }
    }
    QString fileBase = m_structure.name;
    for (const QChar invalid : QStringLiteral("\\/:*?\"<>|"))
        fileBase.replace(invalid, QLatin1Char('_'));
    const QString suggested = m_currentConfigPath.isEmpty()
        ? fileBase + QStringLiteral(".business.json") : m_currentConfigPath;
    QString path = QFileDialog::getSaveFileName(this, QStringLiteral("保存业务配置 JSON"), suggested,
                                                QStringLiteral("JSON 文件 (*.json);;所有文件 (*)"));
    if (path.isEmpty())
        return;
    if (QFileInfo(path).suffix().isEmpty())
        path += QStringLiteral(".json");
    writeConfig(path);
}

void BusinessConfigEditorDialog::writeConfig(const QString &path)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::critical(this, QStringLiteral("保存失败"), file.errorString());
        return;
    }
    const QByteArray data = configDocument().toJson(QJsonDocument::Indented);
    if (file.write(data) != data.size() || !file.commit()) {
        const QString error = file.errorString();
        file.cancelWriting();
        QMessageBox::critical(this, QStringLiteral("保存失败"), error);
        return;
    }
    m_currentConfigPath = QFileInfo(path).absoluteFilePath();
    m_status->setText(QStringLiteral("当前配置：%1").arg(QDir::toNativeSeparators(m_currentConfigPath)));
    QMessageBox::information(this, QStringLiteral("保存成功"),
        QStringLiteral("业务配置 JSON 已保存到：\n%1").arg(QDir::toNativeSeparators(m_currentConfigPath)));
}

void BusinessConfigEditorDialog::openConfig()
{
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("打开业务配置 JSON"),
        m_currentConfigPath.isEmpty() ? QString() : QFileInfo(m_currentConfigPath).absolutePath(),
        QStringLiteral("业务配置 JSON (*.json);;所有文件 (*)"));
    if (path.isEmpty())
        return;
    QString error;
    if (!loadConfigFile(path, error)) {
        QMessageBox::warning(this, QStringLiteral("无法打开配置"), error);
        return;
    }
    m_status->setText(QStringLiteral("当前配置：%1").arg(QDir::toNativeSeparators(m_currentConfigPath)));
}

void BusinessConfigEditorDialog::previewForm()
{
    saveField();
    if (m_title->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("无法预览"), QStringLiteral("表单标题不能为空。"));
        return;
    }
    for (const FieldConfig &field : m_fields) {
        QString error;
        if (!validateField(field, error)) {
            QMessageBox::warning(this, QStringLiteral("配置有误"), error);
            return;
        }
    }
    BusinessFormPreviewDialog preview(configDocument(), this);
    preview.exec();
}

bool BusinessConfigEditorDialog::loadConfigFile(const QString &path, QString &error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        error = QStringLiteral("无法读取文件：%1").arg(file.errorString());
        return false;
    }
    const int maximumBytes = 16 * 1024 * 1024;
    const QByteArray bytes = file.read(maximumBytes + 1);
    if (file.error() != QFileDevice::NoError) {
        error = QStringLiteral("读取文件失败：%1").arg(file.errorString());
        return false;
    }
    if (bytes.size() > maximumBytes) {
        error = QStringLiteral("业务配置 JSON 超过 16 MiB。");
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        error = QStringLiteral("JSON 格式错误：%1").arg(parseError.errorString());
        return false;
    }
    const QJsonObject root = document.object();
    if (!root.value(QStringLiteral("struct")).isString()
        || root.value(QStringLiteral("struct")).toString() != m_structure.name) {
        error = QStringLiteral("配置中的 struct 必须与当前结构体“%1”一致。").arg(m_structure.name);
        return false;
    }
    if (!root.value(QStringLiteral("title")).isString()
        || root.value(QStringLiteral("title")).toString().trimmed().isEmpty()
        || root.value(QStringLiteral("layout")).toString() != QLatin1String("form")
        || !root.value(QStringLiteral("fields")).isObject()) {
        error = QStringLiteral("配置必须包含非空 title、layout=form 和 fields 对象。");
        return false;
    }

    QVector<FieldConfig> candidate = m_defaultFields;
    QHash<QString, int> fieldIndexes;
    for (int i = 0; i < candidate.size(); ++i)
        fieldIndexes.insert(candidate.at(i).path, i);
    const QJsonObject fieldsObject = root.value(QStringLiteral("fields")).toObject();
    for (auto it = fieldsObject.constBegin(); it != fieldsObject.constEnd(); ++it) {
        if (!fieldIndexes.contains(it.key())) {
            error = QStringLiteral("配置包含当前结构体中不存在的字段：%1").arg(it.key());
            return false;
        }
        if (!it.value().isObject()) {
            error = QStringLiteral("字段 %1 的配置必须是对象。").arg(it.key());
            return false;
        }
        QJsonObject object = it.value().toObject();
        if (!object.value(QStringLiteral("label")).isString()
            || !object.value(QStringLiteral("widget")).isString()) {
            error = QStringLiteral("字段 %1 必须包含字符串 label 和 widget。").arg(it.key());
            return false;
        }
        FieldConfig &field = candidate[fieldIndexes.value(it.key())];
        field.label = object.value(QStringLiteral("label")).toString();
        field.widget = object.value(QStringLiteral("widget")).toString();
        const bool numericWidget = field.widget == QLatin1String("spinbox")
            || field.widget == QLatin1String("doublespinbox");
        const bool doubleWidget = field.widget == QLatin1String("doublespinbox");
        const bool comboWidget = field.widget == QLatin1String("combobox");
        const bool checkboxWidget = field.widget == QLatin1String("checkbox");
        const bool lineEditWidget = field.widget == QLatin1String("lineedit");
        const auto readOptionalString = [&object, &error, &it](const QString &key, QString &target) {
            if (!object.contains(key))
                return true;
            if (!object.value(key).isString()) {
                error = QStringLiteral("字段 %1 的 %2 必须是字符串。").arg(it.key(), key);
                return false;
            }
            target = object.value(key).toString();
            return true;
        };
        const auto readOptionalNumber = [&object, &error, &it](const QString &key, QString &target) {
            if (!object.contains(key))
                return true;
            const QJsonValue value = object.value(key);
            if (!value.isDouble() || !std::isfinite(value.toDouble())) {
                error = QStringLiteral("字段 %1 的 %2 必须是有限数字。").arg(it.key(), key);
                return false;
            }
            target = QString::number(value.toDouble(), 'g', 15);
            return true;
        };
        if (numericWidget
            && (!readOptionalString(QStringLiteral("unit"), field.unit)
                || !readOptionalNumber(QStringLiteral("min"), field.minimum)
                || !readOptionalNumber(QStringLiteral("max"), field.maximum)
                || !readOptionalNumber(QStringLiteral("scale"), field.scale)))
            return false;
        if (doubleWidget && object.contains(QStringLiteral("decimals"))) {
            const QJsonValue decimals = object.value(QStringLiteral("decimals"));
            if (!decimals.isDouble() || std::floor(decimals.toDouble()) != decimals.toDouble()
                || decimals.toDouble() < 0.0 || decimals.toDouble() > 15.0) {
                error = QStringLiteral("字段 %1 的 decimals 必须是 0 到 15 的整数。").arg(it.key());
                return false;
            }
            field.decimals = QString::number(int(decimals.toDouble()));
        }
        if (object.contains(QStringLiteral("default"))) {
            const QJsonValue value = object.value(QStringLiteral("default"));
            if (numericWidget) {
                if (!value.isDouble() || !std::isfinite(value.toDouble())) {
                    error = QStringLiteral("字段 %1 的 default 必须是有限数字。").arg(it.key());
                    return false;
                }
                field.defaultValue = QString::number(value.toDouble(), 'g', 15);
            } else if (comboWidget) {
                if (!(value.isDouble() || value.isString())) {
                    error = QStringLiteral("字段 %1 的组合框 default 必须是数字或字符串。").arg(it.key());
                    return false;
                }
                field.defaultValue = value.isString()
                    ? value.toString() : QString::number(value.toDouble(), 'g', 15);
                field.defaultIsString = value.isString();
            } else if (checkboxWidget) {
                if (!value.isBool()) {
                    error = QStringLiteral("字段 %1 的复选框 default 必须是布尔值。").arg(it.key());
                    return false;
                }
                field.defaultValue = value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
            } else if (lineEditWidget) {
                if (!(value.isString() || value.isDouble())) {
                    error = QStringLiteral("字段 %1 的文本框 default 必须是字符串或数字。").arg(it.key());
                    return false;
                }
                field.defaultValue = value.isString()
                    ? value.toString() : QString::number(value.toDouble(), 'g', 15);
            }
        }
        if (comboWidget && object.contains(QStringLiteral("options"))) {
            if (!object.value(QStringLiteral("options")).isArray()) {
                error = QStringLiteral("字段 %1 的 options 必须是数组。").arg(it.key());
                return false;
            }
            field.options = QString::fromUtf8(QJsonDocument(object.value(QStringLiteral("options")).toArray())
                                                  .toJson(QJsonDocument::Compact));
        }
    }

    for (const FieldConfig &field : candidate) {
        if (!validateField(field, error))
            return false;
    }
    m_fields = candidate;
    m_title->setText(root.value(QStringLiteral("title")).toString());
    m_currentConfigPath = QFileInfo(path).absoluteFilePath();
    selectField(m_selectedIndex);
    return true;
}
