      ____  __ __    ___    __   ____  ___     ___
     /    ||  |  |  /  _|  /  | /    ||   \   /   \
    |  o  ||  |  | /  |_  /  / |  o  ||    \ |     |
    |     ||  |  ||    _|/  /  |     ||     ||     |
    |  _  ||  :  ||   |_/   \_ |  _  ||  D  ||  O  |
    |  |  | \   / |     \     ||  |  ||     ||     |
    |__|__|  \_/  |_____|\____||__|__||_____| \___/


Avecado (pronounced "a*vec*ado") is a library for building Mapnik vector tiles, along with language bindings for Python and some utility programs.

Building
--------

Avecado uses the [GNU Build System](http://www.gnu.org/software/automake/manual/html_node/GNU-Build-System.html) to configure and build itself and requires [Mapnik](http://mapnik.org/) 3.x, the [Boost libraries](http://boost.org/) and [Python](http://python.org/). To install on a Debian or Ubuntu system, please first install the prerequisites:

```
sudo apt-get install build-essential autoconf automake libtool \
  g++ python-dev libprotobuf-dev protobuf-compiler
```

You will need 1.56 or later of the Boost libraries. They can be obtained at [boost.org](http://boost.org)

You will also need to install a 3.x version of the Mapnik library. This version of the library was tested with commit `7ee9745` from [Mapnik master](https://github.com/mapnik/mapnik).

Then you should be able to bootstrap the build system:

    ./autogen.sh

You need to increase the maximum vector size to 30 and then run the standard GNU build install:

    CPPFLAGS="-DBOOST_MPL_CFG_NO_PREPROCESSED_HEADERS -DBOOST_MPL_LIMIT_VECTOR_SIZE=30" \
    ./configure && make && make install

Please see `./configure --help` for more options on how to control the build process.

There are detailed [instructions for CentOS 6](docs/building_centos6.md) in the `docs/` folder.

Using
-----

Avecado has three main executable programs:

`avecado`, a simple command-line utility for producing vector tiles. Since each invocation loads a Mapnik map, this is not recommended for generating large volumes of vector tiles. However, it can be useful for one-off testing and debugging of datasource definitions.

`avecado_server`, a very simple HTTP server which serves vector tiles according to the input Mapnik map. The HTTP server is extremely basic and, while it might be useful for ad-hoc testing purposes, is not suitable for production use. It serves tiles in the Google Maps numbering scheme; for example z=2, x=1, y=0 would be available as http://localhost:8080/2/1/0.pbf if the server is run on port 8080. This is explained in more detail on [the OpenStreetMap wiki](http://wiki.openstreetmap.org/wiki/Slippy_map_tilenames) and [Google's developer documentation](https://developers.google.com/maps/documentation/javascript/v2/overlays#Google_Maps_Coordinates).

`scripts/override_xml.py`, a utility for altering settings in an XML datasource configuration. This is useful for correcting or overriding any settings which may be different between the output of Mapbox Studio, or your configuration files in version control, and your local or production setups.

Each of these programs takes as input a Mapnik XML file. When used to make vector tiles they do not read the styling information and instead look only at the `Map` definition, `Layer`s and `Datasource`s. These make up the definition of the vector tile.

Styling information is applied at the time the vector tile is rendered - either on the client, another server, or another instance of a program using Avecado.

Contributing
------------

We welcome contributions to Avecado. If you would like to report an issue, please use the [Avecado issue tracker](https://github.com/MapQuest/avecado/issues) on GitHub.

If you would like to make an improvement to the code, please be aware that Avecado is written mostly in C++11, in the K&R (1TBS variant) with two spaces as indentation. We welcome contributions as pull requests to the [repository](https://github.com/MapQuest/avecado).

It is possible to build a test coverage report, please see [test coverage documentation](docs/test_coverage.md) for details.

