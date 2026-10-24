QT += core gui quick network testlib
CONFIG += c++17 testcase
TARGET = omabuddy_tests
TEMPLATE = app

INCLUDEPATH += ../src

HEADERS += ../src/cli.h ../src/instance.h ../src/pet.h ../src/sessions.h ../src/sprites.h ../src/theme.h ../src/translator.h
SOURCES += omabuddy_tests.cpp ../src/cli.cpp ../src/instance.cpp ../src/pet.cpp ../src/sessions.cpp ../src/sprites.cpp \
    ../src/theme.cpp ../src/translator.cpp
