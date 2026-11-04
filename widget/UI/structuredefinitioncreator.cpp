#include "structuredefinitioncreator.h"

#include "definitionjson.h"
#include "headerparser.h"

#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSet>
#include <QSignalBlocker>
#include <QStyle>
#include <QTableWidget>
#include <QVBoxLayout>
#include <algorithm>

namespace {

struct TypeSpec
{
    const char *name;
    const char *label;
    const char *canonical;
    qint64 size;
    FieldValueKind kind;
    int bits;
};

const TypeSpec typeSpecs[] = {
    {"int8_t", "int8_t（有符号 8 位）", "signed char", 1, FieldValueKind::SignedInteger, 8},
    {"uint8_t", "uint8_t（无符号 8 位）", "unsigned char", 1, FieldValueKind::UnsignedInteger, 8},
    {"int16_t", "int16_t（有符号 16 位）", "short", 2, FieldValueKind::SignedInteger, 16},
    {"uint16_t", "uint16_t（无符号 16 位）", "unsigned short", 2, FieldValueKind::UnsignedInteger, 16},
    {"int32_t", "int32_t（有符号 32 位）", "int", 4, FieldValueKind::SignedInteger, 32},
    {"uint32_t", "uint32_t（无符号 32 位）", "unsigned int", 4, FieldValueKind::UnsignedInteger, 32},
    {"int64_t", "int64_t（有符号 64 位）", "long long", 8, FieldValueKind::SignedInteger, 64},
    {"uint64_t", "uint64_t（无符号 64 位）", "unsigned long long", 8, FieldValueKind::UnsignedInteger, 64},
    {"bool", "bool（布尔）", "bool", 1, FieldValueKind::Boolean, 8},
    {"float", "float（单精度）", "float", 4, FieldValueKind::Float, 0},
    {"double", "double（双精度）", "double", 8, FieldValueKind::Double, 0}
};

const TypeSpec *typeSpec(const QString &name)
{
    for (const TypeSpec &spec : typeSpecs) {
        if (name == QLatin1String(spec.name))
            return &spec;
    }
    return nullptr;
}

bool isIdentifier(const QString &name)
{
    static const QRegularExpression pattern(QStringLiteral("^[A-Za-z_][A-Za-z0-9_]*$"));
    static const QSet<QString> keywords = {
        QStringLiteral("alignas"), QStringLiteral("alignof"), QStringLiteral("and"),
        QStringLiteral("and_eq"), QStringLiteral("asm"), QStringLiteral("auto"),
        QStringLiteral("bitand"), QStringLiteral("bitor"), QStringLiteral("bool"),
        QStringLiteral("break"), QStringLiteral("case"), QStringLiteral("catch"),
        QStringLiteral("char"), QStringLiteral("char16_t"), QStringLiteral("char32_t"),
        QStringLiteral("class"), QStringLiteral("compl"), QStringLiteral("const"),
        QStringLiteral("const_cast"),
        QStringLiteral("constexpr"), QStringLiteral("continue"), QStringLiteral("default"),
        QStringLiteral("decltype"), QStringLiteral("delete"), QStringLiteral("do"),
        QStringLiteral("double"), QStringLiteral("dynamic_cast"),
        QStringLiteral("else"), QStringLiteral("enum"), QStringLiteral("explicit"),
        QStringLiteral("export"), QStringLiteral("extern"), QStringLiteral("false"),
        QStringLiteral("float"), QStringLiteral("for"), QStringLiteral("friend"),
        QStringLiteral("goto"), QStringLiteral("if"), QStringLiteral("inline"),
        QStringLiteral("int"), QStringLiteral("long"), QStringLiteral("mutable"),
        QStringLiteral("namespace"), QStringLiteral("new"), QStringLiteral("noexcept"),
        QStringLiteral("not"), QStringLiteral("not_eq"), QStringLiteral("nullptr"),
        QStringLiteral("operator"), QStringLiteral("or"), QStringLiteral("or_eq"),
        QStringLiteral("private"), QStringLiteral("protected"),
        QStringLiteral("public"), QStringLiteral("register"), QStringLiteral("return"),
        QStringLiteral("reinterpret_cast"), QStringLiteral("short"), QStringLiteral("signed"),
        QStringLiteral("sizeof"), QStringLiteral("static"), QStringLiteral("static_assert"),
        QStringLiteral("static_cast"), QStringLiteral("struct"), QStringLiteral("switch"),
        QStringLiteral("template"), QStringLiteral("this"), QStringLiteral("throw"),
        QStringLiteral("thread_local"), QStringLiteral("true"), QStringLiteral("try"),
        QStringLiteral("typedef"), QStringLiteral("typeid"),
        QStringLiteral("typename"), QStringLiteral("union"), QStringLiteral("unsigned"),
        QStringLiteral("using"), QStringLiteral("virtual"), QStringLiteral("void"),
        QStringLiteral("wchar_t"),
        QStringLiteral("volatile"), QStringLiteral("while"), QStringLiteral("xor")
    };
    return pattern.match(name).hasMatch() && !keywords.contains(name);
}

qint64 alignUp(qint64 value, qint64 alignment)
{
    return ((value + alignment - 1) / alignment) * alignment;
}

QTableWidgetItem *readOnlyItem(const QString &text)
{
    auto *item = new QTableWidgetItem(text);
    item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    return item;
}

} // namespace

StructureDefinitionCreatorDialog::StructureDefinitionCreatorDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("新建结构定义"));
    resize(900, 650);
    setMinimumSize(760, 540);

    auto *root = new QVBoxLayout(this);
    auto *description = new QLabel(
        QStringLiteral("填写结构体名称、字段名称和类型。字段顺序按列表排列，大小与偏移会自动计算。"), this);
    description->setWordWrap(true);
    root->addWidget(description);

    auto *form = new QFormLayout;
    m_structName = new QLineEdit(this);
    m_structName->setObjectName(QStringLiteral("newStructureName"));
    m_structName->setPlaceholderText(QStringLiteral("例如 PacketHeader"));
    form->addRow(QStringLiteral("结构体名称："), m_structName);
    m_packAlignment = new QComboBox(this);
    m_packAlignment->setObjectName(QStringLiteral("newStructurePackAlignment"));
    for (int alignment : {1, 2, 4, 8})
        m_packAlignment->addItem(QStringLiteral("%1 字节").arg(alignment), alignment);
    m_packAlignment->setCurrentIndex(2);
    m_packAlignment->setToolTip(QStringLiteral("限制成员可使用的最大对齐；对应 C++ 的 #pragma pack。"));
    form->addRow(QStringLiteral("最大成员对齐："), m_packAlignment);
    root->addLayout(form);

    auto *fieldActions = new QHBoxLayout;
    auto *addButton = new QPushButton(QStringLiteral("添加字段"), this);
    addButton->setObjectName(QStringLiteral("addDefinitionFieldButton"));
    auto *removeButton = new QPushButton(QStringLiteral("删除字段"), this);
    removeButton->setObjectName(QStringLiteral("removeDefinitionFieldButton"));
    auto *upButton = new QPushButton(QStringLiteral("上移"), this);
    upButton->setObjectName(QStringLiteral("moveDefinitionFieldUpButton"));
    auto *downButton = new QPushButton(QStringLiteral("下移"), this);
    downButton->setObjectName(QStringLiteral("moveDefinitionFieldDownButton"));
    fieldActions->addWidget(addButton);
    fieldActions->addWidget(removeButton);
    fieldActions->addWidget(upButton);
    fieldActions->addWidget(downButton);
    fieldActions->addStretch();
    root->addLayout(fieldActions);

    m_fields = new QTableWidget(0, 4, this);
    m_fields->setObjectName(QStringLiteral("newDefinitionFields"));
    m_fields->setHorizontalHeaderLabels(QStringList() << QStringLiteral("字段名称")
        << QStringLiteral("字段类型") << QStringLiteral("大小（字节）") << QStringLiteral("偏移（字节）"));
    m_fields->horizontalHeaderItem(0)->setToolTip(QStringLiteral("填写英文 C++ 标识符；同一结构体内不能重名。"));
    m_fields->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_fields->setSelectionMode(QAbstractItemView::SingleSelection);
    m_fields->setAlternatingRowColors(true);
    m_fields->verticalHeader()->setVisible(false);
    m_fields->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_fields->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_fields->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_fields->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    root->addWidget(m_fields, 1);

    m_summary = new QLabel(this);
    m_summary->setObjectName(QStringLiteral("newDefinitionSummary"));
    root->addWidget(m_summary);
    m_status = new QLabel(QStringLiteral("添加至少一个字段后即可保存。"), this);
    m_status->setObjectName(QStringLiteral("newDefinitionStatus"));
    m_status->setWordWrap(true);
    root->addWidget(m_status);

    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    auto *cancelButton = new QPushButton(QStringLiteral("取消"), this);
    auto *saveButton = new QPushButton(QStringLiteral("生成并保存 JSON"), this);
    saveButton->setObjectName(QStringLiteral("saveNewDefinitionButton"));
    saveButton->setDefault(true);
    buttons->addWidget(cancelButton);
    buttons->addWidget(saveButton);
    root->addLayout(buttons);

    connect(addButton, &QPushButton::clicked, this, &StructureDefinitionCreatorDialog::addField);
    connect(removeButton, &QPushButton::clicked, this, &StructureDefinitionCreatorDialog::removeField);
    connect(upButton, &QPushButton::clicked, this, [this] { moveField(-1); });
    connect(downButton, &QPushButton::clicked, this, [this] { moveField(1); });
    connect(m_structName, &QLineEdit::textChanged, this, &StructureDefinitionCreatorDialog::updatePreview);
    connect(m_packAlignment, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &StructureDefinitionCreatorDialog::updatePreview);
    connect(m_fields, &QTableWidget::itemChanged, this, [this] { updatePreview(); });
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(saveButton, &QPushButton::clicked, this, &StructureDefinitionCreatorDialog::saveDefinition);
    addField();
}

void StructureDefinitionCreatorDialog::addField()
{
    const int row = m_fields->rowCount();
    QTableWidgetItem *name = nullptr;
    {
        const QSignalBlocker blocker(m_fields);
        m_fields->insertRow(row);
        name = new QTableWidgetItem;
        m_fields->setItem(row, 0, name);
        auto *type = new QComboBox(m_fields);
        for (const TypeSpec &spec : typeSpecs)
            type->addItem(QString::fromUtf8(spec.label), QString::fromLatin1(spec.name));
        m_fields->setCellWidget(row, 1, type);
        m_fields->setItem(row, 2, readOnlyItem(QStringLiteral("—")));
        m_fields->setItem(row, 3, readOnlyItem(QStringLiteral("—")));
        connect(type, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &StructureDefinitionCreatorDialog::updatePreview);
    }
    m_fields->setCurrentCell(row, 0);
    updatePreview();
    m_fields->editItem(name);
}

void StructureDefinitionCreatorDialog::removeField()
{
    const int row = m_fields->currentRow();
    if (row >= 0)
        m_fields->removeRow(row);
    updatePreview();
}

void StructureDefinitionCreatorDialog::moveField(int direction)
{
    const int row = m_fields->currentRow();
    const int target = row + direction;
    if (row < 0 || target < 0 || target >= m_fields->rowCount())
        return;
    const QString name = m_fields->item(row, 0)->text();
    auto *type = qobject_cast<QComboBox *>(m_fields->cellWidget(row, 1));
    const QString typeName = type ? type->currentData().toString() : QString();
    const QString otherName = m_fields->item(target, 0)->text();
    auto *otherType = qobject_cast<QComboBox *>(m_fields->cellWidget(target, 1));
    const QString otherTypeName = otherType ? otherType->currentData().toString() : QString();
    {
        const QSignalBlocker blocker(m_fields);
        m_fields->item(row, 0)->setText(otherName);
        m_fields->item(target, 0)->setText(name);
        if (type) type->setCurrentIndex(type->findData(otherTypeName));
        if (otherType) otherType->setCurrentIndex(otherType->findData(typeName));
    }
    m_fields->setCurrentCell(target, 0);
    updatePreview();
}

bool StructureDefinitionCreatorDialog::buildStructure(StructInfo &structure, QString &error) const
{
    const QString name = m_structName->text().trimmed();
    if (!isIdentifier(name)) {
        error = QStringLiteral("结构体名称必须是有效的 C++ 标识符，且不能使用关键字。示例：PacketHeader。支持英文、数字和下划线，首字符不能是数字。");
        return false;
    }
    if (m_fields->rowCount() == 0) {
        error = QStringLiteral("请至少添加一个字段。");
        return false;
    }

    structure = StructInfo();
    structure.name = name;
    const qint64 pack = m_packAlignment->currentData().toLongLong();
    structure.packAlignment = int(pack);
    qint64 cursor = 0;
    QSet<QString> fieldNames;
    for (int row = 0; row < m_fields->rowCount(); ++row) {
        const QTableWidgetItem *nameItem = m_fields->item(row, 0);
        const QString fieldName = nameItem ? nameItem->text().trimmed() : QString();
        if (!isIdentifier(fieldName)) {
            error = QStringLiteral("第 %1 行的字段名称无效；请填写有效的 C++ 标识符。").arg(row + 1);
            return false;
        }
        if (fieldNames.contains(fieldName)) {
            error = QStringLiteral("字段名称“%1”重复。").arg(fieldName);
            return false;
        }
        fieldNames.insert(fieldName);

        auto *type = qobject_cast<QComboBox *>(m_fields->cellWidget(row, 1));
        const TypeSpec *spec = type ? typeSpec(type->currentData().toString()) : nullptr;
        if (!spec) {
            error = QStringLiteral("第 %1 行的字段类型无效。").arg(row + 1);
            return false;
        }
        const qint64 effectiveAlignment = qMin(spec->size, pack);
        const qint64 offset = alignUp(cursor, effectiveAlignment);
        FieldInfo field;
        field.name = fieldName;
        field.typeName = QString::fromLatin1(spec->name);
        field.canonicalTypeName = QString::fromLatin1(spec->canonical);
        field.size = spec->size;
        field.alignment = spec->size;
        field.offsetBits = offset * 8;
        field.absoluteOffsetBits = field.offsetBits;
        field.valueKind = spec->kind;
        field.valueBits = spec->bits;
        structure.fields.append(field);
        structure.alignment = qMax(structure.alignment, effectiveAlignment);
        cursor = offset + spec->size;
    }
    structure.size = alignUp(cursor, structure.alignment);
    return true;
}

void StructureDefinitionCreatorDialog::updatePreview()
{
    StructInfo structure;
    QString error;
    if (!buildStructure(structure, error)) {
        const QSignalBlocker blocker(m_fields);
        for (int row = 0; row < m_fields->rowCount(); ++row) {
            if (QTableWidgetItem *size = m_fields->item(row, 2))
                size->setText(QStringLiteral("—"));
            if (QTableWidgetItem *offset = m_fields->item(row, 3))
                offset->setText(QStringLiteral("—"));
        }
        m_summary->setText(QStringLiteral("布局预览：等待有效的结构体和字段定义"));
        m_status->setText(error);
        m_status->setProperty("invalid", true);
        m_status->style()->unpolish(m_status);
        m_status->style()->polish(m_status);
        return;
    }
    const QSignalBlocker blocker(m_fields);
    for (int row = 0; row < structure.fields.size(); ++row) {
        const FieldInfo &field = structure.fields.at(row);
        if (QTableWidgetItem *size = m_fields->item(row, 2))
            size->setText(QString::number(field.size));
        if (QTableWidgetItem *offset = m_fields->item(row, 3))
            offset->setText(QString::number(field.offsetBits / 8));
    }
    m_summary->setText(QStringLiteral("布局预览：%1 字节    |    结构体对齐：%2 字节")
                       .arg(structure.size).arg(structure.alignment));
    m_status->setText(QStringLiteral("布局有效。字段按最大成员对齐规则自动计算偏移和填充。"));
    m_status->setProperty("invalid", false);
    m_status->style()->unpolish(m_status);
    m_status->style()->polish(m_status);
}

void StructureDefinitionCreatorDialog::saveDefinition()
{
    StructInfo structure;
    QString error;
    if (!buildStructure(structure, error)) {
        m_status->setText(error);
        return;
    }
    QString suggested = structure.name;
    for (const QChar invalid : QStringLiteral("\\/:*?\"<>|"))
        suggested.replace(invalid, QLatin1Char('_'));
    suggested += QStringLiteral(".json");
    QString path = QFileDialog::getSaveFileName(this, QStringLiteral("保存结构定义 JSON"), suggested,
                                                QStringLiteral("结构定义 JSON (*.json);;所有文件 (*)"));
    if (path.isEmpty())
        return;
    if (QFileInfo(path).suffix().isEmpty())
        path += QStringLiteral(".json");
    if (!saveStructureDefinitionJson(structure, path, &error)) {
        m_status->setText(QStringLiteral("保存失败：%1").arg(error));
        return;
    }
    emit definitionSaved(QDir::toNativeSeparators(path));
    accept();
}
