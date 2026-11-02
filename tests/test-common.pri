QT += core gui widgets
CONFIG += c++11

TEST_ROOT = $$clean_path($$PWD/..)
INCLUDEPATH += $$TEST_ROOT $$TEST_ROOT/core $$TEST_ROOT/widget
RESOURCES += $$TEST_ROOT/widget/resources.qrc

isEmpty(LIBCLANG_ROOT): LIBCLANG_ROOT = D:/msys64/mingw64
QMAKE_CXXFLAGS += -idirafter $$shell_quote($$LIBCLANG_ROOT/include)
LIBS += "$$LIBCLANG_ROOT/lib/libclang.dll.a"
DEFINES += LIBCLANG_ROOT=\\\"$$LIBCLANG_ROOT\\\"
DEFINES += MSG_HEADER_PATH=\\\"$$TEST_ROOT/TestHeader/MsgHeader.h\\\"
