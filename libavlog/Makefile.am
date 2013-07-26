
EXTRA_DIST = CMakeLists.txt

AM_CPPFLAGS = -DAVHTTP_ENABLE_OPENSSL \
		-I ${top_srcdir}/avproxy/include \
		-I ${top_srcdir}/avhttp/include \
		-I ${top_srcdir}/avboost/include \
		-I ${top_srcdir}/deCAPTCHA/include

noinst_LTLIBRARIES = libavlog.la

libavlog_la_SOURCES = avlog.cpp avlog.hpp

libavlog_la_LDFLAGS = -static

libavlog_la_LIBADD = ${BOOST_SYSTEM_LIBS} ${BOOST_FILESYSTEM_LIBS}
