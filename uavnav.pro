TEMPLATE = app
CONFIG -= app_bundle
CONFIG -= qt

LIBS += -lpthread

SOURCES += uavnav_main.c
HEADERS += \
    util.h \
    tsip_decode.h \
    tsip_read.h

copydata.commands = $(COPY_DIR) $$PWD/data $$OUT_PWD
first.depends = $(first) copydata
export(first.depends)
export(copydata.commands)
QMAKE_EXTRA_TARGETS += first copydata
