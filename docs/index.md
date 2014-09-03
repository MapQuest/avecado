# Avecado Library #

## Motivation ##

Avecado ("a*vec*ado") was created to both have functionality not in existing
vector tile creation libraries and to align in language used with other
components of the burritile map stack.

## Building ##

Avecado requires a recent version of [Mapnik](http://mapnik.org/) 3.x,
[Boost libraries](http://boost.org/), and a C++11 compiler, normally GCC 4.8.
The [other](https://github.com/MapQuest/avecado#building) dependencies will
normally be met easily.

On Centos 6 machines, this means ``devtoolset-2`` needs to be installed. Assuming
Mapnik, Boost and other dependencies are installed in ``$STDBASE`` the
following will compile avecado.

```sh
export STDBASE=/data/mapquest/stdbase_devtools
export PATH=.:/opt/rh/devtoolset-2/root/bin:/opt/rh/devtoolset-2/root/usr/bin:$STDBASE/bin:/bin:/opt/bcs/bin:/usr/bin:/bin:/usr/sbin/sbin
export LD_LIBRARY_PATH=/opt/rh/devtoolset-2/root/usr/lib:/opt/rh/devtoolset-2/root/lib:$STDBASE/lib:$STDBASE/lib64:/lib:/lib64:/usr/lib64/:/usr/lib

# Regenerate files, as they may have been generated with different versions of autotools
rm -rf INSTALL Makefile.in aclocal.m4 config.guess config.log config.sub \
  configure depcomp include/config.h.in include/config.h.in~ install-sh \
  ltmain.sh autom4te.cache/ missing
./autogen.sh

CPPFLAGS="-DBOOST_MPL_CFG_NO_PREPROCESSED_HEADERS -DBOOST_MPL_LIMIT_VECTOR_SIZE=30" \
./configure --with-boost=$STDBASE --with-boost-libdir=$STDBASE/lib \
  --with-mapnik-config=$STDBASE/bin/mapnik-config --with-protoc=$STDBASE/bin/protoc \
  --with-protobuf-includes=$STDBASE/include --with-protobuf-libdir=$STDBASE/lib \
  CXXFLAGS="-O2 -Wno-unused-local-typedefs -Wno-unused-variable -Wno-unused-but-set-variable" \
  PYTHON=$STDBASE/bin/python

make clean
make
```

## Mapnik Vector Tile ##

Avecado embeds [mapnik-vector-tile](https://github.com/mapbox/mapnik-vector-tile)
as a subrepository. Its version needs to match with Mapnik's. If building on a
system with an older or newer Mapnik, an older or newer version of
mapnik-vector-tile should be used. This is complicated, but should stabalize
when Mapnik 3 is released.

## Components ##

### Tile creation ###

The ``make_vector_tile`` function is a wrapper around the rendering functions
of mapnik-vector-tile, and its parameters are passed on to mapnik-vector-tile
and Mapnik. When calling, it needs to be passed a ``mapnik::Map``, which will
need to be resized and zoomed to a bounding box. A typical example of creating
a single vector tile can be found in [avecado_exporter.cpp](../src/avecado_exporter.cpp#168-197).

### Tile post-processing ###

After creating a vector tile, Avecado can pass it to a series of "izers" to
perform processing steps such as simplification and merging same-named lines.

Each post-processing step takes in a vector layer and manipulates it,
generally resulting in a differing vector layer.

## Tests ##
Avecado comes with a test suite, run with ``make check``. The tests assume
that serialization of JSON representations of Mapnik featuresets are constant
across computers, which is believed to be true. If you find a platform where
this is not true, [open an issue](https://github.com/MapQuest/avecado/issues/new).
