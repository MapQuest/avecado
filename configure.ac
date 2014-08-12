AC_INIT([avecado],
	[0.0.1],
	[https://github.com/MapQuest/avecado/issues],
	[avecado-0.0.1],
	[https://github.com/MapQuest/avecado])
AM_INIT_AUTOMAKE([subdir-objects parallel-tests])
AM_SILENT_RULES([yes])
AC_CONFIG_HEADERS([include/config.h])

AC_PROG_CXX
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AC_PROG_RANLIB

AC_HEADER_STDC
AC_LANG_CPLUSPLUS

AX_CXX_COMPILE_STDCXX_11([noext],[mandatory])
AX_BOOST_BASE([1.53], , [AC_MSG_ERROR([cannot find Boost libraries, which are are required for building avecado. Please install libboost-dev.])])
AX_PYTHON_DEVEL([>=2.6])
REQUIRE_MAPNIK([3.0.0])

AC_CONFIG_FILES([Makefile])

AC_OUTPUT
