#include "businesshexdecoder.h"

#include "binarydecoder.h"
#include "businessconfigutils.h"
#include "definitionjson.h"
#include "fieldvalue.h"

#include <QComboBox>
#include <QAbstractItemView>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSettings>
#include <QSpinBox>
#include <QSplitter>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidget>

#include <cmath>
#include <limits>

namespace {

const char businessConfigPathKey[] = "businessDataCodec/businessConfigPath";
const char structureDefinitionPathKey[] = "businessDataCodec/structureDefinitionPath";
const char businessConfigDirectoryKey[] = "businessDataCodec/businessConfigDirectory";
const char structureDefinitionDirectoryKey[] = "businessDataCodec/structureDefinitionDirectory";
const char hexDataPathKey[] = "businessDataCodec/hexDataPath";
const char hexDataDirectoryKey[] = "businessDataCodec/hexDataDirectory";

QString dialogStartLocation(const QString &currentPath, const char *directoryKey)
{
    const QString current = currentPath.trimmed();
    if (!current.isEmpty()) {
        const QFileInfo info(current);
        if (info.exists())
            return info.absoluteFilePath();
        if (QDir(info.absolutePath()).exists())
            return info.absolutePath();
    }
    QSettings settings(QStringLiteral("StructHex"), QStringLiteral("StructHex"));
    const QString remembered = settings.value(QLatin1String(directoryKey)).toString();
    return QDir(remembered).exists() ? remembered : QDir::homePath();
}

void rememberFileLocation(const QString &path, const char *fileKey, const char *directoryKey)
{
    if (path.trimmed().isEmpty())
        return;
    const QFileInfo info(path);
    QSettings settings(QStringLiteral("StructHex"), QStringLiteral("StructHex"));
    settings.setValue(QLatin1String(fileKey), info.absoluteFilePath());
    settings.setValue(QLatin1String(directoryKey), info.absolutePath());
    settings.sync();
}

QString byteOffset(qint64 bits)
{
    if (bits % 8 == 0)
        return QString::number(bits / 8);
    return QStringLiteral("%1 bit").arg(bits);
}

bool parseNumber(const QString &text, long double &number)
{
    const QString value = text.trimmed();
    if (value.isEmpty())
        return false;
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

QString jsonValueText(const QJsonValue &value)
{
    if (value.isString()) return value.toString();
    if (value.isBool()) return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    if (value.isDouble()) return QString::number(value.toDouble(), 'g', 15);
    return QString();
}

double configuredNumber(const QJsonObject &object, const QString &key, double fallback)
{
    const QJsonValue value = object.value(key);
    if (value.isDouble()) return value.toDouble();
    if (value.isString()) {
        bool ok = false;
        const double number = value.toString().toDouble(&ok);
        if (ok) return number;
    }
    return fallback;
}

void appendFieldInfo(const QVector<FieldInfo> &fields, const QString &parentPath,
                     QHash<QString, FieldInfo> &output)
{
    for (const FieldInfo &field : fields) {
        const QString path = parentPath.isEmpty() ? field.name : parentPath + QLatin1Char('.') + field.name;
        output.insert(path, field);
        if (!field.children.isEmpty()) appendFieldInfo(field.children, path, output);
    }
}

void appendDecodedValues(const QVector<DecodedField> &fields, QHash<QString, QString> &output)
{
    for (const DecodedField &field : fields) {
        output.insert(field.path, field.value);
        appendDecodedValues(field.children, output);
    }
}

bool optionMatches(const QJsonValue &optionValue, const QString &rawValue)
{
    const QString optionText = jsonValueText(optionValue);
    long double optionNumber = 0.0L, rawNumber = 0.0L;
    if (parseNumber(optionText, optionNumber) && parseNumber(rawValue, rawNumber))
        return optionNumber == rawNumber;
    return optionText.compare(rawValue, Qt::CaseInsensitive) == 0;
}

QString businessValue(const DecodedField &decoded, const QJsonObject &config, QString &extraNote)
{
    const QString widget = config.value(QStringLiteral("widget")).toString();
    const QString raw = decoded.value;
    if (raw == QLatin1String("分组") || raw == QLatin1String("原始字节"))
        return raw;

    if (widget == QLatin1String("combobox")) {
        const QJsonArray options = config.value(QStringLiteral("options")).toArray();
        for (const QJsonValue &value : options) {
            const QJsonObject option = value.toObject();
            if (optionMatches(option.value(QStringLiteral("value")), raw))
                return option.value(QStringLiteral("label")).toString();
        }
        extraNote = QStringLiteral("该原始值未配置对应的下拉选项。 ");
        return QStringLiteral("未配置选项（%1）").arg(raw);
    }

    if (widget == QLatin1String("checkbox")) {
        return (raw == QLatin1String("true") || raw == QLatin1String("1"))
            ? QStringLiteral("已选中") : QStringLiteral("未选中");
    }

    if (widget == QLatin1String("spinbox") || widget == QLatin1String("doublespinbox")) {
        long double rawNumber = 0.0L;
        if (!parseNumber(raw, rawNumber))
            return raw;
        const QJsonValue scaleValue = config.value(QStringLiteral("scale"));
        const double scale = scaleValue.isDouble() ? scaleValue.toDouble() : scaleValue.toString().toDouble();
        const long double display = rawNumber * (scale > 0.0 && std::isfinite(scale) ? scale : 1.0);
        const int decimals = config.value(QStringLiteral("decimals")).toInt(-1);
        QString text = decimals >= 0
            ? QString::number(static_cast<double>(display), 'f', decimals)
            : QString::number(static_cast<double>(display), 'g', 15);
        const QString unit = config.value(QStringLiteral("unit")).toString().trimmed();
        if (!unit.isEmpty()) text += QLatin1Char(' ') + unit;
        return text;
    }

    return raw;
}

void appendResults(QTreeWidgetItem *parent, const QVector<DecodedField> &fields,
                   const QJsonObject &businessFields)
{
    for (const DecodedField &field : fields) {
        const QJsonObject config = businessFields.value(field.path).toObject();
        const QString configuredLabel = config.value(QStringLiteral("label")).toString().trimmed();
        const QString label = configuredLabel.isEmpty() ? field.name : configuredLabel;
        QString extraNote;
        const QString display = businessValue(field, config, extraNote);
        const QString rawBytes = QString::fromLatin1(field.bytes.left(32).toHex(' ').toUpper())
            + (field.bytes.size() > 32 ? QStringLiteral(" …（共 %1 字节）").arg(field.bytes.size()) : QString());
        QString note = field.note;
        if (!extraNote.isEmpty()) note += (note.isEmpty() ? QString() : QStringLiteral(" ")) + extraNote;
        auto *item = new QTreeWidgetItem(parent, {field.name, label, field.typeName,
            byteOffset(field.offsetBits), rawBytes, field.value, display, note});
        item->setToolTip(0, field.path);
        item->setToolTip(3, QStringLiteral("绝对偏移 %1 bit，字段宽度 %2 bit；位域原始字节可能含相邻位。")
                         .arg(field.offsetBits).arg(field.widthBits));
        appendResults(item, field.children, businessFields);
    }
}

} // namespace

BusinessHexDecoderDialog::BusinessHexDecoderDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(QStringLiteral("业务数据编解码"));
    setObjectName(QStringLiteral("businessHexDecoderDialog"));
    resize(1660, 900);
    setMinimumSize(1200, 680);

    auto *layout = new QVBoxLayout(this);
    auto *intro = new QLabel(QStringLiteral(
        "选择结构定义 JSON 和对应的业务配置 JSON，再粘贴十六进制数据。结构定义负责解析布局，业务配置负责显示字段含义、单位、换算值和选项标签。"), this);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    auto makePathRow = [this](QLineEdit *&edit, const QString &placeholder, QPushButton *&browse) {
        auto *row = new QWidget(this);
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        edit = new QLineEdit(row);
        edit->setPlaceholderText(placeholder);
        browse = new QPushButton(QStringLiteral("浏览…"), row);
        rowLayout->addWidget(edit, 1);
        rowLayout->addWidget(browse);
        return row;
    };
    auto *form = new QFormLayout;
    QPushButton *businessBrowse = nullptr;
    QPushButton *structureBrowse = nullptr;
    form->addRow(QStringLiteral("业务配置 JSON："), makePathRow(m_businessPath,
        QStringLiteral("选择 .business.json"), businessBrowse));
    form->addRow(QStringLiteral("结构定义 JSON："), makePathRow(m_structurePath,
        QStringLiteral("选择结构定义 JSON"), structureBrowse));
    QSettings recentSettings(QStringLiteral("StructHex"), QStringLiteral("StructHex"));
    const QString savedBusinessPath = recentSettings.value(QLatin1String(businessConfigPathKey)).toString();
    const QString savedStructurePath = recentSettings.value(QLatin1String(structureDefinitionPathKey)).toString();
    const bool restoreBusiness = QFileInfo(savedBusinessPath).isFile();
    const bool restoreStructure = QFileInfo(savedStructurePath).isFile();
    if (restoreBusiness)
        m_businessPath->setText(QDir::toNativeSeparators(savedBusinessPath));
    if (restoreStructure)
        m_structurePath->setText(QDir::toNativeSeparators(savedStructurePath));
    layout->addLayout(form);
    connect(businessBrowse, &QPushButton::clicked, this, &BusinessHexDecoderDialog::chooseBusinessConfig);
    connect(structureBrowse, &QPushButton::clicked, this, &BusinessHexDecoderDialog::chooseStructureDefinition);
    const auto pathsChanged = [this] {
        m_legacyConfigAccepted = false;
        if (m_hexGeneratedByForm) {
            const QSignalBlocker blocker(m_hexInput);
            m_hexInput->clear();
            m_copyHexButton->setEnabled(false);
            m_hexGeneratedByForm = false;
        }
        clearResult(QStringLiteral("文件路径已更改，请重新解析。"));
    };
    connect(m_businessPath, &QLineEdit::textChanged, this, pathsChanged);
    connect(m_structurePath, &QLineEdit::textChanged, this, pathsChanged);
    connect(m_businessPath, &QLineEdit::editingFinished, this, &BusinessHexDecoderDialog::initializeDefaultsIfReady);
    connect(m_structurePath, &QLineEdit::editingFinished, this, &BusinessHexDecoderDialog::initializeDefaultsIfReady);

    auto *workArea = new QSplitter(Qt::Horizontal, this);
    workArea->setObjectName(QStringLiteral("businessHexWorkArea"));
    auto *leftPane = new QWidget(workArea);
    auto *leftLayout = new QVBoxLayout(leftPane);
    leftLayout->setContentsMargins(0, 0, 6, 0);
    auto *hexRow = new QWidget(leftPane);
    auto *hexLayout = new QHBoxLayout(hexRow);
    hexLayout->setContentsMargins(0, 0, 0, 0);
    auto *hexLabel = new QLabel(QStringLiteral("当前十六进制数据："), hexRow);
    QPushButton *hexBrowse = new QPushButton(QStringLiteral("从文件读取…"), hexRow);
    m_copyHexButton = new QPushButton(QStringLiteral("复制当前 HEX"), hexRow);
    m_copyHexButton->setEnabled(false);
    hexLayout->addWidget(hexLabel);
    hexLayout->addStretch();
    hexLayout->addWidget(hexBrowse);
    hexLayout->addWidget(m_copyHexButton);
    leftLayout->addWidget(hexRow);
    m_hexInput = new QPlainTextEdit(leftPane);
    m_hexInput->setObjectName(QStringLiteral("businessHexInput"));
    m_hexInput->setPlaceholderText(QStringLiteral("粘贴十六进制字节，例如：22 01 94 11 28 00 12 00\n右侧表单编辑后，这里的当前 HEX 会同步更新。"));
    m_hexInput->setMaximumHeight(150);
    leftLayout->addWidget(m_hexInput);
    connect(hexBrowse, &QPushButton::clicked, this, &BusinessHexDecoderDialog::chooseHexFile);
    connect(m_hexInput, &QPlainTextEdit::textChanged, this, [this] {
        m_hexGeneratedByForm = false;
        m_copyHexButton->setEnabled(!m_hexInput->toPlainText().trimmed().isEmpty());
        clearResult(QStringLiteral("输入已更新，请重新解析。"));
    });
    connect(m_copyHexButton, &QPushButton::clicked, this, [this] {
        if (!m_hexInput->toPlainText().trimmed().isEmpty())
            QApplication::clipboard()->setText(m_hexInput->toPlainText());
    });

    auto *actionRow = new QHBoxLayout;
    actionRow->addWidget(new QLabel(QStringLiteral("数据字节序："), leftPane));
    m_byteOrder = new QComboBox(leftPane);
    m_byteOrder->addItems({QStringLiteral("小端"), QStringLiteral("大端")});
    m_byteOrder->setToolTip(QStringLiteral("字段偏移和对齐由结构定义 JSON 决定；此选项只决定字段数值的字节解释。大端位域暂不支持。"));
    actionRow->addWidget(m_byteOrder);
    connect(m_byteOrder, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this] { clearResult(QStringLiteral("字节序已更改，请重新解析。")); });
    actionRow->addStretch();
    auto *parse = new QPushButton(QStringLiteral("解析并显示业务含义"), leftPane);
    parse->setObjectName(QStringLiteral("decodeBusinessHexButton"));
    actionRow->addWidget(parse);
    leftLayout->addLayout(actionRow);

    m_status = new QLabel(QStringLiteral("尚未解析。"), leftPane);
    m_status->setWordWrap(true);
    m_status->setTextInteractionFlags(Qt::TextSelectableByMouse);
    leftLayout->addWidget(m_status);
    if (restoreBusiness || restoreStructure)
        m_status->setText(QStringLiteral("已恢复上次选择的文件路径；文件仍存在时会自动加载。"));
    m_results = new QTreeWidget(leftPane);
    m_results->setObjectName(QStringLiteral("businessDecodedFields"));
    m_results->setHeaderLabels({QStringLiteral("结构字段"), QStringLiteral("业务名称"), QStringLiteral("C++ 类型"),
        QStringLiteral("字节偏移"), QStringLiteral("原始字节"), QStringLiteral("结构值"),
        QStringLiteral("业务显示值"), QStringLiteral("说明")});
    m_results->setAlternatingRowColors(true);
    m_results->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_results->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_results->header()->setStretchLastSection(true);
    leftLayout->addWidget(m_results, 1);

    m_businessForm = new QWidget(workArea);
    auto *rightLayout = new QVBoxLayout(m_businessForm);
    rightLayout->setContentsMargins(6, 0, 0, 0);
    auto *formTitle = new QLabel(QStringLiteral("业务表单（编辑后自动更新左侧 HEX）"), m_businessForm);
    QFont formTitleFont = formTitle->font();
    formTitleFont.setBold(true);
    formTitle->setFont(formTitleFont);
    rightLayout->addWidget(formTitle);
    auto *formScroll = new QScrollArea(m_businessForm);
    formScroll->setWidgetResizable(true);
    m_formContent = new QWidget(formScroll);
    auto *emptyFormLayout = new QVBoxLayout(m_formContent);
    emptyFormLayout->addWidget(new QLabel(QStringLiteral("选择文件并解析十六进制后，业务表单会显示在这里。"), m_formContent));
    emptyFormLayout->addStretch();
    formScroll->setWidget(m_formContent);
    rightLayout->addWidget(formScroll, 1);
    workArea->addWidget(leftPane);
    workArea->addWidget(m_businessForm);
    workArea->setStretchFactor(0, 3);
    workArea->setStretchFactor(1, 2);
    layout->addWidget(workArea, 1);

    auto *close = new QPushButton(QStringLiteral("关闭"), this);
    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    buttons->addWidget(close);
    layout->addLayout(buttons);
    connect(parse, &QPushButton::clicked, this, &BusinessHexDecoderDialog::decode);
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    if (restoreBusiness && restoreStructure)
        QTimer::singleShot(0, this, &BusinessHexDecoderDialog::initializeDefaultsIfReady);
}

void BusinessHexDecoderDialog::chooseBusinessConfig()
{
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("选择业务配置 JSON"),
        dialogStartLocation(m_businessPath->text(), businessConfigDirectoryKey),
        QStringLiteral("业务配置 JSON (*.json);;所有文件 (*)"));
    if (!path.isEmpty()) {
        m_businessPath->setText(QDir::toNativeSeparators(path));
        rememberFileLocation(path, businessConfigPathKey, businessConfigDirectoryKey);
        initializeDefaultsIfReady();
    }
}

void BusinessHexDecoderDialog::chooseStructureDefinition()
{
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("选择结构定义 JSON"),
        dialogStartLocation(m_structurePath->text(), structureDefinitionDirectoryKey),
        QStringLiteral("结构定义 JSON (*.json);;所有文件 (*)"));
    if (!path.isEmpty()) {
        m_structurePath->setText(QDir::toNativeSeparators(path));
        rememberFileLocation(path, structureDefinitionPathKey, structureDefinitionDirectoryKey);
        initializeDefaultsIfReady();
    }
}

void BusinessHexDecoderDialog::initializeDefaultsIfReady()
{
    if (!m_businessPath->text().trimmed().isEmpty()
        && !m_structurePath->text().trimmed().isEmpty()
        && m_hexInput->toPlainText().trimmed().isEmpty())
        decode();
}

void BusinessHexDecoderDialog::chooseHexFile()
{
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("打开十六进制文本"),
        dialogStartLocation(QString(), hexDataDirectoryKey),
        QStringLiteral("十六进制文本 (*.hex *.txt);;所有文件 (*)"));
    if (path.isEmpty()) return;
    rememberFileLocation(path, hexDataPathKey, hexDataDirectoryKey);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, QStringLiteral("无法读取文件"), file.errorString());
        return;
    }
    const QByteArray bytes = file.read(8 * 1024 * 1024 + 1);
    if (file.error() != QFileDevice::NoError) {
        QMessageBox::warning(this, QStringLiteral("读取失败"), file.errorString());
        return;
    }
    if (bytes.size() > 8 * 1024 * 1024) {
        QMessageBox::warning(this, QStringLiteral("文件过大"), QStringLiteral("十六进制文本最大为 8 MiB。"));
        return;
    }
    QByteArray text = bytes;
    if (text.startsWith(QByteArray::fromHex("efbbbf")))
        text.remove(0, 3);
    m_hexInput->setPlainText(QString::fromUtf8(text));
}

void BusinessHexDecoderDialog::rebuildBusinessForm(const QJsonObject &fields, const StructInfo &structure)
{
    m_fieldWidgets.clear();
    m_fieldConfigs = fields;
    m_fieldInfo.clear();
    appendFieldInfo(structure.fields, QString(), m_fieldInfo);

    if (QLayout *oldLayout = m_formContent->layout()) {
        while (QLayoutItem *item = oldLayout->takeAt(0)) {
            if (QWidget *widget = item->widget()) delete widget;
            delete item;
        }
        delete oldLayout;
    }
    auto *form = new QFormLayout(m_formContent);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    for (auto it = fields.constBegin(); it != fields.constEnd(); ++it) {
        if (!it.value().isObject()) continue;
        const QString path = it.key();
        const QJsonObject config = it.value().toObject();
        const QString widgetType = config.value(QStringLiteral("widget")).toString();
        const QString labelText = config.value(QStringLiteral("label")).toString(path);
        const QString unit = config.value(QStringLiteral("unit")).toString().trimmed();
        const QString formLabel = unit.isEmpty() ? labelText + QLatin1Char(':')
            : labelText + QLatin1Char('(') + unit + QStringLiteral("):");
        QWidget *editor = nullptr;
        if (widgetType == QLatin1String("spinbox")) {
            auto *spin = new QSpinBox(m_formContent);
            spin->setRange(int(configuredNumber(config, QStringLiteral("min"), 0)),
                           int(configuredNumber(config, QStringLiteral("max"), 99)));
            if (config.contains(QStringLiteral("default")))
                spin->setValue(int(configuredNumber(config, QStringLiteral("default"), 0)));
            editor = spin;
            connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), this,
                    [this] { businessFormEdited(); });
        } else if (widgetType == QLatin1String("doublespinbox")) {
            auto *spin = new QDoubleSpinBox(m_formContent);
            const int decimals = int(configuredNumber(config, QStringLiteral("decimals"), 2));
            spin->setDecimals(decimals);
            spin->setSingleStep(std::pow(10.0, -decimals));
            spin->setRange(configuredNumber(config, QStringLiteral("min"), -999999.0),
                           configuredNumber(config, QStringLiteral("max"), 999999.0));
            if (config.contains(QStringLiteral("default")))
                spin->setValue(configuredNumber(config, QStringLiteral("default"), 0.0));
            editor = spin;
            connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                    [this] { businessFormEdited(); });
        } else if (widgetType == QLatin1String("combobox")) {
            auto *combo = new QComboBox(m_formContent);
            combo->setObjectName(QStringLiteral("businessSemanticCombo"));
            combo->setStyleSheet(QStringLiteral(
                "QComboBox#businessSemanticCombo { padding-top: 2px; padding-bottom: 2px; min-height: 0px; }"));
            const QJsonArray options = config.value(QStringLiteral("options")).toArray();
            for (const QJsonValue &value : options) {
                const QJsonObject option = value.toObject();
                combo->addItem(option.value(QStringLiteral("label")).toString(),
                               option.value(QStringLiteral("value")).toVariant());
            }
            if (config.contains(QStringLiteral("default"))) {
                const QString defaultText = jsonValueText(config.value(QStringLiteral("default")));
                for (int index = 0; index < combo->count(); ++index) {
                    if (optionMatches(QJsonValue::fromVariant(combo->itemData(index)), defaultText)) {
                        combo->setCurrentIndex(index);
                        break;
                    }
                }
            }
            editor = combo;
            connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                    [this] { businessFormEdited(); });
        } else if (widgetType == QLatin1String("checkbox")) {
            auto *check = new QCheckBox(m_formContent);
            const QJsonValue defaultValue = config.value(QStringLiteral("default"));
            check->setChecked(defaultValue.isBool() ? defaultValue.toBool()
                : defaultValue.isDouble() ? defaultValue.toDouble() != 0.0
                : defaultValue.toString().compare(QLatin1String("true"), Qt::CaseInsensitive) == 0);
            editor = check;
            connect(check, &QCheckBox::toggled, this, [this] { businessFormEdited(); });
        } else if (widgetType == QLatin1String("lineedit")) {
            auto *line = new QLineEdit(m_formContent);
            line->setText(jsonValueText(config.value(QStringLiteral("default"))));
            editor = line;
            connect(line, &QLineEdit::textChanged, this, [this] { businessFormEdited(); });
        } else {
            auto *readOnly = new QLabel(widgetType == QLatin1String("group")
                ? QStringLiteral("结构体分组") : QStringLiteral("只读字段"), m_formContent);
            readOnly->setTextInteractionFlags(Qt::TextSelectableByMouse);
            editor = readOnly;
        }
        editor->setObjectName(path);
        if (!isEditableField(m_fieldInfo.value(path)))
            editor->setEnabled(false);
        m_fieldWidgets.insert(path, editor);
        form->addRow(formLabel, editor);
    }
}

void BusinessHexDecoderDialog::populateBusinessForm(const DecodeResult &decoded)
{
    QHash<QString, QString> decodedValues;
    appendDecodedValues(decoded.fields, decodedValues);
    m_populatingForm = true;
    for (auto it = m_fieldWidgets.constBegin(); it != m_fieldWidgets.constEnd(); ++it) {
        const QString path = it.key();
        QWidget *editor = it.value();
        const QJsonObject config = m_fieldConfigs.value(path).toObject();
        const QString widgetType = config.value(QStringLiteral("widget")).toString();
        const QString raw = decodedValues.value(path);
        if (!isEditableField(m_fieldInfo.value(path))) {
            if (QLabel *label = qobject_cast<QLabel *>(editor))
                label->setText(raw.isEmpty() ? QStringLiteral("（无可解析数值）") : raw);
            continue;
        }
        const long double scaleValue = configuredNumber(config, QStringLiteral("scale"), 1.0);
        const double scale = scaleValue > 0.0 && std::isfinite(double(scaleValue)) ? double(scaleValue) : 1.0;

        if (widgetType == QLatin1String("spinbox")) {
            long double rawNumber = 0.0L;
            if (parseNumber(raw, rawNumber)) {
                QSpinBox *spin = qobject_cast<QSpinBox *>(editor);
                const long double displayed = std::round(rawNumber * scale);
                const int target = displayed < std::numeric_limits<int>::min()
                    ? std::numeric_limits<int>::min()
                    : displayed > std::numeric_limits<int>::max()
                        ? std::numeric_limits<int>::max() : int(displayed);
                if (target < spin->minimum() || target > spin->maximum()) {
                    spin->setRange(qMin(target, spin->minimum()), qMax(target, spin->maximum()));
                    spin->setValue(target);
                    spin->setEnabled(false);
                    spin->setToolTip(QStringLiteral("当前结构值超出业务配置的界面范围；为避免静默改变数据，此字段暂时禁用。"));
                } else {
                    spin->setValue(target);
                }
                if (displayed < std::numeric_limits<int>::min() || displayed > std::numeric_limits<int>::max())
                    spin->setToolTip(QStringLiteral("当前值超出 UI 控件可表示范围；为避免静默改变数据，此字段暂时禁用。"));
            }
        } else if (widgetType == QLatin1String("doublespinbox")) {
            long double rawNumber = 0.0L;
            if (parseNumber(raw, rawNumber)) {
                QDoubleSpinBox *spin = qobject_cast<QDoubleSpinBox *>(editor);
                const double target = double(rawNumber * scale);
                if (target < spin->minimum() || target > spin->maximum()) {
                    spin->setRange(qMin(target, spin->minimum()), qMax(target, spin->maximum()));
                    spin->setValue(target);
                    spin->setEnabled(false);
                    spin->setToolTip(QStringLiteral("当前结构值超出业务配置的界面范围；为避免静默改变数据，此字段暂时禁用。"));
                } else {
                    spin->setValue(target);
                }
            }
        } else if (widgetType == QLatin1String("combobox")) {
            QComboBox *combo = qobject_cast<QComboBox *>(editor);
            int selected = -1;
            for (int index = 0; index < combo->count(); ++index) {
                if (optionMatches(combo->itemData(index).isNull()
                                      ? QJsonValue() : QJsonValue::fromVariant(combo->itemData(index)), raw)) {
                    selected = index;
                    break;
                }
            }
            if (selected < 0 && !raw.isEmpty()) {
                combo->addItem(QStringLiteral("当前值未配置（%1）").arg(raw), raw);
                selected = combo->count() - 1;
            }
            combo->setCurrentIndex(selected);
        } else if (widgetType == QLatin1String("checkbox")) {
            const bool checked = raw == QLatin1String("true") || raw == QLatin1String("1");
            qobject_cast<QCheckBox *>(editor)->setChecked(checked);
        } else if (widgetType == QLatin1String("lineedit")) {
            qobject_cast<QLineEdit *>(editor)->setText(raw);
        } else if (QLabel *label = qobject_cast<QLabel *>(editor)) {
            label->setText(raw.isEmpty() ? QStringLiteral("（无可解析数值）") : raw);
        }
    }
    m_populatingForm = false;
    m_businessForm->setEnabled(m_byteOrder->currentIndex() == 0);
}

bool BusinessHexDecoderDialog::collectBusinessValues(const QString &fieldPath,
                                                     QHash<QString, QString> &values,
                                                     QString &error) const
{
    values.clear();
    for (auto it = m_fieldWidgets.constBegin(); it != m_fieldWidgets.constEnd(); ++it) {
        const QString path = it.key();
        if (!fieldPath.isEmpty() && path != fieldPath)
            continue;
        QWidget *editor = it.value();
        if (!isEditableField(m_fieldInfo.value(path)))
            continue;
        const QJsonObject config = m_fieldConfigs.value(path).toObject();
        const QString widgetType = config.value(QStringLiteral("widget")).toString();
        const FieldInfo field = m_fieldInfo.value(path);
        QString raw;
        if (widgetType == QLatin1String("spinbox") || widgetType == QLatin1String("doublespinbox")) {
            const double display = widgetType == QLatin1String("spinbox")
                ? qobject_cast<QSpinBox *>(editor)->value()
                : qobject_cast<QDoubleSpinBox *>(editor)->value();
            const double scale = configuredNumber(config, QStringLiteral("scale"), 1.0);
            if (!std::isfinite(scale) || scale <= 0.0) {
                error = QStringLiteral("%1：换算比例必须是大于 0 的有限数字。").arg(path);
                return false;
            }
            const long double value = static_cast<long double>(display) / scale;
            if (field.valueKind == FieldValueKind::SignedInteger
                || field.valueKind == FieldValueKind::UnsignedInteger
                || field.valueKind == FieldValueKind::Boolean) {
                const long double rounded = std::round(value);
                const long double tolerance = 1e-9L * (std::fabs(value) > 1.0L ? std::fabs(value) : 1.0L);
                if (std::fabs(value - rounded) > tolerance) {
                    error = QStringLiteral("%1：按换算比例还原后不是整数。").arg(path);
                    return false;
                }
                if (field.valueKind == FieldValueKind::Boolean) {
                    if (rounded != 0.0L && rounded != 1.0L) {
                        error = QStringLiteral("%1：布尔字段换算结果只能为 0 或 1。").arg(path);
                        return false;
                    }
                    raw = rounded == 1.0L ? QStringLiteral("true") : QStringLiteral("false");
                } else if (field.valueKind == FieldValueKind::SignedInteger) {
                    if (rounded < static_cast<long double>(std::numeric_limits<qint64>::min())
                        || rounded > static_cast<long double>(std::numeric_limits<qint64>::max())) {
                        error = QStringLiteral("%1：换算后的整数超出范围。").arg(path);
                        return false;
                    }
                    raw = QString::number(static_cast<qint64>(rounded));
                } else {
                    if (rounded < 0.0L
                        || rounded > static_cast<long double>(std::numeric_limits<quint64>::max())) {
                        error = QStringLiteral("%1：换算后的无符号整数超出范围。").arg(path);
                        return false;
                    }
                    raw = QString::number(static_cast<quint64>(rounded));
                }
            } else {
                raw = QString::number(double(value), 'g', 17);
            }
        } else if (widgetType == QLatin1String("combobox")) {
            raw = qobject_cast<QComboBox *>(editor)->currentData().toString();
            if (raw.isEmpty()) {
                error = QStringLiteral("%1：请先选择一个下拉选项。").arg(path);
                return false;
            }
            if (field.valueKind == FieldValueKind::Boolean) {
                if (raw == QLatin1String("0")) raw = QStringLiteral("false");
                else if (raw == QLatin1String("1")) raw = QStringLiteral("true");
            }
        } else if (widgetType == QLatin1String("checkbox")) {
            const bool checked = qobject_cast<QCheckBox *>(editor)->isChecked();
            raw = field.valueKind == FieldValueKind::Boolean
                ? (checked ? QStringLiteral("true") : QStringLiteral("false"))
                : (checked ? QStringLiteral("1") : QStringLiteral("0"));
        } else if (widgetType == QLatin1String("lineedit")) {
            raw = qobject_cast<QLineEdit *>(editor)->text();
        } else {
            continue;
        }
        values.insert(path, raw);
    }
    error.clear();
    return true;
}

void BusinessHexDecoderDialog::renderResults(const DecodeResult &decoded, const StructInfo &structure,
                                             const QJsonObject &config)
{
    m_results->clear();
    const QJsonObject fields = config.value(QStringLiteral("fields")).toObject();
    auto *root = new QTreeWidgetItem(m_results, {structure.name, config.value(QStringLiteral("title")).toString(),
        QStringLiteral("struct"), QStringLiteral("0"), QString(), QStringLiteral("%1 字节").arg(structure.size), QString(), QString()});
    appendResults(root, decoded.fields, fields);
    m_results->expandAll();
}

void BusinessHexDecoderDialog::businessFormEdited()
{
    if (m_populatingForm || m_currentBytes.isEmpty() || m_byteOrder->currentIndex() != 0)
        return;
    QWidget *editedWidget = qobject_cast<QWidget *>(sender());
    if (!editedWidget)
        return;
    QHash<QString, QString> values;
    QString error;
    if (!collectBusinessValues(editedWidget->objectName(), values, error)) {
        m_status->setText(error);
        return;
    }
    if (values.isEmpty())
        return;
    const BinaryResult encoded = encodeStructure(m_activeStructure, values, m_currentBytes);
    if (!encoded.success) {
        m_status->setText(encoded.diagnostics.join(QLatin1Char('\n')));
        return;
    }
    const DecodeResult decoded = decodeStructure(m_activeStructure, encoded.bytes, BinaryByteOrder::LittleEndian);
    if (!decoded.success) {
        m_status->setText(decoded.diagnostics.join(QLatin1Char('\n')));
        return;
    }
    m_currentBytes = encoded.bytes;
    {
        const QSignalBlocker blocker(m_hexInput);
        m_hexInput->setPlainText(QString::fromLatin1(encoded.bytes.toHex(' ').toUpper()));
    }
    renderResults(decoded, m_activeStructure, m_activeConfig.object());
    m_status->setText(QStringLiteral("已根据右侧业务表单重新生成 HEX，并同步刷新左侧字段解析结果。未编辑的只读字段、数组和填充字节已保留。"));
}

void BusinessHexDecoderDialog::clearResult(const QString &message)
{
    m_results->clear();
    m_status->setText(message);
    m_currentBytes.clear();
    m_businessForm->setEnabled(false);
}

void BusinessHexDecoderDialog::decode()
{
    clearResult(QStringLiteral("正在解析…"));
    const QString businessPath = m_businessPath->text().trimmed();
    const QString structurePath = m_structurePath->text().trimmed();
    if (businessPath.isEmpty() || structurePath.isEmpty()) {
        clearResult(QStringLiteral("请先选择业务配置 JSON 和结构定义 JSON。"));
        return;
    }
    QJsonDocument config;
    QString error;
    if (!loadBusinessConfig(businessPath, config, error)) {
        clearResult(error);
        return;
    }
    const ParseResult structureResult = loadStructureDefinitionJson(structurePath);
    if (!structureResult.success) {
        clearResult(structureResult.diagnostics.join(QLatin1Char('\n')));
        return;
    }
    const StructInfo structure = structureResult.structure;
    if (!validateBusinessConfig(config, structure, error)) {
        clearResult(error);
        return;
    }
    rememberFileLocation(businessPath, businessConfigPathKey, businessConfigDirectoryKey);
    rememberFileLocation(structurePath, structureDefinitionPathKey, structureDefinitionDirectoryKey);
    if (!config.object().contains(QStringLiteral("structureFingerprint")) && !m_legacyConfigAccepted) {
        const auto answer = QMessageBox::question(this, QStringLiteral("旧版业务配置"),
            QStringLiteral("此业务配置没有结构布局指纹，只能校验结构体名称和字段路径。仍要继续吗？"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes) {
            clearResult(QStringLiteral("已取消解析。"));
            return;
        }
        m_legacyConfigAccepted = true;
    }
    rebuildBusinessForm(config.object().value(QStringLiteral("fields")).toObject(), structure);
    const bool generatedDefaults = m_hexInput->toPlainText().trimmed().isEmpty();
    QByteArray inputBytes;
    if (generatedDefaults) {
        if (structure.size <= 0 || structure.size > 1024 * 1024) {
            clearResult(QStringLiteral("结构体大小无效或超过默认 HEX 生成上限（1 MiB）。"));
            return;
        }
        QHash<QString, QString> defaultValues;
        if (!collectBusinessValues(QString(), defaultValues, error)) {
            clearResult(QStringLiteral("无法读取业务表单默认值：%1").arg(error));
            return;
        }
        const QByteArray zeroBase(int(structure.size), '\0');
        const BinaryResult generated = encodeStructure(structure, defaultValues, zeroBase);
        if (!generated.success) {
            clearResult(QStringLiteral("无法根据默认值生成 HEX：\n%1")
                        .arg(generated.diagnostics.join(QLatin1Char('\n'))));
            return;
        }
        inputBytes = generated.bytes;
        if (m_byteOrder->currentIndex() != 0) {
            const QSignalBlocker blocker(m_byteOrder);
            m_byteOrder->setCurrentIndex(0);
        }
        {
            const QSignalBlocker blocker(m_hexInput);
            m_hexInput->setPlainText(QString::fromLatin1(inputBytes.toHex(' ').toUpper()));
        }
        m_copyHexButton->setEnabled(true);
        m_hexGeneratedByForm = true;
    } else {
        const BinaryResult hex = parseHexBytes(m_hexInput->toPlainText());
        if (!hex.success) {
            clearResult(hex.diagnostics.join(QLatin1Char('\n')));
            return;
        }
        inputBytes = hex.bytes;
    }
    const DecodeResult decoded = decodeStructure(structure, inputBytes,
        m_byteOrder->currentIndex() == 0 ? BinaryByteOrder::LittleEndian : BinaryByteOrder::BigEndian);
    if (!decoded.success) {
        clearResult(decoded.diagnostics.join(QLatin1Char('\n')));
        return;
    }
    m_activeStructure = structure;
    m_activeConfig = config;
    m_currentBytes = inputBytes;
    populateBusinessForm(decoded);
    renderResults(decoded, structure, config.object());
    if (m_byteOrder->currentIndex() == 0) {
        if (generatedDefaults) {
            m_status->setText(QStringLiteral("已根据业务表单默认值生成 %1 字节 HEX，并显示字段映射。只读字段、数组和填充区从 0 开始；修改右侧表单会自动更新 HEX。")
                              .arg(inputBytes.size()));
        } else {
            m_status->setText(QStringLiteral("解析成功：%1 字节。右侧表单可编辑，修改后会自动重新编码，并同步刷新左侧解析结果。")
                              .arg(inputBytes.size()));
        }
    } else {
        m_status->setText(QStringLiteral("解析成功：%1 字节。大端结果可查看；当前仅支持在小端模式下通过业务表单重新编码。")
                          .arg(inputBytes.size()));
    }
}
