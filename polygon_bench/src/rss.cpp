#include "rss.h"

#include <fstream>
#include <string>

namespace polybench {

double CurrentRSSMB() {
  std::ifstream f("/proc/self/status");
  std::string line;
  while (std::getline(f, line)) {
    if (line.rfind("VmRSS:", 0) == 0) {
      size_t pos = line.find_first_of("0123456789");
      if (pos == std::string::npos) return 0.0;
      const long kb = std::stol(line.substr(pos));
      return static_cast<double>(kb) / 1024.0;
    }
  }
  return 0.0;
}

}  // namespace polybench

