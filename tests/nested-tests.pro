include(test-common.pri)

TARGET = nested-tests
CONFIG += console
INCLUDEPATH += $$TEST_ROOT $$TEST_ROOT/core $$TEST_ROOT/widget
SOURCES = $$PWD/nested-tests.cpp $$TEST_ROOT/core/headerparser.cpp $$TEST_ROOT/widget/UI/mainwindow.cpp $$TEST_ROOT/core/fieldvalue.cpp $$TEST_ROOT/core/binaryencoder.cpp $$TEST_ROOT/core/binarydecoder.cpp $$TEST_ROOT/core/definitionjson.cpp $$TEST_ROOT/widget/UI/theme.cpp
HEADERS = $$TEST_ROOT/core/headerparser.h $$TEST_ROOT/widget/UI/mainwindow.h $$TEST_ROOT/core/fieldvalue.h $$TEST_ROOT/core/binaryencoder.h $$TEST_ROOT/core/binarydecoder.h $$TEST_ROOT/core/definitionjson.h $$TEST_ROOT/widget/UI/theme.h

SOURCES += $$TEST_ROOT/widget/UI/framelesswindow.cpp $$TEST_ROOT/widget/UI/hexdialogs.cpp
HEADERS += $$TEST_ROOT/widget/UI/framelesswindow.h $$TEST_ROOT/widget/UI/hexdialogs.h
SOURCES += $$TEST_ROOT/widget/UI/helpdialog.cpp
HEADERS += $$TEST_ROOT/widget/UI/helpdialog.h

SOURCES += $$TEST_ROOT/widget/UI/fieldeditor.cpp
HEADERS += $$TEST_ROOT/widget/UI/fieldeditor.h

SOURCES += $$TEST_ROOT/core/structuresession.cpp
HEADERS += $$TEST_ROOT/core/structuresession.h

SOURCES += $$TEST_ROOT/widget/UI/definitionpanel.cpp
HEADERS += $$TEST_ROOT/widget/UI/definitionpanel.h
SOURCES += $$TEST_ROOT/widget/UI/definitionfilebar.cpp $$TEST_ROOT/widget/UI/structureselectorbar.cpp
HEADERS += $$TEST_ROOT/widget/UI/definitionfilebar.h $$TEST_ROOT/widget/UI/structureselectorbar.h
SOURCES += $$TEST_ROOT/widget/UI/structuredefinitioncreator.cpp
HEADERS += $$TEST_ROOT/widget/UI/structuredefinitioncreator.h
