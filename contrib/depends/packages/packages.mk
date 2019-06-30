packages:=boost openssl zeromq cppzmq expat ldns cppzmq libiconv hidapi protobuf libusb
native_packages := native_ccache native_protobuf

darwin_native_packages = native_biplist native_ds_store native_mac_alias
darwin_packages = sodium-darwin unbound ncurses readline

linux_packages = eudev unbound ncurses readline unwind sodium
qt_packages = qt

ifeq ($(host_os),mingw32)
packages += icu4c
packages += sodium
endif

ifneq ($(build_os),darwin)
darwin_native_packages += native_cctools native_cdrkit native_libdmg-hfsplus
endif
