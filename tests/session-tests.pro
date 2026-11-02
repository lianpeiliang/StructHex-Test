STRUCTHEX_ROOT = $$clean_path($$PWD/..)
include(../StructHex.pro)
TARGET = session-tests
QT = core
CONFIG += console
RESOURCES =
RC_ICONS =
SOURCES = $$PWD/session-tests.cpp $$PWD/../core/structuresession.cpp $$PWD/../core/headerparser.cpp $$PWD/../core/definitionjson.cpp $$PWD/../core/fieldvalue.cpp
HEADERS = $$PWD/../core/structuresession.h $$PWD/../core/headerparser.h $$PWD/../core/definitionjson.h $$PWD/../core/fieldvalue.h
INCLUDEPATH += $$PWD/../core
