#pragma once

#include "prefs.h"

// UTF-8 UI strings. Default language is Chinese (LANG_ZH).
inline const char *tr(const char *zh, const char *en) {
  return prefs().lang == LANG_EN ? en : zh;
}

inline bool langIsEn() {
  return prefs().lang == LANG_EN;
}
