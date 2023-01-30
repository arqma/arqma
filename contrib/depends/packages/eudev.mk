package=eudev
$(package)_version=v3.2.6
$(package)_download_path=https://github.com/gentoo/eudev/archive/
$(package)_file_name=$($(package)_version).tar.gz
$(package)_sha256_hash=be4570dca76a570167bb8c3ff183a1233184e3b3c23f6c8be0f869183667558c

define $(package)_set_vars
  $(package)_config_opts=--disable-gudev --disable-introspection --disable-hwdb --disable-manpages --disable-shared
endef

define $(package)_config_cmds
  $($(package)_autoconf) AR_FLAGS=$($(package)_arflags)
endef

define $(package)_build_cmd
  $(MAKE)
endef

define $(package)_preprocess_cmds
  cd $($(package)_build_subdir); autoreconf -f -i
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef
