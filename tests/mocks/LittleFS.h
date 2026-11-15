#pragma once
#include "Arduino.h"
#include <map>
#include <string>

#define FILE_READ "r"
#define FILE_WRITE "w"
#define FILE_APPEND "a"

namespace mock { extern std::map<std::string, std::string> arquivos; }

class File : public Print {
 public:
  File() {}
  File(const std::string& nome, const char* modo) : nome_(nome), ok_(true) {
    if (!strcmp(modo, "w")) mock::arquivos[nome_].clear();
    else if (!strcmp(modo, "r") && !mock::arquivos.count(nome_)) ok_ = false;
    else mock::arquivos[nome_];
  }
  explicit operator bool() const { return ok_; }
  size_t write(uint8_t c) override { mock::arquivos[nome_].push_back((char)c); return 1; }
  using Print::write;
  size_t size() const { return mock::arquivos[nome_].size(); }
  int available() const { return (int)(mock::arquivos[nome_].size() - pos_); }
  size_t read(uint8_t* b, size_t n) {
    const std::string& s = mock::arquivos[nome_];
    size_t k = std::min(n, s.size() - pos_); memcpy(b, s.data() + pos_, k); pos_ += k; return k;
  }
  void close() {}
 private:
  std::string nome_; bool ok_ = false; size_t pos_ = 0;
};

class FSMock {
 public:
  bool begin(bool) { return true; }
  File open(const char* n, const char* modo) { return File(n, modo); }
  bool exists(const char* n) { return mock::arquivos.count(n) > 0; }
  bool remove(const char* n) { return mock::arquivos.erase(n) > 0; }
  bool rename(const char* a, const char* b) {
    auto it = mock::arquivos.find(a); if (it == mock::arquivos.end()) return false;
    mock::arquivos[b] = it->second; mock::arquivos.erase(a); return true;
  }
};
extern FSMock LittleFS;
