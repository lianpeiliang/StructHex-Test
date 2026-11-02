QT += core

TEMPLATE = lib
TARGET = StructHexCore
CONFIG += dll c++11
CONFIG -= app_bundle debug_and_release

# Keep the public business and data-processing API independent of Qt Widgets.
INCLUDEPATH += $$PWD

SOURCES += headerparser.cpp fieldvalue.cpp binaryencoder.cpp binarydecoder.cpp
SOURCES += definitionjson.cpp structuresession.cpp
SOURCES += business/businessconfigutils.cpp business/businesscodec.cpp business/businessformservice.cpp
HEADERS += headerparser.h fieldvalue.h binaryencoder.h binarydecoder.h
HEADERS += definitionjson.h structuresession.h
HEADERS += business/businessconfigutils.h business/businesscodec.h business/businessformservice.h

isEmpty(LIBCLANG_ROOT): LIBCLANG_ROOT = D:/msys64/mingw64
!exists($$LIBCLANG_ROOT/include/clang-c/Index.h): error("Cannot find clang-c/Index.h; set LIBCLANG_ROOT")
QMAKE_CXXFLAGS += -idirafter $$shell_quote($$LIBCLANG_ROOT/include)
LIBS += "$$LIBCLANG_ROOT/lib/libclang.dll.a"
DEFINES += LIBCLANG_ROOT=\\\"$$LIBCLANG_ROOT\\\"

QMAKE_LFLAGS += -Wl,--export-all-symbols
