#pragma once

// Forward declarations for the fishing control functions (defined in vrc-fish.cpp)
void startFishing();
void stopFishing();
void reconnectWindow();
void saveConfigToIni();

// GUI panel rendering functions (called each frame from RunGuiLoop)
void RenderMainWindow();
