# enve - 2D animations software
# Copyright (C) 2016-2020 Maurycy Liebner

# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

#-------------------------------------------------
#
# Project created by QtCreator 2019-06-02T11:49:48
#
#-------------------------------------------------

VERSION = 0.0.0

QT += core qml xml
CONFIG += c++14
ENVE_FOLDER = $$PWD/../../..

INCLUDEPATH += $$ENVE_FOLDER/include
DEPENDPATH += $$ENVE_FOLDER/include

SKIA_FOLDER = $$ENVE_FOLDER/third_party/skia
INCLUDEPATH += $$SKIA_FOLDER
DEPENDPATH += $$SKIA_FOLDER

CONFIG(debug, debug|release) {
    win32 { # Windows
        LIBS += -L$$SKIA_FOLDER/out/Debug -lskia
    }
} else {
    win32 { # Windows
        LIBS += -L$$SKIA_FOLDER/out/Release -lskia
        QMAKE_CFLAGS_RELEASE += /O2 -O2
        QMAKE_CXXFLAGS_RELEASE += /O2 -O2
    } unix {
        macx { # Mac OS X

        } else { # Linux
            QMAKE_CFLAGS -= -O2
            QMAKE_CFLAGS -= -O1
            QMAKE_CXXFLAGS -= -O2
            QMAKE_CXXFLAGS -= -O1
            QMAKE_CFLAGS = -m64 -O3
            QMAKE_LFLAGS = -m64 -O3
            QMAKE_CXXFLAGS = -m64 -O3
        }
    }
}

LIBS += -L$$OUT_PWD/../../../src/core -lenvecore

TARGET = eBlur
TEMPLATE = lib

DEFINES += EBLUR_LIBRARY

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    eblur.cpp

HEADERS += \
        eblur.h \
        eblur_global.h
