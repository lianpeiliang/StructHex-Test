#include "businessformrunner.h"

#include "businessformpreview.h"
#include "businessconfigutils.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

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
    if (!loadBusinessConfig(businessPath, businessDocument, error)) {
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
