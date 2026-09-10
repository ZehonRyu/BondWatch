#pragma once

#include "ui.h"

struct CloudReply {
  Emotion emotion;
  char text[64];
  bool ok;
};

bool netBegin();
bool netReady();
bool netEnsureSession();
CloudReply netTurn(const char *text);
void netPrintStatus();
