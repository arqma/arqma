package=unwind
$(package)_version=1.3.1
$(package)_download_path=https://download.savannah.nongnu.org/releases/libunwind
$(package)_file_name=lib$(package)-$($(package)_version).tar.gz
$(package)_sha256_hash=43997a3939b6ccdf2f669b50fdb8a4d3205374728c2923ddc2354c65260214f8

define $(package)_config_cmds
  cp -f $(BASEDIR)/config.guess config/config.guess &&\
  cp -f $(BASEDIR)/config.sub config/config.sub &&\
  $($(package)_autoconf) --disable-shared --enable-static
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef

define $(package)_postprocess_cmds
endef
