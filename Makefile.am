ACLOCAL_AMFLAGS = -I m4
AM_LDFLAGS = @BOOST_LDFLAGS@ @COVERAGE_LDFLAGS@
AM_CPPFLAGS = \
	-Iinclude -Imapnik-vector-tile/src \
	@BOOST_CPPFLAGS@ @MAPNIK_CPPFLAGS@ @PROTOC_CPPFLAGS@
AM_CXXFLAGS = @COVERAGE_CXXFLAGS@

LIBTOOL_DEPS = @LIBTOOL_DEPS@
libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status libtool

if ENABLE_COVERAGE

.PHONY: clean-coverage
clean-coverage:
	-find -name '*.gcda' -exec rm -rf {} \;
	-$(LCOV) --directory $(top_builddir) -z
	-rm -rf coverage.info coverage/

.PHONY: coverage-report
coverage-report: clean-coverage
	-$(MAKE) $(AM_MAKEFLAGS) -k check
	$(MAKE) $(AM_MAKEFLAGS) coverage/index.html

coverage.info:
	$(LCOV) --directory $(top_builddir) --base-directory $(top_builddir) --no-external --capture --output-file $@ --no-checksum --compat-libtool

coverage/index.html: coverage.info
	$(GENHTML) --prefix $(top_builddir) --output-directory $(@D) --title "Test Coverage" --legend --show-details $<


.PHONY: clean-gcno
clean-gcno:
	-find -name '*.gcno' -exec rm -rf {} \;

clean-local: clean-coverage clean-gcno

endif # ENABLE_COVERAGE

noinst_LTLIBRARIES = liblogging.la

BUILT_SOURCES = \
	include/vector_tile.pb.h \
	src/vector_tile.pb.cc

CLEANFILES = \
	include/vector_tile.pb.h \
	src/vector_tile.pb.cc

src/vector_tile.pb.cc: include/vector_tile.pb.h

include/vector_tile.pb.h: mapnik-vector-tile/proto/vector_tile.proto
	@PROTOC_BIN@ -Imapnik-vector-tile/proto --cpp_out=src $< && mv src/vector_tile.pb.h include/

lib_LTLIBRARIES = libavecado.la

include_HEADERS = include/avecado.hpp

libavecado_la_SOURCES = \
	src/avecado.cpp \
	src/backend.cpp \
	src/tile.cpp \
	src/post_processor.cpp \
	src/post_process/adminizer.cpp \
	src/post_process/generalizer.cpp \
	src/post_process/labelizer.cpp \
	src/post_process/unionizer.cpp

nodist_libavecado_la_SOURCES = \
	src/vector_tile.pb.cc

libavecado_la_LIBADD = @MAPNIK_LIBS@ @PROTOC_LIBS@ @BOOST_PROGRAM_OPTIONS_LIB@

bin_PROGRAMS = avecado avecado_server

avecado_SOURCES = \
	src/avecado_exporter.cpp

avecado_LDADD = \
	libavecado.la

avecado_server_SOURCES = \
	src/avecado_server.cpp \
	src/http_server/connection.cpp \
	src/http_server/mime_types.cpp \
	src/http_server/parse_path.cpp \
	src/http_server/reply.cpp \
	src/http_server/request_handler.cpp \
	src/http_server/request_parser.cpp \
	src/http_server/server.cpp

avecado_server_CXXFLAGS = @PTHREAD_CFLAGS@

avecado_server_LDADD = \
	libavecado.la \
	@BOOST_LDFLAGS@ @BOOST_ASIO_LIB@ @BOOST_THREAD_LIB@ @PTHREAD_LIBS@

if HAVE_BOOST_PYTHON
pyexec_LTLIBRARIES = avecado.la
avecado_la_CXXFLAGS = @PYTHON_CPPFLAGS@
avecado_la_SOURCES = src/python_module.cpp
avecado_la_LDFLAGS = -avoid-version -module @PYTHON_LDFLAGS@
avecado_la_LIBADD = libavecado.la liblogging.la @BOOST_LDFLAGS@ -l@BOOST_PYTHON_LIB@
endif

check_PROGRAMS = \
	test/make_vector_tile \
	test/generalizer \
	test/adminizer \
	test/multi_verification

liblogging_la_SOURCES = \
	logging/logger.cpp \
	logging/stdout_logger.cpp \
	logging/file_logger.cpp \
	logging/null_logger.cpp

test_make_vector_tile_SOURCES = test/make_vector_tile.cpp test/common.cpp
test_make_vector_tile_LDADD = libavecado.la liblogging.la @BOOST_LDFLAGS@ @BOOST_ASIO_LIB@ @BOOST_THREAD_LIB@ @PTHREAD_LIBS@
test_generalizer_SOURCES = test/generalizer.cpp test/common.cpp
test_generalizer_LDADD = libavecado.la liblogging.la @BOOST_LDFLAGS@ @BOOST_ASIO_LIB@ @BOOST_THREAD_LIB@ @PTHREAD_LIBS@
test_adminizer_SOURCES = test/adminizer.cpp test/common.cpp
test_adminizer_LDADD = libavecado.la liblogging.la @BOOST_LDFLAGS@ @BOOST_ASIO_LIB@ @BOOST_THREAD_LIB@ @PTHREAD_LIBS@
test_multi_verification_SOURCES = test/multi_verification.cpp test/common.cpp
test_multi_verification_LDADD = libavecado.la liblogging.la @BOOST_LDFLAGS@ @BOOST_ASIO_LIB@ @BOOST_THREAD_LIB@ @PTHREAD_LIBS@

TESTS = $(check_PROGRAMS)
TEST_EXTENSIONS = .sh
SH_LOG_COMPILER = sh

test: check
