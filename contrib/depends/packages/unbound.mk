package=unbound
$(package)_version=1.6.8
$(package)_download_path=https://github.com/malbit/unbound/archive
$(package)_file_name=$($(package)_version).tar.gz
$(package)_sha256_hash=b7550366ef47e8ff35765175744907c18444139702995550f27072530a7eae38
$(package)_dependencies=openssl expat ldns

define $(package)_set_vars
  $(package)_config_opts=--disable-shared --enable-static --without-pyunbound --prefix=$(host_prefix) --with-libexpat=$(host_prefix) --with-ssl=$(host_prefix) --with-libevent=no --without-pythonmodule --disable-flto --with-pthreads
  $(package)_config_opts_linux=--with-pic
  $(package)_config_opts_w64=--enable-static-exe --sysconfdir=/etc --prefix=$(host_prefix) --target=$(host_prefix)
  $(package)_build_opts_mingw32=LDFLAGS="$($(package)_ldflags) -lpthread"
endef

define $(package)_config_cmds
  $($(package)_autoconf) $($(package)_config_opts)
endef

define $(package)_build_cmds
  $(MAKE) $($(package)_build_opts)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef

define $(package)_postprocess_cmds
endef
