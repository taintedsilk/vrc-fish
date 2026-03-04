#pragma once
#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <opencv2/core.hpp>

// Fishing state names (must match VrFishState enum order)
static const char* g_stateNames[] = {
	"Cast", "WaitBite", "EnterMinigame", "ControlMinigame", "PostMinigame"
};

// Fishing thread status visible to GUI
struct FishingStatus {
	std::atomic<bool> running{ false };
	std::atomic<bool> paused{ false };
	std::atomic<int> state{ 0 };           // VrFishState as int
	std::atomic<int> catchCount{ 0 };
	std::atomic<bool> windowConnected{ false };

	// For optional preview panel
	std::mutex frameMutex;
	cv::Mat latestFrame;
	bool frameUpdated = false;
};

// Commands from GUI to fishing thread
struct FishingCommand {
	std::atomic<bool> requestStart{ false };
	std::atomic<bool> requestStop{ false };
	std::atomic<bool> requestPause{ false };
	std::atomic<bool> requestResume{ false };
	std::atomic<bool> requestReconnect{ false };
};

// Global instances
extern FishingStatus g_fishStatus;
extern FishingCommand g_fishCmd;
