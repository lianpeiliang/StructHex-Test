# qmake 项目文件：声明 Qt 模块、编译选项、源码文件以及外部库依赖。
QT += core gui widgets

isEmpty(STRUCTHEX_ROOT): STRUCTHEX_ROOT = $$PWD

TEMPLATE = app
TARGET = StructHex
CONFIG += c++11
CONFIG -= console app_bundle

# Windows 可执行文件图标，以及窗口/任务栏使用的内嵌图标资源。
win32:RC_ICONS = $$STRUCTHEX_ROOT/assets/structhex.ico
RESOURCES += $$STRUCTHEX_ROOT/resources.qrc

# SOURCES 中的 .cpp 会参与编译；HEADERS 列出头文件，便于 Qt Creator 展示和项目管理。
INCLUDEPATH += $$STRUCTHEX_ROOT $$STRUCTHEX_ROOT/core
SOURCES += $$STRUCTHEX_ROOT/main.cpp
HEADERS += $$STRUCTHEX_ROOT/core/headerparser.h $$STRUCTHEX_ROOT/core/fieldvalue.h
HEADERS += $$STRUCTHEX_ROOT/core/binaryencoder.h $$STRUCTHEX_ROOT/core/binarydecoder.h
HEADERS += $$STRUCTHEX_ROOT/core/definitionjson.h $$STRUCTHEX_ROOT/core/businessconfigutils.h
HEADERS += $$STRUCTHEX_ROOT/core/businesscodec.h $$STRUCTHEX_ROOT/core/structuresession.h

# 界面代码统一放在 UI/，业务和数据处理模块由 StructHexCore 动态库提供。
SOURCES += $$STRUCTHEX_ROOT/UI/mainwindow.cpp $$STRUCTHEX_ROOT/UI/framelesswindow.cpp
SOURCES += $$STRUCTHEX_ROOT/UI/fieldeditor.cpp $$STRUCTHEX_ROOT/UI/hexdialogs.cpp $$STRUCTHEX_ROOT/UI/theme.cpp
HEADERS += $$STRUCTHEX_ROOT/UI/mainwindow.h $$STRUCTHEX_ROOT/UI/framelesswindow.h
HEADERS += $$STRUCTHEX_ROOT/UI/fieldeditor.h $$STRUCTHEX_ROOT/UI/hexdialogs.h $$STRUCTHEX_ROOT/UI/theme.h
SOURCES += $$STRUCTHEX_ROOT/UI/helpdialog.cpp
HEADERS += $$STRUCTHEX_ROOT/UI/helpdialog.h
HEADERS += $$STRUCTHEX_ROOT/TestHeader/MsgHeader.h $$STRUCTHEX_ROOT/TestHeader/SimpleMsgHeader.h
HEADERS += $$STRUCTHEX_ROOT/TestHeader/MinimalMsgHeader.h $$STRUCTHEX_ROOT/TestHeader/PacketHeader.h
HEADERS += $$STRUCTHEX_ROOT/TestHeader/NestedPacketHeader.h $$STRUCTHEX_ROOT/TestHeader/MultiStructHeader.h

# 将工具链目录及默认待解析头文件路径作为字符串宏传入程序。
DEFINES += MSG_HEADER_PATH=\\\"$$STRUCTHEX_ROOT/TestHeader/MsgHeader.h\\\"

# MSYS2 MINGW64 directory used by the core library's libclang parser.
isEmpty(LIBCLANG_ROOT): LIBCLANG_ROOT = D:/msys64/mingw64

SOURCES += $$STRUCTHEX_ROOT/UI/definitionpanel.cpp
HEADERS += $$STRUCTHEX_ROOT/UI/definitionpanel.h
SOURCES += $$STRUCTHEX_ROOT/UI/definitionfilebar.cpp $$STRUCTHEX_ROOT/UI/structureselectorbar.cpp
HEADERS += $$STRUCTHEX_ROOT/UI/definitionfilebar.h $$STRUCTHEX_ROOT/UI/structureselectorbar.h
SOURCES += $$STRUCTHEX_ROOT/UI/businessconfigeditor.cpp
HEADERS += $$STRUCTHEX_ROOT/UI/businessconfigeditor.h
SOURCES += $$STRUCTHEX_ROOT/UI/businessformpreview.cpp
HEADERS += $$STRUCTHEX_ROOT/UI/businessformpreview.h
SOURCES += $$STRUCTHEX_ROOT/UI/businessformrunner.cpp $$STRUCTHEX_ROOT/UI/businesshexdecoder.cpp
SOURCES += $$STRUCTHEX_ROOT/UI/workflowlauncher.cpp
HEADERS += $$STRUCTHEX_ROOT/UI/businessformrunner.h $$STRUCTHEX_ROOT/UI/businesshexdecoder.h
HEADERS += $$STRUCTHEX_ROOT/UI/workflowlauncher.h

# Build the business/core DLL before linking the QWidget application.
CORE_BUILD_DIR = $$OUT_PWD/core
mkpath($$CORE_BUILD_DIR)
CONFIG(debug, debug|release): CORE_CONFIG = CONFIG+=debug
else: CORE_CONFIG = CONFIG+=release
core_build.target = core_build
core_build.commands = $$QMAKE_QMAKE $$shell_quote($$STRUCTHEX_ROOT/core/StructHexCore.pro) -o $$shell_quote($$CORE_BUILD_DIR/Makefile) "LIBCLANG_ROOT=$$LIBCLANG_ROOT" $$CORE_CONFIG && $(MAKE) -C $$shell_quote($$CORE_BUILD_DIR)
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
