QT += core gui qml quick network

CONFIG += c++17 release
TARGET = omabuddy
TEMPLATE = app

LIBS += -lLayerShellQtInterface

HEADERS += \
    src/cli.h \
    src/instance.h \
    src/pet.h \
    src/pixelsprite.h \
    src/sessions.h \
    src/sprites.h \
    src/theme.h \
    src/translator.h

SOURCES += \
    src/main.cpp \
    src/cli.cpp \
    src/instance.cpp \
    src/pet.cpp \
    src/pixelsprite.cpp \
    src/sessions.cpp \
    src/sprites.cpp \
    src/theme.cpp \
    src/translator.cpp

RESOURCES += src/resources.qrc
