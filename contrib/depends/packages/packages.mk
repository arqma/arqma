packages:=boost openssl zeromq libiconv

native_packages :=

hardware_packages := hidapi libusb
hardware_native_packages :=

android_native_packages = android_ndk
android_packages = ncurses readline sodium

darwin_native_packages = $(hardware_native_packages)
darwin_packages = ncurses readline sodium $(hardware_packages)

linux_packages = unwind eudev ncurses readline sodium $(hardware_packages)
linux_native_packages = $(hardware_native_packages)

ifeq ($(build_tests),ON)
packages += gtest
endif

mingw32_packages = icu4c sodium $(hardware_packages)
mingw32_native_packages = $(hardware_native_packages)

$(host_arch)_$(host_os)_native_packages += native_b2

ifneq ($(build_os), darwin)
darwin_native_packages += darwin_sdk native_clang native_cctools native_libtapi
endif