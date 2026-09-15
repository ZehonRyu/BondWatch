#pragma once

#include <stddef.h>
#include <stdint.h>

bool micBegin();
void micEnd();
void micAbortRequest();
bool micAbortRequested();
void micClearLevel(); // zero cached RMS (call when leaving listen)
// Record ~ms of audio, return RMS loudness (0..~4000 typical).
uint16_t micListenMs(unsigned ms);
// Last RMS from micListenMs (0 if never sampled).
uint16_t micLastRms();
// True when ambient voice crosses threshold (simple VAD).
bool micVoiceDetected(uint16_t threshold = 180);
// Fill 16 kHz / 16-bit mono PCM. Returns RMS, 0 on fail.
// progress: chunkRms, samplesGot, samplesTotal — may be null.
typedef void (*MicProgress)(uint16_t chunkRms, size_t got, size_t total);
uint16_t micRecordPcm(int16_t *out, size_t samples, MicProgress progress = nullptr);
