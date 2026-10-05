QT += widgets network

CONFIG += c++17

TARGET = PresenteGUI
TEMPLATE = app
VERSION = 1.0.0.0

QMAKE_TARGET_PRODUCT = Presente
QMAKE_TARGET_DESCRIPTION = Sistema de gestion docente S.I.G.E.
QMAKE_TARGET_COMPANY = Proyecto escolar
QMAKE_TARGET_COPYRIGHT = 2026

INCLUDEPATH += "C:/Program Files/MySQL/MySQL Server 8.0/include"
LIBS += -L"C:/Program Files/MySQL/MySQL Server 8.0/lib" -llibmysql

SOURCES += gui.cpp \
    api/ApiCliente.cpp \
    dao/SigeDAO.cpp \
    datos/ConexionBD.cpp

HEADERS += datos/ConfigAPI.h \
    datos/ConfigBD.h \
    datos/ConexionBD.h \
    api/ApiCliente.h \
    dao/SigeDAO.h \
    modelo/Docente.h
