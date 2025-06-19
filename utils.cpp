#include <cctype>
#include <cstddef>
#include <string>
#include <vector>

std::string uppercase(const std::string& str) {
  std::string result(str);
  // TODO: Because of IRC's Scandinavian origin, the characters {}|^ are
  // considered to be the lower case equivalents of the characters []\~,
  // respectively. This is a critical issue when determining the
  // equivalence of two nicknames or channel names.

  for (std::string::iterator it = result.begin(); it != result.end(); ++it) {
    *it = static_cast<char>(std::toupper(*it));
  }
  return result;
}

bool startsWith(const std::string& str, const std::string& prefix) {
  return str.size() >= prefix.size() &&
         uppercase(str).compare(0, prefix.size(), prefix) == 0;
}

std::vector<std::string> split(const std::string& line) {
  std::vector<std::string> result;
  if (line.empty()) {
    return result;
  }
  std::string token;
  size_t pos = 0;
  size_t found_colon = line.find(':');

  size_t end = (found_colon == std::string::npos) ? line.length() : found_colon;

  while (pos < end) {
    size_t found_space = line.find(' ', pos);
    if (found_space == std::string::npos || found_space >= end) {
      found_space = end;
    }
    token = line.substr(pos, found_space - pos);
    if (!token.empty()) {
      result.push_back(token);
    }
    pos = found_space + 1;
  }

  if (found_colon != std::string::npos) {
    result.push_back(line.substr(found_colon));
  }
  if (!result.empty()) result[0] = uppercase(result[0]);

  return result;
}
