AM_LDFLAGS = @BOOST_LDFLAGS@
AM_CPPFLAGS = \
	-Iinclude -Imapnik-vector-tile/src -Imapnik-vector-tile/proto \
	@BOOST_CPPFLAGS@ @MAPNIK_CPPFLAGS@ @PROTOC_CPPFLAGS@

BUILT_SOURCES = \
	mapnik-vector-tile/proto/vector_tile.pb.h \
	mapnik-vector-tile/proto/vector_tile.pb.cc

CLEANFILES = \
	mapnik-vector-tile/proto/vector_tile.pb.h \
	mapnik-vector-tile/proto/vector_tile.pb.cc

mapnik-vector-tile/proto/vector_tile.pb.cc: mapnik-vector-tile/proto/vector_tile.pb.h

mapnik-vector-tile/proto/vector_tile.pb.h: mapnik-vector-tile/proto/vector_tile.proto
	@PROTOC_BIN@ -Imapnik-vector-tile/proto --cpp_out=mapnik-vector-tile/proto $<

bin_PROGRAMS = avecado

avecado_SOURCES = \
	src/avecado.cpp \
	src/avecado_util.cpp

nodist_avecado_SOURCES = \
	mapnik-vector-tile/proto/vector_tile.pb.cc

avecado_LDADD = @MAPNIK_LIBS@ @PROTOC_LIBS@
