#pragma once
// Mock minimo da API Arduino/ESP32 para compilar e executar o firmware no PC.
// NAO faz parte do firmware: usado apenas por tests/smoke_firmware.cpp.
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <deque>
using std::isfinite;

#define HIGH 1
#define LOW 0
#define INPUT_PULLUP 2
#define OUTPUT 3
#define SERIAL_8N1 0
#define F(s) (s)

namespace mock {
extern uint32_t agora_ms;
extern int pinos[64];
extern std::string saida_serial;
extern std::deque<char> entrada_serial;
extern std::deque<uint8_t> uart2_tx;
}

inline uint32_t millis() { return mock::agora_ms; }
inline void delay(uint32_t ms) { mock::agora_ms += ms; }
inline void pinMode(uint8_t, uint8_t) {}
inline int digitalRead(uint8_t p) { return mock::pinos[p]; }
inline void digitalWrite(uint8_t p, int v) { mock::pinos[p] = v; }

class Print {
 public:
  virtual ~Print() {}
  virtual size_t write(uint8_t c) = 0;
  virtual size_t write(const uint8_t* b, size_t n) { for (size_t i = 0; i < n; ++i) write(b[i]); return n; }
  size_t print(const char* s) { return write((const uint8_t*)s, strlen(s)); }
  size_t println(const char* s = "") { size_t n = print(s); return n + print("\n"); }
  size_t printf(const char* fmt, ...) __attribute__((format(printf, 2, 3))) {
    char buf[1024]; va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof buf, fmt, ap); va_end(ap);
    return print(buf);
  }
};

class Stream : public Print {
 public:
  virtual int available() = 0;
  virtual int read() = 0;
};

class HardwareSerial : public Stream {
 public:
  explicit HardwareSerial(int n = 0) : n_(n) {}
  void begin(unsigned long, int = 0, int = -1, int = -1) {}
  size_t write(uint8_t c) override {
    if (n_ == 0) mock::saida_serial.push_back((char)c); else mock::uart2_tx.push_back(c);
    return 1;
  }
  using Print::write;
  int available() override { return n_ == 0 ? (int)mock::entrada_serial.size() : 0; }
  int read() override {
    if (n_ != 0 || mock::entrada_serial.empty()) return -1;
    char c = mock::entrada_serial.front(); mock::entrada_serial.pop_front(); return c;
  }
  void flush() {}
 private:
  int n_;
};

extern HardwareSerial Serial;
