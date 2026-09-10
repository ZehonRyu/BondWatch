#pragma once

#include <stddef.h>
#include <stdint.h>

bool micBegin();
void micEnd();
void micClearLevel(); // zero cached RMS (call when leaving listen)
// Record ~ms of audio, return RMS loudness (0..~4000 typical).
uint16_t micListenMs(unsigned ms);
// Last RMS from micListenMs (0 if never sampled).
uint16_t micLastRms();
// True when ambient voice crosses threshold (simple VAD).
bool micVoiceDetected(uint16_t threshold = 180);
