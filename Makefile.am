ACLOCAL_AMFLAGS = -I m4
AM_LDFLAGS = @BOOST_LDFLAGS@
AM_CPPFLAGS = \
	-Iinclude -Imapnik-vector-tile/src \
	@BOOST_CPPFLAGS@ @MAPNIK_CPPFLAGS@ @PROTOC_CPPFLAGS@

LIBTOOL_DEPS = @LIBTOOL_DEPS@
libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status libtool

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
	src/avecado.cpp

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
avecado_la_LIBADD = libavecado.la @BOOST_LDFLAGS@ -l@BOOST_PYTHON_LIB@
endif
