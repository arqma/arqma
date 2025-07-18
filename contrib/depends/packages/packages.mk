packages := boost openssl zeromq sodium

ifneq ($(host_os),android)
packages += libusb hidapi
endif

ifneq ($(host_os),mingw32)
packages += ncurses readline
endif

ifeq ($(build_tests),ON)
packages += gtest
endif

linux_native_packages :=
linux_packages := eudev

ifneq ($(build_os), darwin)
darwin_native_packages := darwin_sdk native_clang native_cctools native_libtapi
endif
darwin_packages :=

android_native_packages := android_ndk
android_packages :=
