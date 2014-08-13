AM_LDFLAGS = @BOOST_LDFLAGS@
AM_CPPFLAGS = \
	-Iinclude -Imapnik-vector-tile/src \
	@BOOST_CPPFLAGS@ @MAPNIK_CPPFLAGS@ @PROTOC_CPPFLAGS@

BUILT_SOURCES = \
	include/vector_tile.pb.h \
	src/vector_tile.pb.cc

CLEANFILES = \
	include/vector_tile.pb.h \
	src/vector_tile.pb.cc

src/vector_tile.pb.cc: include/vector_tile.pb.h

include/vector_tile.pb.h: mapnik-vector-tile/proto/vector_tile.proto
	@PROTOC_BIN@ -Imapnik-vector-tile/proto --cpp_out=src $< && mv src/vector_tile.pb.h include/

bin_PROGRAMS = avecado

avecado_SOURCES = \
	src/avecado.cpp \
	src/avecado_util.cpp

nodist_avecado_SOURCES = \
	src/vector_tile.pb.cc

avecado_LDADD = @MAPNIK_LIBS@ @PROTOC_LIBS@ @BOOST_PROGRAM_OPTIONS_LIB@
