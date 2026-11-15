#pragma once
#include <map>
#include <string>
#include <vector>
#include <cstdint>
#include <cstring>

namespace mock { extern std::map<std::string, std::vector<uint8_t>> nvs; }

class Preferences {
 public:
  bool begin(const char*, bool) { return true; }
  uint32_t getUInt(const char* k, uint32_t def) {
    auto it = mock::nvs.find(k); if (it == mock::nvs.end()) return def;
    uint32_t v; memcpy(&v, it->second.data(), 4); return v;
  }
  size_t putUInt(const char* k, uint32_t v) { mock::nvs[k].assign((uint8_t*)&v, (uint8_t*)&v + 4); return 4; }
  size_t getBytesLength(const char* k) { auto it = mock::nvs.find(k); return it == mock::nvs.end() ? 0 : it->second.size(); }
  size_t getBytes(const char* k, void* b, size_t n) {
    auto it = mock::nvs.find(k); if (it == mock::nvs.end() || it->second.size() > n) return 0;
    memcpy(b, it->second.data(), it->second.size()); return it->second.size();
  }
  size_t putBytes(const char* k, const void* b, size_t n) { mock::nvs[k].assign((const uint8_t*)b, (const uint8_t*)b + n); return n; }
  bool remove(const char* k) { return mock::nvs.erase(k) > 0; }
};
