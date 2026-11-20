// Minimal Arduino.h stub for host-side compile-checking of the firmware.
// NOT used on the ESP32 (the real Arduino framework provides Arduino.h).
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#define HIGH 1
#define LOW 0
#define OUTPUT 1
#define INPUT 0
#define INPUT_PULLDOWN 2

static inline void pinMode(int,int){}
static inline void digitalWrite(int,int){}
static inline int  digitalRead(int){return LOW;}
static inline void delay(unsigned long){}
static inline void delayMicroseconds(unsigned long){}
static inline unsigned long millis(){return 0;}

class Print {
public:
    void begin(unsigned long){}
    template<typename... A> void printf(const char* f, A... a){ std::printf(f, a...); }
    void println(){ std::printf("\n"); }
    void println(const char* s){ std::printf("%s\n", s); }
    template<typename T> void println(T v){ std::printf("%lld\n", (long long)v); }
    void print(const char* s){ std::printf("%s", s); }
    int available(){ return 0; }
    std::string readStringUntil(char){ return ""; }
};
static Print Serial;

class String : public std::string {
public:
    String()=default;
    String(const char* s):std::string(s){}
    String(const std::string& s):std::string(s){}
    void trim(){ /* noop */ }
    bool startsWith(const char* s) const { return this->compare(0,strlen(s),s)==0; }
    String substring(int i) const { return String(this->substr(i)); }
    int toInt() const { return std::atoi(this->c_str()); }
    size_t length() const { return this->size(); }
};
