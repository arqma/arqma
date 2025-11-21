#include "scoped_message_writer.h"


tools::scoped_message_writer::~scoped_message_writer()
{
  if (m_flush)
  {
    m_flush = false;

    MCLOG_FILE(m_log_level, "msgwriter", m_oss.str());
    if (epee::console_color_default == m_color)
    {
      std::cout << m_oss.str();
    }
    else
    {
      [[maybe_unused]] rdln::suspend_readline pause_readline;
      set_console_color(m_color, m_bright);
      std::cout << m_oss.str();
      epee::reset_console_color();
    }
    std::cout << std::endl;
  }
}
