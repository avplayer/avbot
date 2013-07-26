
EXTRA_DIST = CMakeLists.txt README.md

AM_CPPFLAGS = -DAVHTTP_ENABLE_OPENSSL \
		-I ${top_srcdir}/avproxy/include \
		-I ${top_srcdir}/avhttp/include \
		-I ${top_srcdir}/avboost/include \
		-I ${top_srcdir}/deCAPTCHA/include \
		@LUAJIT_CFLAGS@

noinst_LTLIBRARIES = libextension.la

libextension_la_SOURCES =  html.hpp extension.hpp extension.cpp joke.hpp joke.cpp urlpreview.hpp urlpreview.cpp bulletin.hpp bulletin.cpp metalprice.hpp stockprice.hpp

libextension_la_LDFLAGS = -static

libextension_la_LIBADD =  ${BOOST_SYSTEM_LIBS} ${BOOST_FILESYSTEM_LIBS}

if ENABLE_LUA

SUBDIRS = luascript

libextension_la_LIBADD += ${builddir}/luascript/libluascript.la

endif
