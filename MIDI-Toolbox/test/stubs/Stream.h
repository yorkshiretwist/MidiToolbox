#pragma once
#include <cstddef>

class Stream {
  public:
    virtual int available() { return 0; }
    virtual int read() { return -1; }
    virtual int peek() { return -1; }
    virtual size_t readBytes(char *buffer, size_t length) { (void)buffer; (void)length; return 0; }
    virtual ~Stream() {}
};
