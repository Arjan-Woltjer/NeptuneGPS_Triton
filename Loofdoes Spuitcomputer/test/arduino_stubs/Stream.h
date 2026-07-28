#pragma once
#include "Print.h"

class Stream : public Print {
  public:
    virtual ~Stream() {}
    virtual int available() { return 0; }
    virtual int read()      { return -1; }
    virtual int peek()      { return -1; }
};
