#pragma once

namespace rdln
{
  class readline_buffer;

  class suspend_readline
  {
#ifdef HAVE_READLINE
  public:
    suspend_readline();
    ~suspend_readline();
  private:
    readline_buffer* m_buffer;
    bool m_restart;
#endif
  };
}
