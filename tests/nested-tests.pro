STRUCTHEX_ROOT = $$clean_path($$PWD/..)
include(../StructHex.pro)

TARGET = nested-tests
CONFIG += console
INCLUDEPATH += $$PWD/.. $$PWD/../core
SOURCES = $$PWD/nested-tests.cpp $$PWD/../core/headerparser.cpp $$PWD/../UI/mainwindow.cpp $$PWD/../core/fieldvalue.cpp $$PWD/../core/binaryencoder.cpp $$PWD/../core/binarydecoder.cpp $$PWD/../core/definitionjson.cpp $$PWD/../UI/theme.cpp
HEADERS = $$PWD/../core/headerparser.h $$PWD/../UI/mainwindow.h $$PWD/../core/fieldvalue.h $$PWD/../core/binaryencoder.h $$PWD/../core/binarydecoder.h $$PWD/../core/definitionjson.h $$PWD/../UI/theme.h

SOURCES += $$PWD/../UI/framelesswindow.cpp $$PWD/../UI/hexdialogs.cpp
HEADERS += $$PWD/../UI/framelesswindow.h $$PWD/../UI/hexdialogs.h
SOURCES += $$PWD/../UI/helpdialog.cpp
HEADERS += $$PWD/../UI/helpdialog.h

SOURCES += $$PWD/../UI/fieldeditor.cpp
HEADERS += $$PWD/../UI/fieldeditor.h

SOURCES += $$PWD/../core/structuresession.cpp
HEADERS += $$PWD/../core/structuresession.h

SOURCES += $$PWD/../UI/definitionpanel.cpp
HEADERS += $$PWD/../UI/definitionpanel.h
SOURCES += $$PWD/../UI/definitionfilebar.cpp $$PWD/../UI/structureselectorbar.cpp
HEADERS += $$PWD/../UI/definitionfilebar.h $$PWD/../UI/structureselectorbar.h
