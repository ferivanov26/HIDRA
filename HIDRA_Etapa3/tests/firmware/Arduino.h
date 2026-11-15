#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
inline size_t strlcpy(char* dst,const char* src,size_t size) {
    const size_t n=std::strlen(src);
    if(size) { std::memcpy(dst,src,n<size-1?n:size-1); dst[n<size-1?n:size-1]='\0'; }
    return n;
}
struct TestSerial {
    std::string input;
    void begin(int) {}
    void println(const char*) {}
    template<class... T> void printf(const char*,T...) {}
    bool available() const { return !input.empty(); }
    char read() { const char c=input.front(); input.erase(0,1); return c; }
};
extern TestSerial Serial;
inline void delay(int) {}
