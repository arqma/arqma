  #pragma once

namespace cryptonote
{
  class LockedTXN {
  public:
    LockedTXN(BlockchainDB &db): m_db(db)
    {
      m_batch = m_db.batch_start();
    }
    LockedTXN(const LockedTXN &) = delete;
    LockedTXN &operator=(const LockedTXN &) = delete;
    LockedTXN(LockedTXN &&o) : m_db{o.m_db}, m_batch{o.m_batch} { o.m_batch = false; }
    LockedTXN &operator=(LockedTXN &&) = delete;

    void commit() { try { if (m_batch) { m_db.batch_stop(); m_batch = false; } } catch (const std::exception &e) { MWARNING("LockedTXN::commit filtering exception: " << e.what()); } }
    void abort() { try { if (m_batch) { m_db.batch_abort(); m_batch = false; } } catch (const std::exception &e) { MWARNING("LockedTXN::abort filtering exception: " << e.what()); } }
    ~LockedTXN() { this->abort(); }
  private:
    BlockchainDB &m_db;
    bool m_batch;
  };
}
