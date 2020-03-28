package=cppzmq
$(package)_version=4.6.0
$(package)_download_path=https://github.com/zeromq/cppzmq/archive/
$(package)_file_name=v$($(package)_version).tar.gz
$(package)_sha256_hash=e9203391a0b913576153a2ad22a2dc1479b1ec325beb6c46a3237c669aef5a52
$(package)_dependencies=zeromq

define $(package)_stage_cmds
  mkdir $($(package)_staging_prefix_dir)/include &&\
  cp zmq.hpp $($(package)_staging_prefix_dir)/include
endef

define $(package)_postprocess_cmds
  rm -rf bin share
endef


