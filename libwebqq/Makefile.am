
EXTRA_DIST = impl utf CMakeLists.txt README.md

AM_CPPFLAGS = -DAVHTTP_ENABLE_OPENSSL \
		-I ${top_srcdir}/avproxy/include \
		-I ${top_srcdir}/avhttp/include \
		-I ${top_srcdir}/avboost/include \
		-I ${top_srcdir}/deCAPTCHA/include

noinst_LTLIBRARIES = libwebqq.la

libwebqq_la_SOURCES = error_code.cpp error_code.hpp webqq.cpp webqq.hpp impl/webqq_face.cpp impl/webqq_impl.cpp

libwebqq_la_LDFLAGS = -static

libwebqq_la_LIBADD = ${BOOST_THREAD_LIBS} ${BOOST_SYSTEM_LIBS} ${BOOST_REGEX_LIBS} ${BOOST_SIGNALS_LIBS}
