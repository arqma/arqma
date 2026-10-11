OSX_MIN_VERSION=11.0
OSX_SDK_VERSION=11.0
XCODE_VERSION=12.2
XCODE_BUILD_ID=12B45b
LD64_VERSION=711

OSX_SDK=$(host_prefix)/native/SDK

# Prefer the modern clang+lld path used by Monero/Bitcoin over the legacy
# cctools/libtapi stack. Host clang against cctools ld fails libzmq's
# AC_LINK_IFELSE C++ empty-program check on current Ubuntu runners.
darwin_native_toolchain=darwin_sdk

clang_prog=clang
clangxx_prog=clang++

darwin_AR=llvm-ar
darwin_DSYMUTIL=dsymutil
darwin_NM=llvm-nm
darwin_OBJDUMP=llvm-objdump
darwin_RANLIB=llvm-ranlib
darwin_STRIP=llvm-strip

darwin_CC=env -u C_INCLUDE_PATH -u CPLUS_INCLUDE_PATH \
              -u OBJC_INCLUDE_PATH -u OBJCPLUS_INCLUDE_PATH -u CPATH \
              -u LIBRARY_PATH \
            $(clang_prog) --target=$(host) -mmacosx-version-min=$(OSX_MIN_VERSION) \
              -mlinker-version=$(LD64_VERSION) \
              -isysroot$(OSX_SDK) -nostdlibinc \
              -iwithsysroot/usr/include -iframeworkwithsysroot/System/Library/Frameworks

darwin_CXX=env -u C_INCLUDE_PATH -u CPLUS_INCLUDE_PATH \
               -u OBJC_INCLUDE_PATH -u OBJCPLUS_INCLUDE_PATH -u CPATH \
               -u LIBRARY_PATH \
             $(clangxx_prog) --target=$(host) -mmacosx-version-min=$(OSX_MIN_VERSION) \
               -mlinker-version=$(LD64_VERSION) \
               -isysroot$(OSX_SDK) -nostdlibinc \
               -iwithsysroot/usr/include/c++/v1 \
               -iwithsysroot/usr/include -iframeworkwithsysroot/System/Library/Frameworks

darwin_CFLAGS=-pipe -std=$(C_STANDARD)
darwin_CXXFLAGS=-pipe -std=$(CXX_STANDARD)
darwin_LDFLAGS=-Wl,-platform_version,macos,$(OSX_MIN_VERSION),$(OSX_SDK_VERSION) -Wl,-no_adhoc_codesign -fuse-ld=lld
darwin_ARFLAGS=cr

darwin_release_CFLAGS=-O2
darwin_release_CXXFLAGS=$(darwin_release_CFLAGS)

darwin_debug_CFLAGS=-O1
darwin_debug_CXXFLAGS=$(darwin_debug_CFLAGS)

darwin_cmake_system=Darwin
