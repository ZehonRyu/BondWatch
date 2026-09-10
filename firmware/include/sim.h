#pragma once

void simBegin();
bool simSeePressed();
bool simRotPressed();
bool simHasNet();
bool simWifiOn();
bool simLteOn();
bool simLandscape();
void simSetWifi(bool on);
void simSetLte(bool on);
void simToggleLandscape();
void simSetLandscape(bool on);
void simPrintHelp();
bool simPollSerial(char *outCmd);
