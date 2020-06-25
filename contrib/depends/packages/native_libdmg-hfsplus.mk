package=native_libdmg-hfsplus
$(package)_version=tarball
$(package)_download_path=https://github.com/planetbeing/libdmg-hfsplus/tarball/master/
$(package)_file_name=libdmg-hfsplus.tar.gz
$(package)_sha256_hash=6f75eedb5d81b5c0cc38111ae4386e8aba9e4bf6efcd70a1bb918f04ec1345a5
$(package)_build_subdir=build
$(package)_patches=remove-libcrypto-dependency.patch

define $(package)_preprocess_cmds
  patch -p1 < $($(package)_patch_dir)/remove-libcrypto-dependency.patch && \
  mkdir build
endef

define $(package)_config_cmds
  cmake -DCMAKE_INSTALL_PREFIX:PATH=$(build_prefix) -DCMAKE_C_FLAGS="-Wl,--build-id=none" ..
endef

define $(package)_build_cmds
  $(MAKE) -C dmg
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) -C dmg install
endef
