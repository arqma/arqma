#include "cryptonote_basic.h"

namespace cryptonote
{
  void transaction_prefix::set_null()
  {
    version = txversion::v1;
    unlock_time = 0;
    vin.clear();
    vout.clear();
    extra.clear();
    output_unlock_times.clear();
    tx_type = txtype::standard;
    hard_fork_version = {};
  }

  transaction::transaction(const transaction &t)
    : transaction_prefix(t)
    , hash_valid(t.is_hash_valid())
    , blob_size_valid(t.is_blob_size_valid())
    , signatures(t.signatures)
    , rct_signatures(t.rct_signatures)
    , hash(t.hash)
    , blob_size(t.blob_size)
    , pruned(t.pruned)
    , unprunable_size(t.unprunable_size.load())
    , prefix_size(t.prefix_size.load())
  {}

  transaction::transaction(transaction &&t)
    : transaction_prefix(std::move(t))
    , hash_valid(t.is_hash_valid())
    , blob_size_valid(t.is_blob_size_valid())
    , signatures(std::move(t.signatures))
    , rct_signatures(std::move(t.rct_signatures))
    , hash(std::move(t.hash))
    , blob_size(std::move(t.blob_size))
    , pruned(t.pruned)
    , unprunable_size(t.unprunable_size.load())
    , prefix_size(t.prefix_size.load())
  {
    t.set_null();
  }

  transaction& transaction::operator=(const transaction& t)
  {
    if (this == std::addressof(t))
      return *this;

    transaction_prefix::operator=(t);

    set_hash_valid(t.is_hash_valid());
    set_blob_size_valid(t.is_blob_size_valid());
    signatures = t.signatures;
    rct_signatures = t.rct_signatures;
    hash = t.hash;
    blob_size = t.blob_size;
    pruned = t.pruned;
    unprunable_size = t.unprunable_size.load();
    prefix_size = t.prefix_size.load();
    return *this;
  }

  transaction& transaction::operator=(transaction &&t)
  {
    if (this == std::addressof(t))
      return *this;

    transaction_prefix::operator=(std::move(t));

    set_hash_valid(t.is_hash_valid());
    set_blob_size_valid(t.is_blob_size_valid());
    signatures = std::move(t.signatures);
    rct_signatures = std::move(t.rct_signatures);
    hash = std::move(t.hash);
    blob_size = std::move(t.blob_size);
    pruned = std::move(t.pruned);
    unprunable_size = t.unprunable_size.load();
    prefix_size = t.prefix_size.load();

    t.set_null();
    return *this;
  }

  void transaction::set_null()
  {
    transaction_prefix::set_null();
    signatures.clear();
    rct_signatures = {};
    rct_signatures = rct::rctSig{};
    set_hash_valid(false);
    set_blob_size_valid(false);
    pruned = false;
    unprunable_size = 0;
    prefix_size = 0;
  }

  void transaction::invalidate_hashes()
  {
    set_hash_valid(false);
    set_blob_size_valid(false);
  }

  size_t transaction::get_signature_size(const txin_v& tx_in)
  {
    struct txin_signature_size_visitor : public boost::static_visitor<size_t>
    {
      size_t operator()(const txin_gen& txin) const { return 0; }
      size_t operator()(const txin_to_script& txin) const { return 0; }
      size_t operator()(const txin_to_scripthash& txin) const { return 0; }
      size_t operator()(const txin_to_key& txin) const { return txin.key_offsets.size(); }
    };

    return boost::apply_visitor(txin_signature_size_visitor(), tx_in);
  }

  block::block(const block& b)
    : block_header(b)
    , miner_tx{b.miner_tx}
    , tx_hashes{b.tx_hashes}
  {
    copy_hash(b);
  }

  block::block(block&& b)
    : block_header(std::move(b))
    , miner_tx{std::move(b.miner_tx)}
    , tx_hashes{std::move(b.tx_hashes)}
  {
    copy_hash(b);
  }

  block& block::operator=(const block& b)
  {
    if (this != std::addressof(b))
    {
      block_header::operator=(b);
      miner_tx = b.miner_tx;
      tx_hashes = b.tx_hashes;
      copy_hash(b);
    }
    return *this;
  }
  block& block::operator=(block&& b)
  {
    if (this != std::addressof(b))
    {
      block_header::operator=(std::move(b));
      miner_tx = std::move(b.miner_tx);
      tx_hashes = std::move(b.tx_hashes);
      copy_hash(b);
    }
    return *this;
  }

  bool block::is_hash_valid() const
  {
    return hash_valid.load(std::memory_order_acquire);
  }
  void block::set_hash_valid(bool v) const
  {
    hash_valid.store(v, std::memory_order_release);
  }

}
