package=native_libtapi
$(package)_version=664b8414f89612f2dfd35a9b679c345aa5389026
$(package)_download_path=https://github.com/tpoechtrager/apple-libtapi/archive
$(package)_download_file=$($(package)_version).tar.gz
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_sha256_hash=97e0d6fdf19e9ea28cfbc2ffb9ee62a78d5f9d3a33f236759d64c5dd228eb185
$(package)_build_subdir=build
$(package)_dependencies=native_clang

define $(package)_config_cmds
  echo -n $(build_prefix) > INSTALLPREFIX; \
  CC=$(host_prefix)/native/bin/clang CXX=$(host_prefix)/native/bin/clang++ \
  cmake -DCMAKE_INSTALL_PREFIX=$(build_prefix) \
    -DLLVM_INCLUDE_TESTS=OFF \
	-DCMAKE_BUILD_TYPE=RELEASE \
	-DTAPI_REPOSITORY_STRING="1100.0.11" \
	-DTAPI_FULL_VERSION="11.0.0" \
	-DCMAKE_CXX_FLAGS="-I $($(package)_extract_dir)/src/llvm/projects/clang/include -I $($(package)_build_dir)/projects/clang/include" \
	$($(package)_extract_dir)/src/llvm
endef

define $(package)_build_cmds
  $(MAKE) clangBasic && $(MAKE) libtapi
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install-libtapi install-tapi-headers
endef
