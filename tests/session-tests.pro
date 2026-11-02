include(test-common.pri)
TARGET = session-tests
QT = core
CONFIG += console
RESOURCES =
RC_ICONS =
SOURCES = $$PWD/session-tests.cpp $$TEST_ROOT/core/structuresession.cpp $$TEST_ROOT/core/headerparser.cpp $$TEST_ROOT/core/definitionjson.cpp $$TEST_ROOT/core/fieldvalue.cpp
HEADERS = $$TEST_ROOT/core/structuresession.h $$TEST_ROOT/core/headerparser.h $$TEST_ROOT/core/definitionjson.h $$TEST_ROOT/core/fieldvalue.h
