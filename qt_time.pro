QT       += core gui opengl widgets openglwidgets

CONFIG += c++11

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

INCLUDEPATH += Libraries

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    myglwidget.cpp \
    opengllog.cpp

HEADERS += \
    mainwindow.h \
    myglwidget.h \
    opengllog.h

FORMS +=

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

# Depend on OpenGL
win32:LIBS += -lopengl32
linux:LIBS += -lGL
macx:LIBS += -framework OpenGL -framework CoreFoundation -framework GLUT

RESOURCES += \
    Assets/resource.qrc
