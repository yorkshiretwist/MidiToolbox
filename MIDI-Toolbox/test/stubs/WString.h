// Minimal stand-in for the Arduino/Teensy String class.
// Implements only the subset of behaviour this project relies on, with the
// same semantics as the real Teensyduino WString, so the project code can be
// syntax/type checked on a desktop compiler.
#pragma once
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>

class __FlashStringHelper;

class String {
  public:
    String() : s_() {}
    String(const char *cstr) : s_(cstr ? cstr : "") {}
    String(const std::string &str) : s_(str) {}
    String(const String &str) : s_(str.s_) {}
    String(char c) : s_(1, c) {}
    String(unsigned char v, unsigned char base = 10) { s_ = std::to_string((unsigned)v); (void)base; }
    String(int v) : s_(std::to_string(v)) {}
    String(unsigned int v) : s_(std::to_string(v)) {}
    String(long v) : s_(std::to_string(v)) {}
    String(unsigned long v) : s_(std::to_string(v)) {}
    // number in a given base (the real library's String(value, HEX)); letters are lower case
    String(unsigned long v, unsigned char base) {
      if (base == 16) {
        char text[24];
        snprintf(text, sizeof(text), "%lx", v);
        s_ = text;
      } else {
        s_ = std::to_string(v);
      }
    }
    // 64-bit integer overloads, matching recent Teensyduino WString. Needed
    // on LLP64 targets (e.g. MinGW on Windows) where size_t is
    // "unsigned long long" - without these, every "(String)someVector.size()"
    // in the project headers is an ambiguous conversion.
    String(long long v) : s_(std::to_string(v)) {}
    String(unsigned long long v) : s_(std::to_string(v)) {}
    String(float v, unsigned char decimalPlaces = 2) { s_ = std::to_string(v); (void)decimalPlaces; }
    String(double v, unsigned char decimalPlaces = 2) { s_ = std::to_string(v); (void)decimalPlaces; }
    String(const __FlashStringHelper *pgmstr) : s_(reinterpret_cast<const char *>(pgmstr)) {}

    const char *c_str() const { return s_.c_str(); }
    unsigned int length() const { return (unsigned int)s_.length(); }

    String &operator=(const String &rhs) { s_ = rhs.s_; return *this; }
    String &operator=(const char *cstr) { s_ = cstr ? cstr : ""; return *this; }

    String operator+(const String &rhs) const { return String(s_ + rhs.s_); }
    String operator+(const char *rhs) const { return String(s_ + std::string(rhs)); }
    friend String operator+(const char *lhs, const String &rhs) { return String(std::string(lhs) + rhs.s_); }

    String &operator+=(const String &rhs) { s_ += rhs.s_; return *this; }
    String &operator+=(const char *rhs) { s_ += rhs; return *this; }
    String &operator+=(char c) { s_ += c; return *this; }

    bool concat(const char *cstr) { s_ += cstr; return true; }
    bool concat(const String &s) { s_ += s.s_; return true; }
    bool concat(char c) { s_ += c; return true; }

    bool operator==(const String &rhs) const { return s_ == rhs.s_; }
    bool operator!=(const String &rhs) const { return s_ != rhs.s_; }
    bool operator==(const char *rhs) const { return s_ == rhs; }
    bool operator!=(const char *rhs) const { return s_ != rhs; }

    char operator[](unsigned int index) const { return s_[index]; }

    int compareTo(const String &s) const { return s_.compare(s.s_); }
    bool equals(const String &s) const { return s_ == s.s_; }

    String substring(unsigned int beginIndex) const {
      if (beginIndex > s_.length()) return String("");
      return String(s_.substr(beginIndex));
    }
    String substring(unsigned int beginIndex, unsigned int endIndex) const {
      if (beginIndex > s_.length()) return String("");
      if (endIndex > s_.length()) endIndex = (unsigned int)s_.length();
      if (beginIndex >= endIndex) return String("");
      return String(s_.substr(beginIndex, endIndex - beginIndex));
    }

    int indexOf(char c) const { auto p = s_.find(c); return p == std::string::npos ? -1 : (int)p; }
    int indexOf(const String &s) const { auto p = s_.find(s.s_); return p == std::string::npos ? -1 : (int)p; }

    long toInt() const { return atol(s_.c_str()); }
    float toFloat() const { return (float)atof(s_.c_str()); }

    void trim() {
      size_t a = s_.find_first_not_of(" \t\r\n");
      size_t b = s_.find_last_not_of(" \t\r\n");
      s_ = (a == std::string::npos) ? std::string("") : s_.substr(a, b - a + 1);
    }

    const std::string &std_str() const { return s_; }

  private:
    std::string s_;
};

inline String operator+(const String &lhs, int rhs) { return lhs + String(rhs); }
