#pragma once

#include "ui.h"

struct CloudReply {
  Emotion emotion;
  char text[64];
  bool ok;
};

typedef void (*NetFaceHandler)(const char *emotion, const char *text, const char *source);

void netBegin();
void netTick();
bool netReady();
bool netWifiOk();
bool netCloudUp(); // API session healthy (ST_OK)
bool netCanTalk(); // cloud up + session ready — required before voice
bool netBound();
const char *netPairCode();
const char *netLinkLine();
void netPublishFace(const char *emotion, const char *text);
void netOnRemoteFace(NetFaceHandler cb);
bool netEnsureSession();
CloudReply netTurn(const char *text);
void netPrintStatus();
bool netUploadWav(const int16_t *pcm, size_t samples);
