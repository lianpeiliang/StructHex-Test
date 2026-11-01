#include "workflowlauncher.h"

#include "businessconfigeditor.h"
#include "businesshexdecoder.h"
#include "businessformrunner.h"
#include "definitionjson.h"
#include "mainwindow.h"

#include <QFileDialog>
#include <QFont>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <functional>

WorkflowLauncher::WorkflowLauncher(const QString &defaultStructurePath, QWidget *parent)
    : QDialog(parent), m_defaultStructurePath(defaultStructurePath)
{
    setWindowTitle(QStringLiteral("StructHex · 工作台"));
    setObjectName(QStringLiteral("workflowLauncher"));
    resize(760, 620);
    setMinimumSize(640, 540);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(36, 30, 36, 30);
    root->setSpacing(18);
    auto *title = new QLabel(QStringLiteral("StructHex 工作台"), this);
    QFont titleFont = font();
    titleFont.setPointSize(20);
    titleFont.setBold(true);
    title->setFont(titleFont);
    root->addWidget(title);
    auto *intro = new QLabel(QStringLiteral("选择你要进行的工作。每个入口对应一段独立流程。"), this);
    intro->setWordWrap(true);
    root->addWidget(intro);

    auto addWorkflow = [this, root](const QString &name, const QString &description,
                                    const QString &objectName, const std::function<void()> &action) {
        auto *button = new QPushButton(name, this);
        button->setObjectName(objectName);
        button->setMinimumHeight(54);
        button->setToolTip(description);
        root->addWidget(button);
        auto *detail = new QLabel(description, this);
        detail->setWordWrap(true);
        root->addWidget(detail);
        connect(button, &QPushButton::clicked, this, action);
    };

    addWorkflow(QStringLiteral("1. 结构定义"),
        QStringLiteral("打开 C++ 头文件查看结构体布局，或加载已有结构定义 JSON；也可以导出结构定义 JSON。"),
        QStringLiteral("structureWorkspaceButton"), [this] { openStructureWorkspace(); });
    addWorkflow(QStringLiteral("2. 业务表单设计"),
        QStringLiteral("选择结构定义 JSON，为字段配置显示名称、UI 控件、范围、换算比例和选项，保存 .business.json。"),
        QStringLiteral("businessDesignerButton"), [this] { openBusinessDesigner(); });
    addWorkflow(QStringLiteral("3. 业务表单运行"),
        QStringLiteral("分别选择 .business.json 和结构定义 JSON，打开动态表单并生成结构体十六进制。"),
        QStringLiteral("businessRunnerButton"), [this] { openBusinessRunner(); });
    addWorkflow(QStringLiteral("4. 业务语义解析 Plus"),
        QStringLiteral("选择业务配置 JSON 和结构定义 JSON，解析十六进制并查看字段含义；右侧业务表单可编辑，改动会自动生成新数据并刷新字段映射。"),
        QStringLiteral("businessHexDecoderButton"), [this] { openBusinessHexDecoder(); });
    root->addStretch(1);
    auto *close = new QPushButton(QStringLiteral("退出"), this);
    close->setObjectName(QStringLiteral("exitButton"));
    root->addWidget(close, 0, Qt::AlignRight);
    connect(close, &QPushButton::clicked, this, &QDialog::reject);
}

void WorkflowLauncher::openStructureWorkspace()
{
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("打开结构定义"),
        m_defaultStructurePath,
        QStringLiteral("头文件或结构定义 JSON (*.h *.hpp *.hh *.hxx *.json);;所有文件 (*)"));
    if (path.isEmpty())
        return;
    auto *window = new MainWindow(path);
    window->setAttribute(Qt::WA_DeleteOnClose);
    window->show();
}

void WorkflowLauncher::openBusinessDesigner()
{
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("选择结构定义 JSON"),
        m_defaultStructurePath, QStringLiteral("结构定义 JSON (*.json);;所有文件 (*)"));
    if (path.isEmpty())
        return;
    const ParseResult result = loadStructureDefinitionJson(path);
    if (!result.success) {
        QMessageBox::warning(this, QStringLiteral("无法打开结构定义 JSON"),
            result.diagnostics.join(QLatin1Char('\n')));
        return;
    }
    BusinessConfigEditorDialog dialog(result.structure, path, this);
    dialog.exec();
}

void WorkflowLauncher::openBusinessRunner()
{
    BusinessFormRunnerDialog dialog(this);
    dialog.exec();
}

void WorkflowLauncher::openBusinessHexDecoder()
{
    BusinessHexDecoderDialog dialog(this);
    dialog.exec();
}
