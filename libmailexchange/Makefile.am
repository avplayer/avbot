
EXTRA_DIST = CMakeLists.txt FindICU.cmake smtptest.cpp pop3test.cpp

AM_CPPFLAGS = -DAVHTTP_ENABLE_OPENSSL \
		-I ${top_srcdir}/avproxy/include \
		-I ${top_srcdir}/avhttp/include \
		-I ${top_srcdir}/avboost/include \
		-I ${top_srcdir}/deCAPTCHA/include

noinst_LTLIBRARIES = libmx.la

libmx_la_SOURCES = pop3.cpp smtp.cpp smtp.hpp pop3.hpp internet_mail_format.hpp  mx.hpp

libmx_la_LDFLAGS = -static

libmx_la_LIBADD = ${BOOST_SYSTEM_LIBS} ${BOOST_REGEX_LIBS} ${BOOST_SIGNALS_LIBS} ${BOOST_LOCALE_LIBS}