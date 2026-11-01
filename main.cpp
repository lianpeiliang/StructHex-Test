#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include "UI/mainwindow.h"
#include "UI/workflowlauncher.h"
#include "UI/theme.h"

// 程序入口。阅读顺序：创建应用 → 确定头文件路径 → 创建窗口 → 进入事件循环。
int main(int argc, char *argv[])
{
    // GUI 程序使用 QApplication；事件循环负责窗口显示和按钮响应。
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("StructHex"));
    app.setApplicationDisplayName(QStringLiteral("StructHex · 工作台"));
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/structhex.png")));
    applyApplicationTheme(app);
    // 便携包优先使用程序旁边的示例，不依赖构建时的本机源码路径。
    QString defaultPath = QDir(app.applicationDirPath()).filePath(QStringLiteral("TestHeader/MsgHeader.h"));
    if (!QFileInfo::exists(defaultPath))
        defaultPath = QStringLiteral(MSG_HEADER_PATH);
    // 传入文件路径时沿用直接打开结构定义的方式；普通启动显示三阶段工作台。
    if (app.arguments().size() > 1) {
        MainWindow window(app.arguments().at(1));
        window.show();
        return app.exec();
    }
    WorkflowLauncher launcher(defaultPath);
    launcher.show();
    // exec() 持续处理点击、键盘等事件；关闭最后一个窗口后，事件循环通常结束。
    return app.exec();
}
