package=zeromq
$(package)_version=4.3.5
$(package)_download_path=https://github.com/zeromq/libzmq/releases/download/v$($(package)_version)/
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_sha256_hash=6653ef5910f17954861fe72332e68b03ca6e4d9c7160eb3a8de5a5a913bfab43
$(package)_dependencies=sodium

define $(package)_set_vars
  $(package)_config_opts=--enable-static --disable-shared --disable-curve-keygen --enable-curve --disable-drafts --disable-libunwind --with-libsodium --without-pgm --without-norm --without-vmci --without-docs --disable-Werror
  $(package)_cxxflags_linux=-std=c++17
  $(package)_cxxflags_darwin=-std=c++17
  $(package)_cxxflags_mingw32=-std=c++17 -D_FORTIFY_SOURCE=0 -lssp
  $(package)_build_opts_mingw32=LDFLAGS="$($(package)_ldflags) -lsodium -liphlpapi"
endef

define $(package)_preprocess_cmds
  cp -f $(BASEDIR)/config.guess $(BASEDIR)/config.sub config
endef

define $(package)_config_cmds
  $($(package)_autoconf) AR_FLAGS=$($(package)_arflags)
endef

define $(package)_build_cmds
  $(MAKE) $($(package)_build_opts) src/libzmq.la
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef

define $(package)_postprocess_cmds
  rm -rf bin share &&\
  rm lib/*.la
endef
