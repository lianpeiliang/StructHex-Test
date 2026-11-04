QT += core gui widgets

TEMPLATE = app
TARGET = StructHex
CONFIG += c++11
CONFIG -= console app_bundle debug_and_release

WIDGET_ROOT = $$clean_path($$PWD)
PROJECT_ROOT = $$clean_path($$WIDGET_ROOT/..)

win32:RC_ICONS = $$WIDGET_ROOT/assets/structhex.ico
RESOURCES += $$WIDGET_ROOT/resources.qrc

INCLUDEPATH += $$PROJECT_ROOT $$PROJECT_ROOT/core $$WIDGET_ROOT
DEFINES += MSG_HEADER_PATH=\\\"$$PROJECT_ROOT/TestHeader/MsgHeader.h\\\"

SOURCES += $$WIDGET_ROOT/main.cpp
SOURCES += $$WIDGET_ROOT/UI/mainwindow.cpp $$WIDGET_ROOT/UI/framelesswindow.cpp
SOURCES += $$WIDGET_ROOT/UI/fieldeditor.cpp $$WIDGET_ROOT/UI/hexdialogs.cpp $$WIDGET_ROOT/UI/theme.cpp
SOURCES += $$WIDGET_ROOT/UI/helpdialog.cpp $$WIDGET_ROOT/UI/definitionpanel.cpp
SOURCES += $$WIDGET_ROOT/UI/definitionfilebar.cpp $$WIDGET_ROOT/UI/structureselectorbar.cpp
SOURCES += $$WIDGET_ROOT/UI/structuredefinitioncreator.cpp
SOURCES += $$WIDGET_ROOT/UI/businessconfigeditor.cpp $$WIDGET_ROOT/UI/businessformpreview.cpp
SOURCES += $$WIDGET_ROOT/UI/businessformrunner.cpp $$WIDGET_ROOT/UI/businesshexdecoder.cpp
SOURCES += $$WIDGET_ROOT/UI/workflowlauncher.cpp

HEADERS += $$WIDGET_ROOT/UI/mainwindow.h $$WIDGET_ROOT/UI/framelesswindow.h
HEADERS += $$WIDGET_ROOT/UI/fieldeditor.h $$WIDGET_ROOT/UI/hexdialogs.h $$WIDGET_ROOT/UI/theme.h
HEADERS += $$WIDGET_ROOT/UI/helpdialog.h $$WIDGET_ROOT/UI/definitionpanel.h
HEADERS += $$WIDGET_ROOT/UI/definitionfilebar.h $$WIDGET_ROOT/UI/structureselectorbar.h
HEADERS += $$WIDGET_ROOT/UI/structuredefinitioncreator.h
HEADERS += $$WIDGET_ROOT/UI/businessconfigeditor.h $$WIDGET_ROOT/UI/businessformpreview.h
HEADERS += $$WIDGET_ROOT/UI/businessformrunner.h $$WIDGET_ROOT/UI/businesshexdecoder.h
HEADERS += $$WIDGET_ROOT/UI/workflowlauncher.h
HEADERS += $$PROJECT_ROOT/TestHeader/MsgHeader.h $$PROJECT_ROOT/TestHeader/SimpleMsgHeader.h
HEADERS += $$PROJECT_ROOT/TestHeader/MinimalMsgHeader.h $$PROJECT_ROOT/TestHeader/PacketHeader.h
HEADERS += $$PROJECT_ROOT/TestHeader/NestedPacketHeader.h $$PROJECT_ROOT/TestHeader/MultiStructHeader.h

CORE_BUILD_DIR = $$clean_path($$OUT_PWD/../core)
LIBS += -L$$shell_quote($$CORE_BUILD_DIR) -lStructHexCore

# Keep the runtime DLL beside the executable produced by this subproject.
CORE_DLL_NATIVE = $$shell_quote($$system_path($$CORE_BUILD_DIR/StructHexCore.dll))
APP_DLL_NATIVE = $$shell_quote($$system_path($$OUT_PWD/StructHexCore.dll))
QMAKE_POST_LINK += cmd /c copy /y $$CORE_DLL_NATIVE $$APP_DLL_NATIVE
