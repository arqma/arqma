#include "string_util.h"
#include <sstream>

namespace tools {

using namespace std::literals;

std::string friendly_duration(std::chrono::nanoseconds dur) {
  std::ostringstream os;
  bool some = false;
  if (dur >= 24h) {
    os << dur / 24h << 'd';
    dur %= 24h;
    some = true;
  }
  if (dur >= 1h || some) {
    os << dur / 1h << 'h';
    dur %= 1h;
    some = true;
  }
  if (dur >= 1min || some) {
    os << dur / 1min << 'm';
    dur %= 1min;
    some = true;
  }
  if (some || dur == 0s) {
    os << dur / 1s << 's';
  } else {
    double seconds = std::chrono::duration<double>(dur).count();
    os.precision(3);
    if (dur >= 1s)
      os << seconds << "s";
    else if (dur >= 1ms)
      os << seconds * 1000 << "ms";
    else if (dur >= 1us)
      os << seconds * 1'000'000 << u8"µs";
    else
      os << seconds * 1'000'000'000 << "ns";
  }
  return os.str();
}

}
