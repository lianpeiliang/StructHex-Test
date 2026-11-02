# Top-level Qt Creator project: build the reusable core first, then the Widgets app.
TEMPLATE = subdirs
CONFIG += ordered

isEmpty(LIBCLANG_ROOT): LIBCLANG_ROOT = D:/msys64/mingw64
QMAKE_EXTRA_ARGS += LIBCLANG_ROOT=$$LIBCLANG_ROOT

SUBDIRS += core widget
core.file = core/StructHexCore.pro
widget.file = widget/StructHexWidget.pro
widget.depends = core
