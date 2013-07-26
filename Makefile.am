
DISTCHECK_CONFIGURE_FLAGS = --enable-lua
EXTRA_DIST = README.md m4 deCAPTCHA avproxy avhttp boost libirc gui CMakeLists.txt fsconfig.ipp  avbot.rc avbot.manifest

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}
AUTOMAKE_OPTIONS = color-tests parallel-tests

SUBDIRS = libmailexchange libwebqq libxmpp libirc libavbot libavlog extension

AM_CPPFLAGS = -DAVHTTP_ENABLE_OPENSSL \
		-I ${top_srcdir}/avproxy/include \
		-I ${top_srcdir}/avhttp/include \
		-I ${top_srcdir}/avboost/include \
		-I ${top_srcdir}/deCAPTCHA/include

bin_PROGRAMS = avbot

avbot_SOURCES = avbot_vc_feed_input.hpp main.cpp botctl.cpp avbot_rpc_server.cpp avbot_rpc_server.hpp \
		auto_welcome.hpp  botctl.hpp  counter.hpp resource.h input.cpp input.hpp
if OS_WINNT
avbot_SOURCES += avbot.rc
endif

avbot_LDFLAGS=${BOOST_THREAD_LDFLAGS}

avbot_LDADD = ${top_builddir}/libavbot/libavbot.la ${top_builddir}/libavlog/libavlog.la ${top_builddir}/extension/libextension.la

avbot_LDADD += ${BOOST_LOG_SETUP_LIBS} ${BOOST_LOG_LIBS} ${BOOST_THREAD_LIBS} ${BOOST_SYSTEM_LIBS} ${BOOST_PROGRAM_OPTIONS_LIBS} ${BOOST_FILESYSTEM_LIBS}  ${BOOST_REGEX_LIBS} ${BOOST_SIGNALS_LIBS} ${BOOST_LOCALE_LIBS}

.rc.o:
	${RC} -o $@ $<

rpm: dist-bzip2 @PACKAGE_NAME@.spec
	rpmbuild -bb \
		--define "_sourcedir `pwd`" \
		--define "_builddir `pwd`" \
		--define "_specdir `pwd`" \
		--define "_rpmdir `pwd`" \
		--define "_srcrpmdir `pwd`" \
		@PACKAGE_NAME@.spec

rpm32: dist-bzip2 @PACKAGE_NAME@.spec
	CXXFLAGS="-m32 -Os -g0" CFLAGS="-m32 -Os -g0" rpmbuild -bb --target=i686 \
		--define "_sourcedir `pwd`" \
		--define "_builddir `pwd`" \
		--define "_specdir `pwd`" \
		--define "_rpmdir `pwd`" \
		--define "_srcrpmdir `pwd`" \
		@PACKAGE_NAME@.spec
