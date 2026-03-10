#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

// Input simulation for VRChat fishing.
// Supports both foreground (SendInput) and background (PostMessage) modes.
// All functions use the global `params` and `config` from vrc-fish.

void activateGameWindowInner(bool forceCursorCenter);
void activateGameWindow();
LPARAM getClientCenterLParam();
void sendMouseLeftRaw(DWORD flags, const char* phaseTag);
void mouseLeftDown();
void mouseLeftUp();
void mouseLeftClickCentered(int delayMs = 40);
void mouseMoveRelative(int dx, int dy, const char* phaseTag);
void keyTapVk(WORD vk, int delayMs = 30);

// OSC via VRChat OSC API (UDP 127.0.0.1:9000)
bool sendOscInt(const char* path, int value);
void oscResetAll();
void shakeHeadOSC();
void jumpOSC();
