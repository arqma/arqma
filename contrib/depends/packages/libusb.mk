package=libusb
$(package)_version=1.0.22
$(package)_download_path=https://github.com/libusb/libusb/archive
$(package)_file_name=v$($(package)_version).tar.gz
$(package)_sha256_hash=3500f7b182750cd9ccf9be8b1df998f83df56a39ab264976bdb3307773e16f48

define $(package)_preprocess_cmds
  autoreconf -i
endef

define $(package)_set_vars
  $(package)_config_opts=--disable-shared
  $(package)_config_opts_linux=--with-pic --disable-udev
  $(package)_config_opts_mingw32=--disable-udev
  $(package)_config_opts_darwin=--disable-udev
endef

ifneq ($(host_os),darwin)
  define $(package)_config_cmds
    cp -f $(BASEDIR)/config.guess config.guess &&\
    cp -f $(BASEDIR)/config.sub config.sub &&\
    $($(package)_autoconf)
  endef
else
  define $(package)_config_cmds
    $($(package)_autoconf)
  endef
endif

define $(package)_build_cmd
  $(MAKE)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef

define $(package)_postprocess_cmds  cp -f lib/libusb-1.0.a lib/libusb.a
endef
