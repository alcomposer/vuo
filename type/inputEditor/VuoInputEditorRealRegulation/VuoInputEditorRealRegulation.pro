TEMPLATE = lib
CONFIG += plugin qtCore qtGui json VuoPCH VuoInputEditor
TARGET = VuoInputEditorRealRegulation

include(../../../vuo.pri)

SOURCES +=\
	VuoInputEditorRealRegulation.cc

HEADERS += \
	VuoInputEditorRealRegulation.hh

OTHER_FILES += \
	VuoInputEditorRealRegulation.json

INCLUDEPATH += \
	$$ROOT/library

LIBS += \
	$$ROOT/library/libVuoHeap.dylib \
	$$ROOT/type/VuoRealRegulation.o \
	$$ROOT/type/VuoInteger.o \
	$$ROOT/type/VuoReal.o \
	$$ROOT/type/VuoText.o \
	$$ROOT/type/VuoDictionary_VuoText_VuoReal.o \
	$$ROOT/type/list/VuoList_VuoInteger.o \
	$$ROOT/type/list/VuoList_VuoReal.o \
	$$ROOT/type/list/VuoList_VuoText.o \
	$$ROOT/library/VuoMathExpressionParser.o \
	$$MUPARSER_ROOT/lib/libmuparser.a