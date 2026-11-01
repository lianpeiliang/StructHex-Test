#include "businesshexdecoder.h"

#include "binarydecoder.h"
#include "businessconfigutils.h"
#include "definitionjson.h"

#include <QComboBox>
#include <QAbstractItemView>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <cmath>

namespace {

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
    setWindowTitle(QStringLiteral("业务语义解析 Plus"));
    setObjectName(QStringLiteral("businessHexDecoderDialog"));
    resize(1180, 780);
    setMinimumSize(900, 620);

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
    layout->addLayout(form);
    connect(businessBrowse, &QPushButton::clicked, this, &BusinessHexDecoderDialog::chooseBusinessConfig);
    connect(structureBrowse, &QPushButton::clicked, this, &BusinessHexDecoderDialog::chooseStructureDefinition);
    connect(m_businessPath, &QLineEdit::textChanged, this, [this] { m_legacyConfigAccepted = false; clearResult(QStringLiteral("文件路径已更改，请重新解析。")); });
    connect(m_structurePath, &QLineEdit::textChanged, this, [this] { m_legacyConfigAccepted = false; clearResult(QStringLiteral("文件路径已更改，请重新解析。")); });

    auto *hexRow = new QWidget(this);
    auto *hexLayout = new QHBoxLayout(hexRow);
    hexLayout->setContentsMargins(0, 0, 0, 0);
    auto *hexLabel = new QLabel(QStringLiteral("十六进制数据："), hexRow);
    QPushButton *hexBrowse = new QPushButton(QStringLiteral("从文件读取…"), hexRow);
    hexLayout->addWidget(hexLabel);
    hexLayout->addStretch();
    hexLayout->addWidget(hexBrowse);
    layout->addWidget(hexRow);
    m_hexInput = new QPlainTextEdit(this);
    m_hexInput->setObjectName(QStringLiteral("businessHexInput"));
    m_hexInput->setPlaceholderText(QStringLiteral("例如：01 01 D2 04 05 00 00 00\n支持空格、换行、逗号和 0x 前缀；请粘贴字节本身，不含偏移地址。"));
    layout->addWidget(m_hexInput, 1);
    connect(hexBrowse, &QPushButton::clicked, this, &BusinessHexDecoderDialog::chooseHexFile);
    connect(m_hexInput, &QPlainTextEdit::textChanged, this, [this] { clearResult(QStringLiteral("输入已更新，请重新解析。")); });

    auto *actionRow = new QHBoxLayout;
    actionRow->addWidget(new QLabel(QStringLiteral("数据字节序："), this));
    m_byteOrder = new QComboBox(this);
    m_byteOrder->addItems({QStringLiteral("小端"), QStringLiteral("大端")});
    m_byteOrder->setToolTip(QStringLiteral("字段偏移和对齐由结构定义 JSON 决定；此选项只决定字段数值的字节解释。大端位域暂不支持。"));
    actionRow->addWidget(m_byteOrder);
    connect(m_byteOrder, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this] { clearResult(QStringLiteral("字节序已更改，请重新解析。")); });
    actionRow->addStretch();
    auto *parse = new QPushButton(QStringLiteral("解析并显示业务含义"), this);
    parse->setObjectName(QStringLiteral("decodeBusinessHexButton"));
    actionRow->addWidget(parse);
    layout->addLayout(actionRow);

    m_status = new QLabel(QStringLiteral("尚未解析。"), this);
    m_status->setWordWrap(true);
    m_status->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_status);
    m_results = new QTreeWidget(this);
    m_results->setObjectName(QStringLiteral("businessDecodedFields"));
    m_results->setHeaderLabels({QStringLiteral("结构字段"), QStringLiteral("业务名称"), QStringLiteral("C++ 类型"),
        QStringLiteral("字节偏移"), QStringLiteral("原始字节"), QStringLiteral("结构值"),
        QStringLiteral("业务显示值"), QStringLiteral("说明")});
    m_results->setAlternatingRowColors(true);
    m_results->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_results->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_results->header()->setStretchLastSection(true);
    layout->addWidget(m_results, 2);

    auto *close = new QPushButton(QStringLiteral("关闭"), this);
    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    buttons->addWidget(close);
    layout->addLayout(buttons);
    connect(parse, &QPushButton::clicked, this, &BusinessHexDecoderDialog::decode);
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
}

void BusinessHexDecoderDialog::chooseBusinessConfig()
{
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("选择业务配置 JSON"),
        m_businessPath->text(), QStringLiteral("业务配置 JSON (*.json);;所有文件 (*)"));
    if (!path.isEmpty()) m_businessPath->setText(QDir::toNativeSeparators(path));
}

void BusinessHexDecoderDialog::chooseStructureDefinition()
{
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("选择结构定义 JSON"),
        m_structurePath->text(), QStringLiteral("结构定义 JSON (*.json);;所有文件 (*)"));
    if (!path.isEmpty()) m_structurePath->setText(QDir::toNativeSeparators(path));
}

void BusinessHexDecoderDialog::chooseHexFile()
{
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("打开十六进制文本"), QString(),
        QStringLiteral("十六进制文本 (*.hex *.txt);;所有文件 (*)"));
    if (path.isEmpty()) return;
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

void BusinessHexDecoderDialog::clearResult(const QString &message)
{
    m_results->clear();
    m_status->setText(message);
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
    const BinaryResult hex = parseHexBytes(m_hexInput->toPlainText());
    if (!hex.success) {
        clearResult(hex.diagnostics.join(QLatin1Char('\n')));
        return;
    }
    const DecodeResult decoded = decodeStructure(structure, hex.bytes,
        m_byteOrder->currentIndex() == 0 ? BinaryByteOrder::LittleEndian : BinaryByteOrder::BigEndian);
    if (!decoded.success) {
        clearResult(decoded.diagnostics.join(QLatin1Char('\n')));
        return;
    }
    const QJsonObject fields = config.object().value(QStringLiteral("fields")).toObject();
    auto *root = new QTreeWidgetItem(m_results, {structure.name, config.object().value(QStringLiteral("title")).toString(),
        QStringLiteral("struct"), QStringLiteral("0"), QString(), QStringLiteral("%1 字节").arg(structure.size), QString(), QString()});
    appendResults(root, decoded.fields, fields);
    m_results->expandAll();
    m_status->setText(QStringLiteral("解析成功：%1 字节。结构值是原始数值；业务显示值已按业务配置的选项、换算比例和单位转换。")
                       .arg(hex.bytes.size()));
}
