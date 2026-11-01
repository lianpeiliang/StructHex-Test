#include "businessformrunner.h"

#include "businessformpreview.h"
#include "definitionjson.h"

#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSet>
#include <QVBoxLayout>

namespace {

void appendFieldPaths(const QVector<FieldInfo> &fields, const QString &parentPath, QSet<QString> &paths)
{
    for (const FieldInfo &field : fields) {
        const QString path = parentPath.isEmpty() ? field.name : parentPath + QLatin1Char('.') + field.name;
        paths.insert(path);
        if (!field.children.isEmpty())
            appendFieldPaths(field.children, path, paths);
    }
}

bool readBusinessConfig(const QString &path, QJsonDocument &document, QString &error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        error = QStringLiteral("无法读取业务配置：%1").arg(file.errorString());
        return false;
    }
    const int maximumBytes = 16 * 1024 * 1024;
    const QByteArray bytes = file.read(maximumBytes + 1);
    if (file.error() != QFileDevice::NoError) {
        error = QStringLiteral("读取业务配置失败：%1").arg(file.errorString());
        return false;
    }
    if (bytes.size() > maximumBytes) {
        error = QStringLiteral("业务配置 JSON 超过 16 MiB。");
        return false;
    }
    QJsonParseError parseError;
    document = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        error = QStringLiteral("业务配置 JSON 格式错误：%1").arg(parseError.errorString());
        return false;
    }
    return true;
}

bool validateBusinessConfig(const QJsonDocument &document, const StructInfo &structure, QString &error)
{
    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("struct")).toString() != structure.name) {
        error = QStringLiteral("业务配置对应“%1”，结构定义文件对应“%2”。")
            .arg(root.value(QStringLiteral("struct")).toString(), structure.name);
        return false;
    }
    if (!root.value(QStringLiteral("title")).isString()
        || root.value(QStringLiteral("title")).toString().trimmed().isEmpty()
        || root.value(QStringLiteral("layout")).toString() != QLatin1String("form")
        || !root.value(QStringLiteral("fields")).isObject()) {
        error = QStringLiteral("业务配置必须包含非空 title、layout=form 和 fields 对象。");
        return false;
    }
    if (root.contains(QStringLiteral("structureFingerprint"))
        && (!root.value(QStringLiteral("structureFingerprint")).isString()
            || root.value(QStringLiteral("structureFingerprint")).toString()
                != structureDefinitionFingerprint(structure))) {
        error = QStringLiteral("业务配置与所选结构定义的字段布局不一致。");
        return false;
    }

    QSet<QString> expectedPaths;
    appendFieldPaths(structure.fields, QString(), expectedPaths);
    const QJsonObject fields = root.value(QStringLiteral("fields")).toObject();
    if (fields.size() != expectedPaths.size()) {
        error = QStringLiteral("业务配置的字段数量与结构定义不一致。");
        return false;
    }
    const QStringList widgetTypes = QStringList() << QStringLiteral("spinbox")
        << QStringLiteral("doublespinbox") << QStringLiteral("combobox")
        << QStringLiteral("checkbox") << QStringLiteral("lineedit")
        << QStringLiteral("group") << QStringLiteral("readonly");
    for (auto it = fields.constBegin(); it != fields.constEnd(); ++it) {
        if (!expectedPaths.contains(it.key()) || !it.value().isObject()) {
            error = QStringLiteral("业务配置包含无效字段：%1").arg(it.key());
            return false;
        }
        const QJsonObject field = it.value().toObject();
        const QString widget = field.value(QStringLiteral("widget")).toString();
        if (!field.value(QStringLiteral("label")).isString() || !widgetTypes.contains(widget)) {
            error = QStringLiteral("字段 %1 缺少有效的 label 或 widget。").arg(it.key());
            return false;
        }
        if (widget == QLatin1String("combobox") && !field.value(QStringLiteral("options")).isArray()) {
            error = QStringLiteral("字段 %1 是组合框，但没有 options 数组。").arg(it.key());
            return false;
        }
    }
    for (const QString &path : expectedPaths) {
        if (!fields.contains(path)) {
            error = QStringLiteral("业务配置缺少结构字段：%1").arg(path);
            return false;
        }
    }
    return true;
}

} // namespace

BusinessFormRunnerDialog::BusinessFormRunnerDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(QStringLiteral("业务表单运行"));
    resize(820, 300);
    setMinimumSize(680, 260);
    auto *root = new QVBoxLayout(this);
    auto *intro = new QLabel(
        QStringLiteral("分别选择业务配置 JSON 和结构定义 JSON。程序会校验两者匹配后打开业务表单。"), this);
    intro->setWordWrap(true);
    root->addWidget(intro);

    auto *form = new QFormLayout;
    auto *businessRow = new QWidget(this);
    auto *businessLayout = new QHBoxLayout(businessRow);
    businessLayout->setContentsMargins(0, 0, 0, 0);
    m_businessPath = new QLineEdit(businessRow);
    m_businessPath->setPlaceholderText(QStringLiteral("选择 .business.json 文件"));
    auto *businessBrowse = new QPushButton(QStringLiteral("浏览…"), businessRow);
    businessLayout->addWidget(m_businessPath, 1);
    businessLayout->addWidget(businessBrowse);
    form->addRow(QStringLiteral("业务配置："), businessRow);

    auto *structureRow = new QWidget(this);
    auto *structureLayout = new QHBoxLayout(structureRow);
    structureLayout->setContentsMargins(0, 0, 0, 0);
    m_structurePath = new QLineEdit(structureRow);
    m_structurePath->setPlaceholderText(QStringLiteral("选择结构定义 JSON"));
    auto *structureBrowse = new QPushButton(QStringLiteral("浏览…"), structureRow);
    structureLayout->addWidget(m_structurePath, 1);
    structureLayout->addWidget(structureBrowse);
    form->addRow(QStringLiteral("结构定义："), structureRow);
    root->addLayout(form);

    m_status = new QLabel(QStringLiteral("不会自动搜索结构定义文件。"), this);
    m_status->setWordWrap(true);
    m_status->setTextInteractionFlags(Qt::TextSelectableByMouse);
    root->addWidget(m_status);

    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    auto *run = new QPushButton(QStringLiteral("打开业务表单"), this);
    auto *close = new QPushButton(QStringLiteral("关闭"), this);
    buttons->addWidget(run);
    buttons->addWidget(close);
    root->addLayout(buttons);

    connect(businessBrowse, &QPushButton::clicked, this, &BusinessFormRunnerDialog::chooseBusinessConfig);
    connect(structureBrowse, &QPushButton::clicked, this, &BusinessFormRunnerDialog::chooseStructureDefinition);
    connect(run, &QPushButton::clicked, this, &BusinessFormRunnerDialog::runForm);
    connect(close, &QPushButton::clicked, this, &QDialog::reject);
}

void BusinessFormRunnerDialog::chooseBusinessConfig()
{
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("选择业务配置 JSON"),
        m_businessPath->text(), QStringLiteral("业务配置 JSON (*.json);;所有文件 (*)"));
    if (!path.isEmpty())
        m_businessPath->setText(QDir::toNativeSeparators(path));
}

void BusinessFormRunnerDialog::chooseStructureDefinition()
{
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("选择结构定义 JSON"),
        m_structurePath->text(), QStringLiteral("结构定义 JSON (*.json);;所有文件 (*)"));
    if (!path.isEmpty())
        m_structurePath->setText(QDir::toNativeSeparators(path));
}

void BusinessFormRunnerDialog::runForm()
{
    const QString businessPath = m_businessPath->text().trimmed();
    const QString structurePath = m_structurePath->text().trimmed();
    if (businessPath.isEmpty() || structurePath.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("文件未选择"),
            QStringLiteral("请分别选择业务配置 JSON 和结构定义 JSON。"));
        return;
    }
    QJsonDocument businessDocument;
    QString error;
    if (!readBusinessConfig(businessPath, businessDocument, error)) {
        QMessageBox::warning(this, QStringLiteral("业务配置无效"), error);
        return;
    }
    const ParseResult structureResult = loadStructureDefinitionJson(structurePath);
    if (!structureResult.success) {
        QMessageBox::warning(this, QStringLiteral("结构定义无效"), structureResult.diagnostics.join(QLatin1Char('\n')));
        return;
    }
    if (!validateBusinessConfig(businessDocument, structureResult.structure, error)) {
        QMessageBox::warning(this, QStringLiteral("文件不匹配"), error);
        return;
    }
    const QJsonObject root = businessDocument.object();
    if (!root.contains(QStringLiteral("structureFingerprint"))) {
        const auto answer = QMessageBox::question(this, QStringLiteral("旧版业务配置"),
            QStringLiteral("此业务配置没有结构布局指纹，只能校验结构体名称和字段路径。仍要继续吗？"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes)
            return;
    }
    m_status->setText(QStringLiteral("已校验：%1 + %2")
        .arg(QFileInfo(businessPath).fileName(), QFileInfo(structurePath).fileName()));
    BusinessFormPreviewDialog form(businessDocument, structureResult.structure,
                                   QFileInfo(structurePath).absoluteFilePath(), this);
    form.exec();
}
