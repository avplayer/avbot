
EXTRA_DIST = CMakeLists.txt

AM_CPPFLAGS = -DAVHTTP_ENABLE_OPENSSL \
		-I ${top_srcdir}/avproxy/include \
		-I ${top_srcdir}/avhttp/include \
		-I ${top_srcdir}/avboost/include \
		-I ${top_srcdir}/deCAPTCHA/include

noinst_LTLIBRARIES = libavbot.la

libavbot_la_SOURCES = avbot.cpp avbot.hpp

libavbot_la_LDFLAGS = -static

libavbot_la_LIBADD = ${top_builddir}/libxmpp/libxmpp.la \
			${top_builddir}/libwebqq/libwebqq.la \
			${top_builddir}/libmailexchange/libmx.la \
			${BOOST_SYSTEM_LIBS} ${BOOST_REGEX_LIBS} ${BOOST_LOCALE_LIBS}
