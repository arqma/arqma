native_packages :=
packages := boost openssl unbound

ifneq ($(host_os),android)
packages += sodium zeromq libusb hidapi
endif

ifneq ($(host_os),mingw32)
packages += sodium zeromq ncurses readline
endif

linux_native_packages :=
linux_packages := sodium zeromq

ifeq ($(build_tests),ON)
packages += gtest
endif

ifneq ($(build_os), darwin)
darwin_native_packages := darwin_sdk native_cctools native_libtapi
endif
darwin_packages := sodium zeromq ncurses readline

android_native_packages := android_ndk
android_packages := ncurses readline