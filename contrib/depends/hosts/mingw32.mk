mingw32_CFLAGS=-pipe -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0
mingw32_CXXFLAGS=$(mingw32_CFLAGS)
mingw32_ARFLAGS=cr

mingw32_release_CFLAGS=-O2
mingw32_release_CXXFLAGS=$(mingw32_release_CFLAGS)

mingw32_debug_CFLAGS=-O1
mingw32_debug_CXXFLAGS=$(mingw32_debug_CFLAGS)

mingw32_debug_CPPFLAGS=-D_GLIBCXX_DEBUG -D_GLIBCXX_DEBUG_PEDANTIC

mingw32_cmake_system=Windows