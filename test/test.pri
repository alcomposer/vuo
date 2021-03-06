QMAKE_CXXFLAGS += \
	$(INCPATH) \
	$(DEFINES) \
	-I$$ROOT/base \
	-I$$ROOT/library \
	-I$$ROOT/node \
	-I$$ROOT/node/vuo.audio \
	-I$$ROOT/node/vuo.font \
	-I$$ROOT/node/vuo.layer \
	-I$$ROOT/node/vuo.midi \
	-I$$ROOT/node/vuo.video \
	-I$$ROOT/runtime \
	-I$$ROOT/type \
	-I$$ROOT/type/list \
	$$QMAKE_CXXFLAGS_WARN_ON

QMAKE_LFLAGS += \
	$$ZMQ_ROOT/lib/libzmq.a \
	$${GRAPHVIZ_ROOT}/lib/libgvc.dylib \
	$${GRAPHVIZ_ROOT}/lib/libgraph.dylib \
	$${GRAPHVIZ_ROOT}/lib/graphviz/libgvplugin_dot_layout.dylib \
	$${GRAPHVIZ_ROOT}/lib/graphviz/libgvplugin_core.dylib \
	$$MUPARSER_ROOT/lib/libmuparser.a \
	$$FFMPEG_ROOT/lib/libavcodec.dylib \
	$$FFMPEG_ROOT/lib/libavformat.dylib \
	$$FFMPEG_ROOT/lib/libavutil.dylib \
	$$FFMPEG_ROOT/lib/libswresample.dylib \
	$$FFMPEG_ROOT/lib/libswscale.dylib \
	$$CURL_ROOT/lib/libcurl.a \
	-lssl \
	-lcrypto \
	-lobjc \
	-framework Foundation \
	-framework QuartzCore \
	-framework AppKit \
	$$ROOT/runtime/libVuoRuntime.bc \
	$$ROOT/base/VuoCompositionStub.o \
	$$ROOT/base/VuoTelemetry.o \
	$$ROOT/library/libVuoGlContext.dylib \
	$$ROOT/library/libVuoGlPool.dylib \
	$$ROOT/library/libVuoHeap.dylib \
	$$ROOT/library/VuoImageBlur.bc \
	$$ROOT/library/VuoImageMapColors.o \
	$$ROOT/library/VuoImageRenderer.o \
	$$ROOT/library/VuoImageText.o \
	$$ROOT/library/VuoMeshParametric.o \
	$$ROOT/library/VuoMathExpressionParser.o \
	$$ROOT/library/VuoSceneRenderer.o \
	$$ROOT/library/VuoUrlFetch.o \
	$$ROOT/type/VuoAudioSamples.o \
	$$ROOT/type/VuoBoolean.o \
	$$ROOT/type/VuoBlendMode.o \
	$$ROOT/type/VuoColor.o \
	$$ROOT/type/VuoImage.o \
	$$ROOT/type/VuoImageColorDepth.o \
	$$ROOT/type/VuoInteger.o \
	$$ROOT/node/vuo.font/VuoFont.o \
	$$ROOT/node/vuo.layer/VuoLayer.o \
	$$ROOT/node/vuo.midi/VuoMidiNote.o \
	$$ROOT/node/vuo.video/VuoMovie.o \
	$$ROOT/type/VuoDictionary_VuoText_VuoReal.o \
	$$ROOT/type/VuoMesh.o \
	$$ROOT/type/VuoMathExpressionList.o \
	$$ROOT/type/VuoPoint2d.o \
	$$ROOT/type/VuoPoint3d.o \
	$$ROOT/type/VuoPoint4d.o \
	$$ROOT/type/VuoReal.o \
	$$ROOT/type/VuoSceneObject.o \
	$$ROOT/type/VuoShader.o \
	$$ROOT/type/VuoText.o \
	$$ROOT/type/VuoTransform.o \
	$$ROOT/type/VuoTransform2d.o \
	$$ROOT/type/VuoUrl.o \
	$$ROOT/type/list/VuoList_VuoAudioSamples.o \
	$$ROOT/type/list/VuoList_VuoBoolean.o \
	$$ROOT/type/list/VuoList_VuoBlendMode.o \
	$$ROOT/type/list/VuoList_VuoColor.o \
	$$ROOT/type/list/VuoList_VuoImage.o \
	$$ROOT/type/list/VuoList_VuoImageColorDepth.o \
	$$ROOT/type/list/VuoList_VuoInteger.o \
	$$ROOT/type/list/VuoList_VuoLayer.o \
	$$ROOT/type/list/VuoList_VuoPoint2d.o \
	$$ROOT/type/list/VuoList_VuoPoint3d.o \
	$$ROOT/type/list/VuoList_VuoReal.o \
	$$ROOT/type/list/VuoList_VuoSceneObject.o \
	$$ROOT/type/list/VuoList_VuoText.o \
	$$QMAKE_LFLAGS $$LIBS $$QMAKE_LIBS \
	-Wl,-rpath,$$ROOT/framework \
	"-framework QtTest" \
	"-framework QtCore" \
	"-framework IOSurface" \
	"-framework OpenGL"
