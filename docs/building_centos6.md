# Building on Centos 6 #

Avecado requires moire recent compilers and libraries than are available by
default on Centos 6. They are met two ways, by installing ``devtoolset-2``,
and by installing Mapnik, Boost and other dependencies in ``$STDBASE``. If
that is done, the following will compile avecado.

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