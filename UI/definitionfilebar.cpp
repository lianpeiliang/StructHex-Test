#include "definitionfilebar.h"
#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

DefinitionFileBar::DefinitionFileBar(const QString &filePath, QWidget *parent) : QWidget(parent)
{
    auto *row = new QHBoxLayout(this);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(10);
    row->addWidget(new QLabel(QStringLiteral("结构定义文件："), this));
    m_filePath = new QLineEdit(QDir::toNativeSeparators(filePath), this);
    m_filePath->setObjectName(QStringLiteral("headerFilePath"));
    m_filePath->setPlaceholderText(QStringLiteral("输入头文件或结构定义 JSON 路径，按回车加载"));
    row->addWidget(m_filePath, 1);
    auto *openButton = new QPushButton(QStringLiteral("打开结构定义"), this);
    openButton->setObjectName(QStringLiteral("openDefinitionButton"));
    auto *reloadButton = new QPushButton(QStringLiteral("重新加载"), this);
    reloadButton->setObjectName(QStringLiteral("reloadDefinitionButton"));
    row->addWidget(openButton);
    row->addWidget(reloadButton);
    m_exportButton = new QPushButton(QStringLiteral("导出结构定义 JSON"), this);
    m_exportButton->setObjectName(QStringLiteral("exportDefinitionButton"));
    m_exportButton->setToolTip(QStringLiteral("导出当前解析的类型和布局，不包含输入值；无需填写字段值。"));
    m_exportButton->setEnabled(false);
    row->addWidget(m_exportButton);
    m_businessConfigButton = new QPushButton(QStringLiteral("设计业务配置 JSON"), this);
    m_businessConfigButton->setObjectName(QStringLiteral("businessConfigButton"));
    m_businessConfigButton->setToolTip(QStringLiteral("根据当前结构体字段设计业务表单定义，不填写业务数值。"));
    m_businessConfigButton->setEnabled(false);
    row->addWidget(m_businessConfigButton);
    connect(m_exportButton, &QPushButton::clicked, this, &DefinitionFileBar::exportRequested);
    connect(m_businessConfigButton, &QPushButton::clicked, this, &DefinitionFileBar::businessConfigRequested);
    connect(reloadButton, &QPushButton::clicked, this, [this] { emit loadRequested(m_filePath->text()); });
    connect(m_filePath, &QLineEdit::returnPressed, this, [this] { emit loadRequested(m_filePath->text()); });
    connect(openButton, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(
            this, QStringLiteral("打开结构定义"), m_filePath->text(),
            QStringLiteral("结构定义 (*.h *.hpp *.hh *.hxx *.json);;C++ 头文件 (*.h *.hpp *.hh *.hxx);;结构定义 JSON (*.json);;所有文件 (*)"));
        if (!path.isEmpty()) {
            m_filePath->setText(QDir::toNativeSeparators(path));
            emit loadRequested(path);
        }
    });
}

void DefinitionFileBar::setExportEnabled(bool enabled)
{
    m_exportButton->setEnabled(enabled);
    m_businessConfigButton->setEnabled(enabled);
}
