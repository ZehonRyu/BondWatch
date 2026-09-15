#pragma once

#include <stdint.h>

struct LteTestResult {
  bool gotBytes;
  bool atOk;
  char id[40];
  char sim[24];
  char net[24];
  int16_t csq; // -1 = none
  char hint[80];
};

void lteBegin();
void lteSelfTest(LteTestResult *out);
