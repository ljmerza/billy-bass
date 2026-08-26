#pragma once
#include <cstdint>
#include <cstring>
#include <string>
static const float PI = 3.14159265358979f;
extern unsigned long g_millis;
inline unsigned long millis() { return g_millis; }
struct String {
  std::string s;
  String() {}
  String(const char* c) : s(c) {}
  String& operator+=(const char* c) { s += c; return *this; }
  String& operator+=(char c) { s += c; return *this; }
  String& operator+=(int v) { s += std::to_string(v); return *this; }
  String& operator+=(unsigned long v) { s += std::to_string(v); return *this; }
};
