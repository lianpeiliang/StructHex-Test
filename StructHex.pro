# qmake 项目文件：声明 Qt 模块、编译选项、源码文件以及外部库依赖。
QT += core gui widgets

TEMPLATE = app
TARGET = StructHex
CONFIG += c++11
CONFIG -= console app_bundle

# Windows 可执行文件图标，以及窗口/任务栏使用的内嵌图标资源。
win32:RC_ICONS = $$PWD/assets/structhex.ico
RESOURCES += $$PWD/resources.qrc

# SOURCES 中的 .cpp 会参与编译；HEADERS 列出头文件，便于 Qt Creator 展示和项目管理。
INCLUDEPATH += $$PWD
SOURCES += main.cpp
HEADERS += headerparser.h fieldvalue.h binaryencoder.h binarydecoder.h
HEADERS += definitionjson.h businessconfigutils.h businesscodec.h structuresession.h

# 界面代码统一放在 UI/，业务和数据处理模块由 StructHexCore 动态库提供。
SOURCES += UI/mainwindow.cpp UI/framelesswindow.cpp UI/fieldeditor.cpp UI/hexdialogs.cpp UI/theme.cpp
HEADERS += UI/mainwindow.h UI/framelesswindow.h UI/fieldeditor.h UI/hexdialogs.h UI/theme.h
SOURCES += UI/helpdialog.cpp
HEADERS += UI/helpdialog.h
HEADERS += TestHeader/MsgHeader.h TestHeader/SimpleMsgHeader.h TestHeader/MinimalMsgHeader.h \
           TestHeader/PacketHeader.h TestHeader/NestedPacketHeader.h TestHeader/MultiStructHeader.h

# 将工具链目录及默认待解析头文件路径作为字符串宏传入程序。
DEFINES += MSG_HEADER_PATH=\\\"$$PWD/TestHeader/MsgHeader.h\\\"

# MSYS2 MINGW64 directory used by the core library's libclang parser.
isEmpty(LIBCLANG_ROOT): LIBCLANG_ROOT = D:/msys64/mingw64

SOURCES += UI/definitionpanel.cpp
HEADERS += UI/definitionpanel.h
SOURCES += UI/definitionfilebar.cpp UI/structureselectorbar.cpp
HEADERS += UI/definitionfilebar.h UI/structureselectorbar.h
SOURCES += UI/businessconfigeditor.cpp
HEADERS += UI/businessconfigeditor.h
SOURCES += UI/businessformpreview.cpp
HEADERS += UI/businessformpreview.h
SOURCES += UI/businessformrunner.cpp UI/businesshexdecoder.cpp UI/workflowlauncher.cpp
HEADERS += UI/businessformrunner.h UI/businesshexdecoder.h UI/workflowlauncher.h

# Build the business/core DLL before linking the QWidget application.
CORE_BUILD_DIR = $$OUT_PWD/core
mkpath($$CORE_BUILD_DIR)
CONFIG(debug, debug|release): CORE_CONFIG = CONFIG+=debug
else: CORE_CONFIG = CONFIG+=release
core_build.target = core_build
core_build.commands = $$QMAKE_QMAKE $$shell_quote($$PWD/StructHexCore.pro) -o $$shell_quote($$CORE_BUILD_DIR/Makefile) "LIBCLANG_ROOT=$$LIBCLANG_ROOT" $$CORE_CONFIG && $(MAKE) -C $$shell_quote($$CORE_BUILD_DIR)
QMAKE_EXTRA_TARGETS += core_build
PRE_TARGETDEPS += core_build
LIBS += -L$$shell_quote($$CORE_BUILD_DIR) -lStructHexCore
mkpath($$OUT_PWD/debug)
mkpath($$OUT_PWD/release)
CORE_DLL_NATIVE = $$shell_quote($$system_path($$CORE_BUILD_DIR/StructHexCore.dll))
APP_DLL_NATIVE = $$shell_quote($$system_path($$OUT_PWD/StructHexCore.dll))
DEBUG_DLL_NATIVE = $$shell_quote($$system_path($$OUT_PWD/debug/StructHexCore.dll))
RELEASE_DLL_NATIVE = $$shell_quote($$system_path($$OUT_PWD/release/StructHexCore.dll))
QMAKE_POST_LINK += cmd /c copy /y $$CORE_DLL_NATIVE $$APP_DLL_NATIVE && cmd /c copy /y $$CORE_DLL_NATIVE $$DEBUG_DLL_NATIVE && cmd /c copy /y $$CORE_DLL_NATIVE $$RELEASE_DLL_NATIVE
