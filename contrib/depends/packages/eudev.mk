package=eudev
$(package)_version=v3.2.8
$(package)_download_path=https://github.com/gentoo/eudev/archive/
$(package)_file_name=$($(package)_version).tar.gz
$(package)_sha256_hash=5648d44958c82ffdd1a247a7abd602a018de49a7cb0653bb74d93e2f1220aaa6

define $(package)_set_vars
  $(package)_config_opts=--disable-gudev --enable-introspection=no --disable-hwdb --disable-manpages --disable-shared --host=$(HOST)
endef

define $(package)_config_cmds
  $($(package)_autoconf)
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

define $(package)_postprocess_cmds
  rm lib/*.la
endef
