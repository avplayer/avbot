
EXTRA_DIST = CMakeLists.txt test.cpp

SUBDIRS = gloox

AM_CPPFLAGS = -DAVHTTP_ENABLE_OPENSSL \
		-I ${top_srcdir}/avproxy/include \
		-I ${top_srcdir}/avhttp/include \
		-I ${top_srcdir}/avboost/include \
		-I ${top_srcdir}/deCAPTCHA/include

AM_CPPFLAGS += -I ${srcdir}/gloox

noinst_LTLIBRARIES = libxmpp.la

libxmpp_la_SOURCES = xmpp.h xmpp.cpp xmpp_impl.h xmpp_impl.cpp

libxmpp_la_LDFLAGS = -static

libxmpp_la_LIBADD = ${builddir}/gloox/gloox/libgloox.la