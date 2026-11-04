#include "structuredefinitioncreator.h"

#include "definitionjson.h"
#include "headerparser.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QSet>
#include <QStyle>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <algorithm>
#include <functional>

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
    {"double", "double（双精度）", "double", 8, FieldValueKind::Double, 0},
    {"long double", "long double（扩展精度）", "long double", 16, FieldValueKind::LongDouble, 0}
};

const int RecordRole = Qt::UserRole;

QString normalizedRecordName(QString name)
{
    name = name.trimmed();
    if (name.startsWith(QLatin1String("struct ")))
        name.remove(0, 7);
    return name.trimmed();
}

const TypeSpec *typeSpec(const QString &name)
{
    const QString normalized = name.startsWith(QLatin1String("std::")) ? name.mid(5) : name;
    for (const TypeSpec &spec : typeSpecs) {
        if (normalized == QLatin1String(spec.name))
            return &spec;
    }
    return nullptr;
}

const TypeSpec *typeSpec(const FieldInfo &field)
{
    const TypeSpec *named = typeSpec(field.typeName);
    if (named && named->kind == field.valueKind && named->size == field.size)
        return named;
    for (const TypeSpec &spec : typeSpecs) {
        if (spec.kind == field.valueKind && spec.size == field.size)
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
        QStringLiteral("const_cast"), QStringLiteral("constexpr"), QStringLiteral("continue"),
        QStringLiteral("default"), QStringLiteral("decltype"), QStringLiteral("delete"),
        QStringLiteral("do"), QStringLiteral("double"), QStringLiteral("dynamic_cast"),
        QStringLiteral("else"), QStringLiteral("enum"), QStringLiteral("explicit"),
        QStringLiteral("export"), QStringLiteral("extern"), QStringLiteral("false"),
        QStringLiteral("float"), QStringLiteral("for"), QStringLiteral("friend"),
        QStringLiteral("goto"), QStringLiteral("if"), QStringLiteral("inline"),
        QStringLiteral("int"), QStringLiteral("long"), QStringLiteral("mutable"),
        QStringLiteral("namespace"), QStringLiteral("new"), QStringLiteral("noexcept"),
        QStringLiteral("not"), QStringLiteral("not_eq"), QStringLiteral("nullptr"),
        QStringLiteral("operator"), QStringLiteral("or"), QStringLiteral("or_eq"),
        QStringLiteral("private"), QStringLiteral("protected"), QStringLiteral("public"),
        QStringLiteral("register"), QStringLiteral("reinterpret_cast"), QStringLiteral("return"),
        QStringLiteral("short"), QStringLiteral("signed"), QStringLiteral("sizeof"),
        QStringLiteral("static"), QStringLiteral("static_assert"), QStringLiteral("static_cast"),
        QStringLiteral("struct"), QStringLiteral("switch"), QStringLiteral("template"),
        QStringLiteral("this"), QStringLiteral("throw"), QStringLiteral("thread_local"),
        QStringLiteral("true"), QStringLiteral("try"), QStringLiteral("typedef"),
        QStringLiteral("typeid"), QStringLiteral("typename"), QStringLiteral("union"),
        QStringLiteral("unsigned"), QStringLiteral("using"), QStringLiteral("virtual"),
        QStringLiteral("void"), QStringLiteral("volatile"), QStringLiteral("wchar_t"),
        QStringLiteral("while"), QStringLiteral("xor")
    };
    return pattern.match(name).hasMatch() && !keywords.contains(name);
}

qint64 alignUp(qint64 value, qint64 alignment)
{
    return ((value + alignment - 1) / alignment) * alignment;
}

void shiftAbsoluteOffsets(QVector<FieldInfo> &fields, qint64 shiftBits)
{
    for (FieldInfo &field : fields) {
        field.absoluteOffsetBits += shiftBits;
        shiftAbsoluteOffsets(field.children, shiftBits);
    }
}

struct LayoutResult
{
    QVector<FieldInfo> fields;
    qint64 size = 1;
    qint64 alignment = 1;
};

bool supportsFieldEditing(const FieldInfo &field, QString &reason)
{
    if (!isIdentifier(field.name) || field.isArray || field.isBitField
        || !field.binaryEncodingIssue.isEmpty()) {
        reason = QStringLiteral("字段“%1”包含当前编辑器尚不支持的名称、数组、位域或布局。")
                     .arg(field.name);
        return false;
    }
    if (field.valueKind == FieldValueKind::Record) {
        if (!isIdentifier(normalizedRecordName(field.typeName)) || field.size <= 0
            || field.alignment <= 0) {
            reason = QStringLiteral("嵌套结构字段“%1”的类型名或布局无效。").arg(field.name);
            return false;
        }
        for (const FieldInfo &child : field.children) {
            if (!supportsFieldEditing(child, reason))
                return false;
        }
        return true;
    }
    const TypeSpec *spec = typeSpec(field);
    if (!spec || field.valueKind != spec->kind || field.size != spec->size
        || field.alignment != spec->size || !field.children.isEmpty()) {
        reason = QStringLiteral("字段“%1”包含当前编辑器尚不支持的类型或布局。")
                     .arg(field.name);
        return false;
    }
    return true;
}

} // namespace

StructureDefinitionCreatorDialog::StructureDefinitionCreatorDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("新建结构定义"));
    resize(960, 680);
    setMinimumSize(780, 560);

    auto *root = new QVBoxLayout(this);
    auto *description = new QLabel(
        QStringLiteral("按树形层级填写结构体字段。结构体成员可展开添加子字段；大小、偏移和填充会自动计算。"), this);
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
    m_packAlignment->setToolTip(QStringLiteral("限制成员可使用的最大对齐；对应 C++ 的 #pragma pack。首版此设置应用于树中的所有层级。"));
    form->addRow(QStringLiteral("最大成员对齐："), m_packAlignment);
    root->addLayout(form);

    auto *fieldActions = new QHBoxLayout;
    auto *addButton = new QPushButton(QStringLiteral("添加基础字段"), this);
    addButton->setObjectName(QStringLiteral("addDefinitionFieldButton"));
    auto *addRecordButton = new QPushButton(QStringLiteral("添加结构体字段"), this);
    addRecordButton->setObjectName(QStringLiteral("addNestedDefinitionFieldButton"));
    auto *removeButton = new QPushButton(QStringLiteral("删除字段"), this);
    removeButton->setObjectName(QStringLiteral("removeDefinitionFieldButton"));
    auto *upButton = new QPushButton(QStringLiteral("上移"), this);
    upButton->setObjectName(QStringLiteral("moveDefinitionFieldUpButton"));
    auto *downButton = new QPushButton(QStringLiteral("下移"), this);
    downButton->setObjectName(QStringLiteral("moveDefinitionFieldDownButton"));
    fieldActions->addWidget(addButton);
    fieldActions->addWidget(addRecordButton);
    fieldActions->addWidget(removeButton);
    fieldActions->addWidget(upButton);
    fieldActions->addWidget(downButton);
    fieldActions->addStretch();
    root->addLayout(fieldActions);

    m_fields = new QTreeWidget(this);
    m_fields->setObjectName(QStringLiteral("newDefinitionFields"));
    m_fields->setColumnCount(4);
    m_fields->setHeaderLabels(QStringList() << QStringLiteral("字段名称")
        << QStringLiteral("字段类型 / 结构体名") << QStringLiteral("大小（字节）")
        << QStringLiteral("偏移（字节）"));
    m_fields->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_fields->setSelectionMode(QAbstractItemView::SingleSelection);
    m_fields->setAlternatingRowColors(true);
    m_fields->setUniformRowHeights(true);
    m_fields->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_fields->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_fields->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_fields->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    root->addWidget(m_fields, 1);

    m_summary = new QLabel(this);
    m_summary->setObjectName(QStringLiteral("newDefinitionSummary"));
    root->addWidget(m_summary);
    m_status = new QLabel(QStringLiteral("添加字段后即可保存。"), this);
    m_status->setObjectName(QStringLiteral("newDefinitionStatus"));
    m_status->setWordWrap(true);
    root->addWidget(m_status);

    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    auto *cancelButton = new QPushButton(QStringLiteral("取消"), this);
    m_saveButton = new QPushButton(QStringLiteral("生成并保存 JSON"), this);
    m_saveButton->setObjectName(QStringLiteral("saveNewDefinitionButton"));
    m_saveButton->setDefault(true);
    m_saveAsButton = new QPushButton(QStringLiteral("另存为…"), this);
    m_saveAsButton->setObjectName(QStringLiteral("saveDefinitionAsButton"));
    m_saveAsButton->hide();
    buttons->addWidget(cancelButton);
    buttons->addWidget(m_saveAsButton);
    buttons->addWidget(m_saveButton);
    root->addLayout(buttons);

    connect(addButton, &QPushButton::clicked, this, &StructureDefinitionCreatorDialog::addField);
    connect(addRecordButton, &QPushButton::clicked, this, &StructureDefinitionCreatorDialog::addNestedField);
    connect(removeButton, &QPushButton::clicked, this, &StructureDefinitionCreatorDialog::removeField);
    connect(upButton, &QPushButton::clicked, this, [this] { moveField(-1); });
    connect(downButton, &QPushButton::clicked, this, [this] { moveField(1); });
    connect(m_structName, &QLineEdit::textChanged, this, &StructureDefinitionCreatorDialog::updatePreview);
    connect(m_packAlignment, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &StructureDefinitionCreatorDialog::updatePreview);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_saveButton, &QPushButton::clicked, this, &StructureDefinitionCreatorDialog::saveDefinition);
    connect(m_saveAsButton, &QPushButton::clicked, this, &StructureDefinitionCreatorDialog::saveDefinitionAs);
    addField();
}

StructureDefinitionCreatorDialog::StructureDefinitionCreatorDialog(
    const StructInfo &structure, const QString &filePath, QWidget *parent)
    : StructureDefinitionCreatorDialog(parent)
{
    setWindowTitle(QStringLiteral("修改结构定义 · %1").arg(structure.name));
    m_currentPath = QFileInfo(filePath).absoluteFilePath();
    m_originalFingerprint = structureDefinitionFingerprint(structure);
    m_saveButton->setText(QStringLiteral("保存"));
    m_saveAsButton->show();
    {
        const QSignalBlocker nameBlocker(m_structName);
        m_structName->setText(structure.name);
    }
    {
        const QSignalBlocker treeBlocker(m_fields);
        m_fields->clear();
        for (const FieldInfo &field : structure.fields)
            appendExistingField(nullptr, field);
    }
    if (structure.packAlignment == 0) {
        m_packAlignment->insertItem(0, QStringLiteral("请选择（原 JSON 未记录）"), 0);
        m_packAlignment->setCurrentIndex(0);
    } else {
        m_packAlignment->setCurrentIndex(m_packAlignment->findData(structure.packAlignment));
    }
    updatePreview();
}

bool StructureDefinitionCreatorDialog::supportsEditing(const StructInfo &structure, QString &reason)
{
    if (!isIdentifier(structure.name) || structure.fields.isEmpty()
        || !structure.binaryEncodingIssue.isEmpty()) {
        reason = QStringLiteral("当前结构定义包含此编辑器尚不支持的结构体名称或布局。");
        return false;
    }
    for (const FieldInfo &field : structure.fields) {
        if (!supportsFieldEditing(field, reason))
            return false;
    }
    reason.clear();
    return true;
}

QTreeWidgetItem *StructureDefinitionCreatorDialog::currentContainer() const
{
    QTreeWidgetItem *current = m_fields->currentItem();
    if (!current)
        return nullptr;
    return current->data(0, RecordRole).toBool() ? current : current->parent();
}

QTreeWidgetItem *StructureDefinitionCreatorDialog::appendScalarField(
    QTreeWidgetItem *parent, const QString &fieldName, const QString &typeName)
{
    auto *item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(m_fields);
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    item->setData(0, RecordRole, false);

    auto *name = new QLineEdit(fieldName, m_fields);
    name->setPlaceholderText(QStringLiteral("字段名"));
    name->setToolTip(QStringLiteral("填写英文 C++ 标识符；同一结构体层级内不能重名。"));
    m_fields->setItemWidget(item, 0, name);
    connect(name, &QLineEdit::textChanged, this, &StructureDefinitionCreatorDialog::updatePreview);

    auto *type = new QComboBox(m_fields);
    for (const TypeSpec &spec : typeSpecs)
        type->addItem(QString::fromUtf8(spec.label), QString::fromLatin1(spec.name));
    const TypeSpec *selected = typeSpec(typeName);
    type->setCurrentIndex(type->findData(selected ? QString::fromLatin1(selected->name)
                                                   : QStringLiteral("int8_t")));
    m_fields->setItemWidget(item, 1, type);
    connect(type, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &StructureDefinitionCreatorDialog::updatePreview);
    item->setText(2, QStringLiteral("—"));
    item->setText(3, QStringLiteral("—"));
    return item;
}

QTreeWidgetItem *StructureDefinitionCreatorDialog::appendRecordField(
    QTreeWidgetItem *parent, const QString &fieldName, const QString &typeName)
{
    auto *item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(m_fields);
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    item->setData(0, RecordRole, true);
    item->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);

    auto *name = new QLineEdit(fieldName, m_fields);
    name->setPlaceholderText(QStringLiteral("成员名"));
    name->setToolTip(QStringLiteral("这是外层结构体中该结构体成员的字段名。"));
    m_fields->setItemWidget(item, 0, name);
    connect(name, &QLineEdit::textChanged, this, &StructureDefinitionCreatorDialog::updatePreview);

    auto *type = new QLineEdit(normalizedRecordName(typeName), m_fields);
    type->setPlaceholderText(QStringLiteral("结构体类型名，例如 A"));
    type->setToolTip(QStringLiteral("填写该嵌套结构体的 C++ 类型名。"));
    m_fields->setItemWidget(item, 1, type);
    connect(type, &QLineEdit::textChanged, this, &StructureDefinitionCreatorDialog::updatePreview);
    item->setText(2, QStringLiteral("—"));
    item->setText(3, QStringLiteral("—"));
    return item;
}

void StructureDefinitionCreatorDialog::appendExistingField(QTreeWidgetItem *parent, const FieldInfo &field)
{
    QTreeWidgetItem *item = nullptr;
    if (field.valueKind == FieldValueKind::Record) {
        item = appendRecordField(parent, field.name, field.typeName);
        for (const FieldInfo &child : field.children)
            appendExistingField(item, child);
        item->setExpanded(true);
    } else {
        const TypeSpec *spec = typeSpec(field);
        item = appendScalarField(parent, field.name,
                                 spec ? QString::fromLatin1(spec->name) : field.typeName);
    }
    item->setText(2, QString::number(field.size));
    item->setText(3, QString::number(field.offsetBits / 8));
}

void StructureDefinitionCreatorDialog::addField()
{
    QTreeWidgetItem *item = appendScalarField(currentContainer(), QString(), QStringLiteral("int8_t"));
    m_fields->setCurrentItem(item);
    m_fields->scrollToItem(item);
    if (QLineEdit *name = qobject_cast<QLineEdit *>(m_fields->itemWidget(item, 0)))
        name->setFocus();
    updatePreview();
}

void StructureDefinitionCreatorDialog::addNestedField()
{
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("添加结构体字段"));
    auto *form = new QFormLayout(&dialog);
    auto *fieldName = new QLineEdit(QStringLiteral("nestedValue"), &dialog);
    fieldName->setObjectName(QStringLiteral("nestedFieldName"));
    auto *typeName = new QLineEdit(QStringLiteral("NestedStruct"), &dialog);
    typeName->setObjectName(QStringLiteral("nestedStructTypeName"));
    form->addRow(QStringLiteral("成员字段名："), fieldName);
    form->addRow(QStringLiteral("结构体类型名："), typeName);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const QString member = fieldName->text().trimmed();
    const QString type = typeName->text().trimmed();
    if (!isIdentifier(member) || !isIdentifier(type)) {
        QMessageBox::information(this, QStringLiteral("名称无效"),
            QStringLiteral("成员名和结构体类型名都必须是有效的英文 C++ 标识符。"));
        return;
    }
    QTreeWidgetItem *item = appendRecordField(currentContainer(), member, type);
    m_fields->setCurrentItem(item);
    item->setExpanded(true);
    m_fields->scrollToItem(item);
    updatePreview();
}

void StructureDefinitionCreatorDialog::removeField()
{
    QTreeWidgetItem *item = m_fields->currentItem();
    if (!item)
        return;
    if (item->parent()) {
        const int index = item->parent()->indexOfChild(item);
        delete item->parent()->takeChild(index);
    } else {
        delete m_fields->takeTopLevelItem(m_fields->indexOfTopLevelItem(item));
    }
    updatePreview();
}

void StructureDefinitionCreatorDialog::moveField(int direction)
{
    QTreeWidgetItem *item = m_fields->currentItem();
    if (!item)
        return;
    QTreeWidgetItem *parent = item->parent();
    const int row = parent ? parent->indexOfChild(item) : m_fields->indexOfTopLevelItem(item);
    const int target = row + direction;
    const int count = parent ? parent->childCount() : m_fields->topLevelItemCount();
    if (target < 0 || target >= count)
        return;
    if (parent) {
        item = parent->takeChild(row);
        parent->insertChild(target, item);
    } else {
        item = m_fields->takeTopLevelItem(row);
        m_fields->insertTopLevelItem(target, item);
    }
    m_fields->setCurrentItem(item);
    updatePreview();
}

bool StructureDefinitionCreatorDialog::buildStructure(StructInfo &structure, QString &error) const
{
    const QString name = m_structName->text().trimmed();
    if (!isIdentifier(name)) {
        error = QStringLiteral("结构体名称必须是有效的 C++ 标识符，且不能使用关键字。示例：PacketHeader。支持英文、数字和下划线，首字符不能是数字。");
        return false;
    }
    if (m_fields->topLevelItemCount() == 0) {
        error = QStringLiteral("请至少添加一个字段。");
        return false;
    }

    const qint64 pack = m_packAlignment->currentData().toLongLong();
    if (pack != 1 && pack != 2 && pack != 4 && pack != 8) {
        error = QStringLiteral("请选择最大成员对齐（pack）。");
        return false;
    }

    std::function<bool(const QTreeWidgetItem *, LayoutResult &, QString &)> buildLevel;
    buildLevel = [&](const QTreeWidgetItem *parent, LayoutResult &layout, QString &layoutError) {
        const int count = parent ? parent->childCount() : m_fields->topLevelItemCount();
        qint64 cursor = 0;
        QSet<QString> fieldNames;
        layout = LayoutResult();
        layout.fields.clear();

        for (int index = 0; index < count; ++index) {
            const QTreeWidgetItem *item = parent ? parent->child(index) : m_fields->topLevelItem(index);
            const auto *nameEdit = qobject_cast<QLineEdit *>(m_fields->itemWidget(
                const_cast<QTreeWidgetItem *>(item), 0));
            const QString fieldName = nameEdit ? nameEdit->text().trimmed() : QString();
            if (!isIdentifier(fieldName)) {
                layoutError = QStringLiteral("字段名无效：请填写有效的英文 C++ 标识符。");
                return false;
            }
            if (fieldNames.contains(fieldName)) {
                layoutError = QStringLiteral("同一结构体层级内的字段名“%1”重复。").arg(fieldName);
                return false;
            }
            fieldNames.insert(fieldName);

            FieldInfo field;
            field.name = fieldName;
            qint64 fieldSize = 0;
            qint64 fieldAlignment = 0;
            if (item->data(0, RecordRole).toBool()) {
                const auto *typeEdit = qobject_cast<QLineEdit *>(m_fields->itemWidget(
                    const_cast<QTreeWidgetItem *>(item), 1));
                const QString recordName = typeEdit ? typeEdit->text().trimmed() : QString();
                if (!isIdentifier(recordName)) {
                    layoutError = QStringLiteral("结构体字段“%1”的类型名无效。").arg(fieldName);
                    return false;
                }
                LayoutResult nested;
                if (!buildLevel(item, nested, layoutError))
                    return false;
                field.typeName = recordName;
                field.canonicalTypeName = recordName;
                field.valueKind = FieldValueKind::Record;
                field.readOnlyReason = QStringLiteral("结构体作为分组，请编辑展开后的基础类型成员。");
                field.children = nested.fields;
                fieldSize = nested.size;
                fieldAlignment = nested.alignment;
            } else {
                const auto *type = qobject_cast<QComboBox *>(m_fields->itemWidget(
                    const_cast<QTreeWidgetItem *>(item), 1));
                const TypeSpec *spec = type ? typeSpec(type->currentData().toString()) : nullptr;
                if (!spec) {
                    layoutError = QStringLiteral("字段“%1”的类型无效。").arg(fieldName);
                    return false;
                }
                field.typeName = QString::fromLatin1(spec->name);
                field.canonicalTypeName = QString::fromLatin1(spec->canonical);
                field.valueKind = spec->kind;
                field.valueBits = spec->bits;
                fieldSize = spec->size;
                fieldAlignment = spec->size;
            }

            const qint64 effectiveAlignment = qMin(fieldAlignment, pack);
            const qint64 offset = alignUp(cursor, effectiveAlignment);
            field.size = fieldSize;
            field.alignment = fieldAlignment;
            field.offsetBits = offset * 8;
            field.absoluteOffsetBits = offset * 8;
            if (!field.children.isEmpty())
                shiftAbsoluteOffsets(field.children, offset * 8);
            layout.fields.append(field);
            layout.alignment = qMax(layout.alignment, effectiveAlignment);
            cursor = offset + fieldSize;
        }
        layout.size = count == 0 ? 1 : alignUp(cursor, layout.alignment);
        return true;
    };

    LayoutResult layout;
    if (!buildLevel(nullptr, layout, error))
        return false;
    structure = StructInfo();
    structure.name = name;
    structure.size = layout.size;
    structure.alignment = layout.alignment;
    structure.packAlignment = int(pack);
    structure.fields = layout.fields;
    return true;
}

void StructureDefinitionCreatorDialog::updatePreview()
{
    StructInfo structure;
    QString error;
    if (!buildStructure(structure, error)) {
        std::function<void(QTreeWidgetItem *)> clearRows = [&](QTreeWidgetItem *parent) {
            const int count = parent ? parent->childCount() : m_fields->topLevelItemCount();
            for (int i = 0; i < count; ++i) {
                QTreeWidgetItem *item = parent ? parent->child(i) : m_fields->topLevelItem(i);
                item->setText(2, QStringLiteral("—"));
                item->setText(3, QStringLiteral("—"));
                clearRows(item);
            }
        };
        clearRows(nullptr);
        m_summary->setText(QStringLiteral("布局预览：等待有效的结构体和字段定义"));
        m_status->setText(error);
        m_status->setProperty("invalid", true);
        m_status->style()->unpolish(m_status);
        m_status->style()->polish(m_status);
        return;
    }

    std::function<void(QTreeWidgetItem *, const QVector<FieldInfo> &)> showRows;
    showRows = [&](QTreeWidgetItem *parent, const QVector<FieldInfo> &fields) {
        for (int i = 0; i < fields.size(); ++i) {
            QTreeWidgetItem *item = parent ? parent->child(i) : m_fields->topLevelItem(i);
            const FieldInfo &field = fields.at(i);
            item->setText(2, QString::number(field.size));
            item->setText(3, QString::number(field.offsetBits / 8));
            showRows(item, field.children);
        }
    };
    showRows(nullptr, structure.fields);
    m_summary->setText(QStringLiteral("布局预览：%1 字节    |    结构体对齐：%2 字节")
                       .arg(structure.size).arg(structure.alignment));
    m_status->setText(m_originalFingerprint.isEmpty()
        || structureDefinitionFingerprint(structure) == m_originalFingerprint
        ? QStringLiteral("布局有效。所选 pack 上限应用于树中的所有结构体层级。")
        : QStringLiteral("布局有效，但保存后会改变原结构布局；配套业务配置可能需要更新。"));
    m_status->setProperty("invalid", false);
    m_status->style()->unpolish(m_status);
    m_status->style()->polish(m_status);
}

void StructureDefinitionCreatorDialog::saveDefinition()
{
    if (m_currentPath.isEmpty()) {
        saveDefinitionAs();
        return;
    }
    saveToPath(m_currentPath);
}

void StructureDefinitionCreatorDialog::saveDefinitionAs()
{
    StructInfo structure;
    QString error;
    if (!buildStructure(structure, error)) {
        m_status->setText(error);
        return;
    }
    QString suggested = m_currentPath;
    if (suggested.isEmpty()) {
        suggested = structure.name;
        for (const QChar invalid : QStringLiteral("\\/:*?\"<>|"))
            suggested.replace(invalid, QLatin1Char('_'));
        suggested += QStringLiteral(".json");
    }
    QString path = QFileDialog::getSaveFileName(this, QStringLiteral("保存结构定义 JSON"), suggested,
                                                QStringLiteral("结构定义 JSON (*.json);;所有文件 (*)"));
    if (path.isEmpty())
        return;
    if (QFileInfo(path).suffix().isEmpty())
        path += QStringLiteral(".json");
    saveToPath(path);
}

void StructureDefinitionCreatorDialog::saveToPath(const QString &path)
{
    StructInfo structure;
    QString error;
    if (!buildStructure(structure, error)) {
        m_status->setText(error);
        return;
    }
    if (!saveStructureDefinitionJson(structure, path, &error)) {
        m_status->setText(QStringLiteral("保存失败：%1").arg(error));
        return;
    }
    emit definitionSaved(QDir::toNativeSeparators(path));
    accept();
}
