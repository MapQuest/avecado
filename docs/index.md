# Avecado Library #

## Motivation ##

Avecado ("a*vec*ado") was created to both have functionality not in existing
vector tile creation libraries and to align in language used with other
components of the burritile map stack (pending release).

## Building ##

Avecado requires a recent version of [Mapnik](http://mapnik.org/) 3.x,
[Boost libraries](http://boost.org/), and a C++11 compiler, normally GCC 4.8.
The [other](https://github.com/MapQuest/avecado#building) dependencies will
normally be met easily.

## Testing ##

Tests can be found in the `tests/` directory, and are run by autotools
when you run `make check`. If you are preparing a pull request or
patch, please make sure these pass before submitting it.

There is a test coverage tool built into the build system, but it's
fairly annoying to use. Please see
[the coverage docs](test_coverage.md) for more information and details
on running it.

## Mapnik Vector Tile ##

Avecado embeds [mapnik-vector-tile](https://github.com/mapbox/mapnik-vector-tile)
as a subrepository. Its version needs to match with Mapnik's. If building on a
system with an older or newer Mapnik, an older or newer version of
mapnik-vector-tile should be used. The version of Mapnik used when testing
this version of Avecado was `87e9c64f`. This is complicated, but should stabilize
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
