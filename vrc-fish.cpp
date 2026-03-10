#include<iostream>
#include<winsock2.h>
#include<ws2tcpip.h>
#include<windows.h>
#include<opencv2/highgui.hpp>
#include<opencv2/imgproc/imgproc.hpp>
#include <algorithm>
#include <cmath>
#include <thread>
#include <fstream>
#include <vector>
#include <sstream>
#include <deque>
#include <random>
#include <ShlObj.h>
#include "ini.h"
#include "gui/gui_logger.h"
#include "gs-public.h"
#include "gs-opencv.h"
#include "gs-mfc.h"
#include "gui/gui_state.h"
#include "gui/gui_main.h"
#include "gui/gui_panels.h"
#include "gui/gui_lang.h"
#include "core/types.h"
#include "engine/template_store.h"
#include "engine/matcher.h"
#include "engine/detectors.h"
#include "engine/debug_frame.h"
#include "engine/ml_model.h"
#include "infra/fs/path_utils.h"
#include "infra/win/window_api.h"
#include "infra/win/input_api.h"
#include "runtime/params.h"
#pragma comment(lib, "opencv_core460.lib")
#pragma comment(lib, "opencv_imgproc460.lib")
#pragma comment(lib, "opencv_imgcodecs460.lib")
#pragma comment(lib, "opencv_highgui460.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "ws2_32.lib")
using namespace std;
using namespace cv;

// GUI shared state
FishingStatus g_fishStatus;
FishingCommand g_fishCmd;
static std::thread g_fishThread;

#define KEY_PRESSED(VK_NONAME) ((GetAsyncKeyState(VK_NONAME) & 0x0001) ? 1:0) //如果为真，表示按下过
#define KEY_PRESSING(VK_NONAME) ((GetAsyncKeyState(VK_NONAME) & 0x8000) ? 1:0)  //如果为真，表示正处于按下状态

// g_params struct defined in runtime/params.h

// g_config struct defined in gui/gui_config_def.h
#include "gui/gui_config_def.h"

// 程序全局变量
g_params params;

// 配置参数
g_config config;

static bool isPaused() {
	return g_fishStatus.paused.load();
}

static bool isStopRequested() {
	return g_fishCmd.requestStop.load();
}

static void waitWhilePaused(int sleepMs = 1000) {
	while (isPaused() && !isStopRequested()) {
		Sleep(sleepMs);
	}
}

void saveConfigToIni(); // forward declaration

// 读取配置文件
void loadConfig() {
	ZIni ini("config.ini");
	config.is_pause = ini.getInt("common", "is_pause", 1);

	config.window_class = ini.get("vrchat_fish", "window_class", "UnityWndClass");
	config.window_title_contains = ini.get("vrchat_fish", "window_title_contains", "VRChat");
	config.force_resolution = ini.getInt("vrchat_fish", "force_resolution", 0);
	config.background_input = ini.getInt("vrchat_fish", "background_input", 1) != 0;
	config.target_width = ini.getInt("vrchat_fish", "target_width", 1280);
	config.target_height = ini.getInt("vrchat_fish", "target_height", 960);
	config.capture_interval_ms = ini.getInt("vrchat_fish", "capture_interval_ms", 20);
	config.cast_delay_ms = ini.getInt("vrchat_fish", "cast_delay_ms", 200);
	config.cast_mouse_move_dx = ini.getInt("vrchat_fish", "cast_mouse_move_dx", 20);
	config.cast_mouse_move_dy = ini.getInt("vrchat_fish", "cast_mouse_move_dy", 0);
	config.cast_mouse_move_random_range = ini.getInt("vrchat_fish", "cast_mouse_move_random_range", 5);
	config.cast_mouse_move_delay_max = ini.getInt("vrchat_fish", "cast_mouse_move_delay_max", 1000);
	config.cast_mouse_move_duration_ms = ini.getInt("vrchat_fish", "cast_mouse_move_duration_ms", 500);
	config.cast_mouse_move_step_ms = ini.getInt("vrchat_fish", "cast_mouse_move_step_ms", 30);
	config.bite_timeout_ms = ini.getInt("vrchat_fish", "bite_timeout_ms", 30000);
	config.bite_autopull = ini.getInt("vrchat_fish", "bite_autopull", 1);
	config.bite_autopull_ms = ini.getInt("vrchat_fish", "bite_autopull_ms", 18000);
	config.minigame_enter_delay_ms = ini.getInt("vrchat_fish", "minigame_enter_delay_ms", 0);
	config.minigame_verify_timeout_ms = ini.getInt("vrchat_fish", "minigame_verify_timeout_ms", 3000);
	config.recast_fail_delay_ms = ini.getInt("vrchat_fish", "recast_fail_delay_ms", 1000);
	config.cleanup_wait_before_ms = ini.getInt("vrchat_fish", "cleanup_wait_before_ms", 1500);
	config.cleanup_click_count = ini.getInt("vrchat_fish", "cleanup_click_count", 1);
	config.cleanup_click_interval_ms = ini.getInt("vrchat_fish", "cleanup_click_interval_ms", 150);
	config.cleanup_reel_key_name = ini.get("vrchat_fish", "cleanup_reel_key", "T");
	config.cleanup_reel_key = getKeyVal(config.cleanup_reel_key_name);
	int legacyLootClickDelayMs = ini.getInt("vrchat_fish", "loot_click_delay_ms", 4000);
	config.cleanup_wait_after_ms = ini.getInt("vrchat_fish", "cleanup_wait_after_ms", legacyLootClickDelayMs);
	config.bite_confirm_frames = ini.getInt("vrchat_fish", "bite_confirm_frames", 1);
	config.game_end_confirm_frames = ini.getInt("vrchat_fish", "game_end_confirm_frames", 5);
	config.bite_threshold = ini.getDouble("vrchat_fish", "bite_threshold", 0.75);
	config.minigame_threshold = ini.getDouble("vrchat_fish", "minigame_threshold", 0.7);
	config.fish_icon_threshold = ini.getDouble("vrchat_fish", "fish_icon_threshold", 0.44);
	config.slider_threshold = ini.getDouble("vrchat_fish", "slider_threshold", 0.88);
	config.control_interval_ms = ini.getInt("vrchat_fish", "control_interval_ms", 20);
	config.velocity_ema_alpha = ini.getDouble("vrchat_fish", "velocity_ema_alpha", 0.9);
	config.slider_bright_thresh = ini.getInt("vrchat_fish", "slider_bright_thresh", 200);
	config.slider_min_height = ini.getInt("vrchat_fish", "slider_min_height", 15);
	config.bb_gravity = ini.getDouble("vrchat_fish", "bb_gravity", 1.4);
	config.bb_thrust = ini.getDouble("vrchat_fish", "bb_thrust", -2.8);
	config.bb_drag = ini.getDouble("vrchat_fish", "bb_drag", 1.0);
	config.bb_sim_horizon = ini.getInt("vrchat_fish", "bb_sim_horizon", 8);
	config.bb_margin_ratio = ini.getDouble("vrchat_fish", "bb_margin_ratio", 0.25);
	config.bb_boundary_zone = ini.getDouble("vrchat_fish", "bb_boundary_zone", 80.0);
	config.bb_boundary_weight = ini.getDouble("vrchat_fish", "bb_boundary_weight", 0.5);
	config.bb_log_interval_ms = ini.getInt("vrchat_fish", "bb_log_interval_ms", 200);

	config.ml_mode = ini.getInt("vrchat_fish", "ml_mode", 0);
	config.ml_record_csv = ini.get("vrchat_fish", "ml_record_csv", "record_data.csv");
	config.ml_weights_file = ini.get("vrchat_fish", "ml_weights_file", "ml_weights.txt");

	config.resource_dir = ini.get("vrchat_fish", "resource_dir", "Resource-VRChat");
	config.tpl_bite_exclamation_bottom = ini.get("vrchat_fish", "tpl_bite_exclamation_bottom", "bite_exclamation_bottom.png");
	config.tpl_bite_exclamation_full = ini.get("vrchat_fish", "tpl_bite_exclamation_full", "bite_exclamation_full.png");
	config.tpl_minigame_bar_full = ini.get("vrchat_fish", "tpl_minigame_bar_full", "minigame_bar_full.png");
	config.tpl_fish_icon = ini.get("vrchat_fish", "tpl_fish_icon", "fish_icon.png");
	config.tpl_fish_icon_alt = ini.get("vrchat_fish", "tpl_fish_icon_alt", "fish_icon_alt.png");
	config.tpl_fish_icon_alt2 = ini.get("vrchat_fish", "tpl_fish_icon_alt2", "fish_icon_alt2.png");
	config.tpl_player_slider = ini.get("vrchat_fish", "tpl_player_slider", "player_slider.png");

	config.bite_scale_min = ini.getDouble("vrchat_fish", "bite_scale_min", 0.8);
	config.bite_scale_max = ini.getDouble("vrchat_fish", "bite_scale_max", 1.5);
	config.bite_scale_step = ini.getDouble("vrchat_fish", "bite_scale_step", 0.1);

	config.fish_scale_1 = ini.getDouble("vrchat_fish", "fish_scale_1", 0.9);
	config.fish_scale_2 = ini.getDouble("vrchat_fish", "fish_scale_2", 1.0);
	config.fish_scale_3 = ini.getDouble("vrchat_fish", "fish_scale_3", 1.2);
	config.fish_scale_4 = ini.getDouble("vrchat_fish", "fish_scale_4", 1.5);
	config.track_scale_1 = ini.getDouble("vrchat_fish", "track_scale_1", 0.9);
	config.track_scale_2 = ini.getDouble("vrchat_fish", "track_scale_2", 1.0);
	config.track_scale_3 = ini.getDouble("vrchat_fish", "track_scale_3", 1.2);
	config.track_scale_4 = ini.getDouble("vrchat_fish", "track_scale_4", 1.5);
	config.track_scale_min = ini.getDouble("vrchat_fish", "track_scale_min", 0.8);
	config.track_scale_max = ini.getDouble("vrchat_fish", "track_scale_max", 2.0);
	config.track_scale_step = ini.getDouble("vrchat_fish", "track_scale_step", 0.2);
	config.track_scale_refine_topk = ini.getInt("vrchat_fish", "track_scale_refine_topk", 0);
	config.track_scale_refine_radius = ini.getDouble("vrchat_fish", "track_scale_refine_radius", 0.04);
	config.track_scale_refine_step = ini.getDouble("vrchat_fish", "track_scale_refine_step", 0.02);
	config.track_angle_min = ini.getDouble("vrchat_fish", "track_angle_min", -2.0);
	config.track_angle_max = ini.getDouble("vrchat_fish", "track_angle_max", 2.0);
	config.track_angle_step = ini.getDouble("vrchat_fish", "track_angle_step", 0.0);
	config.track_angle_refine_topk = ini.getInt("vrchat_fish", "track_angle_refine_topk", 0);
	config.track_angle_refine_radius = ini.getDouble("vrchat_fish", "track_angle_refine_radius", 0.2);
	config.track_angle_refine_step = ini.getDouble("vrchat_fish", "track_angle_refine_step", 0.05);
	config.slider_detect_half_width = ini.getInt("vrchat_fish", "slider_detect_half_width", 4);
	config.slider_detect_merge_gap = ini.getInt("vrchat_fish", "slider_detect_merge_gap", 40);
	config.track_pad_y = ini.getInt("vrchat_fish", "track_pad_y", 30);
	config.track_lock_miss_multiplier = ini.getInt("vrchat_fish", "track_lock_miss_multiplier", 6);
	config.track_lock_miss_min_frames = ini.getInt("vrchat_fish", "track_lock_miss_min_frames", 60);
	config.miss_release_frames = ini.getInt("vrchat_fish", "miss_release_frames", 3);
	config.minigame_end_min_frames = ini.getInt("vrchat_fish", "minigame_end_min_frames", 10);
	config.slider_tpl_jump_threshold = ini.getInt("vrchat_fish", "slider_tpl_jump_threshold", 150);
	config.fish_jump_threshold = ini.getInt("vrchat_fish", "fish_jump_threshold", 80);
	config.slider_height_stable_min = ini.getInt("vrchat_fish", "slider_height_stable_min", 20);
	config.slider_velocity_cap = ini.getDouble("vrchat_fish", "slider_velocity_cap", 30.0);
	config.fish_velocity_cap = ini.getDouble("vrchat_fish", "fish_velocity_cap", 15.0);
	config.base_dt_ms = ini.getDouble("vrchat_fish", "base_dt_ms", 50.0);

	config.fish_bounce_predict = ini.getInt("vrchat_fish", "fish_bounce_predict", 1);
	config.fish_accel_alpha = ini.getDouble("vrchat_fish", "fish_accel_alpha", 0.9);
	config.fish_vel_decay = ini.getDouble("vrchat_fish", "fish_vel_decay", 0.2);
	config.fish_accel_cap = ini.getDouble("vrchat_fish", "fish_accel_cap", 10.0);

	config.reactive_override = ini.getInt("vrchat_fish", "reactive_override", 1);
	config.reactive_dev_ratio = ini.getDouble("vrchat_fish", "reactive_dev_ratio", 0.55);
	config.reactive_grow_threshold = ini.getDouble("vrchat_fish", "reactive_grow_threshold", 3.0);

	config.vr_debug = ini.getInt("vrchat_fish", "debug", 1);
	config.vr_debug_pic = ini.getInt("vrchat_fish", "debug_pic", 0);
	config.vr_debug_dir = ini.get("vrchat_fish", "debug_dir", "debug_vrchat");
	config.vr_log_file = ini.get("vrchat_fish", "vr_log_file", "data/logs/log.csv");
	config.vr_log_enabled = ini.getInt("vrchat_fish", "vr_log_enabled", 0);
	config.debug_console = ini.getInt("vrchat_fish", "debug_console", 0);
	config.osc_head_shake = ini.getInt("vrchat_fish", "osc_head_shake", 1) != 0;
	config.osc_anti_afk_mode = ini.getInt("vrchat_fish", "osc_anti_afk_mode", 1);
	config.osc_shake_duration_ms = ini.getInt("vrchat_fish", "osc_shake_duration_ms", 20);
	config.osc_shake_after_fails = ini.getInt("vrchat_fish", "osc_shake_after_fails", 0);
	config.osc_shake_post_delay_ms = ini.getInt("vrchat_fish", "osc_shake_post_delay_ms", 500);
	config.gui_preview_enabled = ini.getInt("gui", "preview_enabled", 1);
	config.gui_preview_boxes = ini.getInt("gui", "preview_boxes", 1);
	config.language = ini.get("gui", "language", "en");

	LOG_INFO("Config loaded: window_class=%s, title_contains=%s, target=%d*%d",
		config.window_class.c_str(), config.window_title_contains.c_str(),
		config.target_width, config.target_height);
	if (config.ml_mode == 1) {
		LOG_INFO("[ML] Record mode enabled, CSV: %s", config.ml_record_csv.c_str());
	} else if (config.ml_mode == 2) {
		LOG_INFO("[ML] Inference mode enabled, weights: %s", config.ml_weights_file.c_str());
	}

	// Create config.ini with defaults if it doesn't exist
	if (GetFileAttributesA("config.ini") == INVALID_FILE_ATTRIBUTES) {
		LOG_INFO("config.ini not found, creating with defaults");
		// ZIni requires an existing file to write, so create an empty seed
		FILE* fp = fopen("config.ini", "w");
		if (fp) {
			fclose(fp);
			saveConfigToIni();
		}
	}
}

// Window finding: infra/win/window_api.h
using infra::win::findWindowByClassAndTitleContains;
using infra::fs::ensureDirExists;

// Template loading: engine/template_store.h
// File path utilities: infra/fs/path_utils.h

// 窗口区域初始化
void initRect() {
	HWND hwnd = params.hwnd;
	RECT rect;
	GetClientRect(hwnd, &rect);
	int w = rect.right - rect.left, h = rect.bottom - rect.top;
	if (w <= 1 || h <= 1 || w > 9999 || h > 9999) {
		LOG_ERROR("No valid window client area detected. Please enter VRChat.");
		return;
	}
	POINT p1 = { rect.left, rect.top };
	POINT p2 = { rect.right, rect.bottom };
	ClientToScreen(hwnd, &p1);
	ClientToScreen(hwnd, &p2);
	rect.left = p1.x, rect.top = p1.y, rect.right = p2.x, rect.bottom = p2.y;
	RECT r0 = params.rect;
	if (rect.left == r0.left && rect.right == r0.right && rect.top == r0.top && rect.bottom == r0.bottom) {
		return;
	}
	params.rect = rect;
	LOG_INFO("Window captured: %d*%d (force_resolution=%d)", w, h, config.force_resolution);
	int targetW = config.target_width;
	int targetH = config.target_height;
	bool forceResolution = config.force_resolution != 0;
	if (forceResolution && (w != targetW || h != targetH)) {
		LOG_INFO("Auto-resizing to %d*%d", targetW, targetH);
		RECT windowsRect;
		GetWindowRect(hwnd, &windowsRect);
		int dx = (windowsRect.right - windowsRect.left) - (rect.right - rect.left);
		int dy = (windowsRect.bottom - windowsRect.top) - (rect.bottom - rect.top);
		int newWidth = targetW + dx, newHeight = targetH + dy;
		int newLeft = (rect.left + rect.right) / 2 - newWidth / 2;
		int newTop = (rect.top + rect.bottom) / 2 - newHeight / 2;
		MoveWindow(hwnd, newLeft, newTop, newWidth, newHeight, TRUE);
		initRect();
	}
}

// 窗口区域初始化 - 多线程
void initRectThread() {
	Sleep(2000);
	while (true) {
		waitWhilePaused();
		initRect();
		Sleep(1000);
	}
}

// 连接VRChat窗口
bool connectWindow() {
	HWND hwnd = findWindowByClassAndTitleContains(config.window_class, config.window_title_contains);
	if (!hwnd) {
		LOG_ERROR("VRChat window not found: class=%s, title_contains=%s",
			config.window_class.c_str(), config.window_title_contains.c_str());
		g_fishStatus.windowConnected.store(false);
		return false;
	}
	params.hwnd = hwnd;
	g_fishStatus.windowConnected.store(true);
	initRect();
	LOG_INFO("VRChat window connected");
	return true;
}

// Load template resources via extracted template_store module
bool loadTemplates() {
	TemplateStore store;
	if (!loadTemplateStore(store)) return false;
	params.tpl_vr_bite_excl_bottom = store.biteExclBottom;
	params.tpl_vr_bite_excl_full = store.biteExclFull;
	params.tpl_vr_minigame_bar_full = store.minigameBarFull;
	params.tpl_vr_fish_icon = store.fishIcon;
	params.tpl_vr_fish_icon_alt = store.fishIconAlt;
	params.tpl_vr_fish_icon_alt2 = store.fishIconAlt2;
	params.tpl_vr_fish_icons = store.fishIcons;
	params.tpl_vr_fish_icon_files = store.fishIconFiles;
	params.tpl_vr_player_slider = store.playerSlider;
	return true;
}

// Forward declaration
void fishVrchat();

// GUI control functions
void startFishing() {
	if (g_fishStatus.running.load()) return;

	if (!connectWindow()) return;
	if (!loadTemplates()) return;

	// Start window monitor thread
	thread thInitRect(initRectThread);
	thInitRect.detach();

	// Start fishing thread
	g_fishCmd.requestStop.store(false);
	g_fishStatus.paused.store(false);
	g_fishThread = std::thread(fishVrchat);
}

void stopFishing() {
	g_fishCmd.requestStop.store(true);
	if (g_fishThread.joinable()) {
		g_fishThread.join();
	}
	g_fishStatus.running.store(false);
}

void reconnectWindow() {
	connectWindow();
}

void saveConfigToIni() {
	ZIni ini("config.ini");
	ini.enableFastMode(); // batch all writes, flush once at end
	auto setStr = [&](const char* sec, const char* key, const std::string& val) {
		ini.set(sec, key, val.c_str());
	};
	auto setInt = [&](const char* sec, const char* key, int val) {
		ini.set(sec, key, std::to_string(val).c_str());
	};
	auto setDbl = [&](const char* sec, const char* key, double val) {
		char buf[64]; snprintf(buf, sizeof(buf), "%g", val);
		ini.set(sec, key, buf);
	};

	setInt("common", "is_pause", config.is_pause ? 1 : 0);

	setStr("vrchat_fish", "window_class", config.window_class);
	setStr("vrchat_fish", "window_title_contains", config.window_title_contains);
	setInt("vrchat_fish", "force_resolution", config.force_resolution);
	setInt("vrchat_fish", "background_input", config.background_input ? 1 : 0);
	setInt("vrchat_fish", "target_width", config.target_width);
	setInt("vrchat_fish", "target_height", config.target_height);
	setInt("vrchat_fish", "capture_interval_ms", config.capture_interval_ms);
	setInt("vrchat_fish", "cast_delay_ms", config.cast_delay_ms);
	setInt("vrchat_fish", "cast_mouse_move_dx", config.cast_mouse_move_dx);
	setInt("vrchat_fish", "cast_mouse_move_dy", config.cast_mouse_move_dy);
	setInt("vrchat_fish", "cast_mouse_move_random_range", config.cast_mouse_move_random_range);
	setInt("vrchat_fish", "cast_mouse_move_delay_max", config.cast_mouse_move_delay_max);
	setInt("vrchat_fish", "cast_mouse_move_duration_ms", config.cast_mouse_move_duration_ms);
	setInt("vrchat_fish", "cast_mouse_move_step_ms", config.cast_mouse_move_step_ms);
	setInt("vrchat_fish", "bite_timeout_ms", config.bite_timeout_ms);
	setInt("vrchat_fish", "bite_autopull", config.bite_autopull ? 1 : 0);
	setInt("vrchat_fish", "bite_autopull_ms", config.bite_autopull_ms);
	setInt("vrchat_fish", "minigame_enter_delay_ms", config.minigame_enter_delay_ms);
	setInt("vrchat_fish", "minigame_verify_timeout_ms", config.minigame_verify_timeout_ms);
	setInt("vrchat_fish", "recast_fail_delay_ms", config.recast_fail_delay_ms);
	setInt("vrchat_fish", "cleanup_wait_before_ms", config.cleanup_wait_before_ms);
	setInt("vrchat_fish", "cleanup_click_count", config.cleanup_click_count);
	setInt("vrchat_fish", "cleanup_click_interval_ms", config.cleanup_click_interval_ms);
	setStr("vrchat_fish", "cleanup_reel_key", config.cleanup_reel_key_name);
	setInt("vrchat_fish", "cleanup_wait_after_ms", config.cleanup_wait_after_ms);
	setInt("vrchat_fish", "bite_confirm_frames", config.bite_confirm_frames);
	setInt("vrchat_fish", "game_end_confirm_frames", config.game_end_confirm_frames);
	setDbl("vrchat_fish", "bite_threshold", config.bite_threshold);
	setDbl("vrchat_fish", "bite_scale_min", config.bite_scale_min);
	setDbl("vrchat_fish", "bite_scale_max", config.bite_scale_max);
	setDbl("vrchat_fish", "bite_scale_step", config.bite_scale_step);
	setDbl("vrchat_fish", "minigame_threshold", config.minigame_threshold);
	setDbl("vrchat_fish", "fish_icon_threshold", config.fish_icon_threshold);
	setDbl("vrchat_fish", "slider_threshold", config.slider_threshold);
	setInt("vrchat_fish", "control_interval_ms", config.control_interval_ms);
	setDbl("vrchat_fish", "velocity_ema_alpha", config.velocity_ema_alpha);
	setInt("vrchat_fish", "slider_bright_thresh", config.slider_bright_thresh);
	setInt("vrchat_fish", "slider_min_height", config.slider_min_height);
	setDbl("vrchat_fish", "bb_gravity", config.bb_gravity);
	setDbl("vrchat_fish", "bb_thrust", config.bb_thrust);
	setDbl("vrchat_fish", "bb_drag", config.bb_drag);
	setInt("vrchat_fish", "bb_sim_horizon", config.bb_sim_horizon);
	setDbl("vrchat_fish", "bb_margin_ratio", config.bb_margin_ratio);
	setDbl("vrchat_fish", "bb_boundary_zone", config.bb_boundary_zone);
	setDbl("vrchat_fish", "bb_boundary_weight", config.bb_boundary_weight);
	setInt("vrchat_fish", "bb_log_interval_ms", config.bb_log_interval_ms);
	setInt("vrchat_fish", "ml_mode", config.ml_mode);
	setStr("vrchat_fish", "ml_record_csv", config.ml_record_csv);
	setStr("vrchat_fish", "ml_weights_file", config.ml_weights_file);
	setStr("vrchat_fish", "resource_dir", config.resource_dir);
	setDbl("vrchat_fish", "track_scale_min", config.track_scale_min);
	setDbl("vrchat_fish", "track_scale_max", config.track_scale_max);
	setDbl("vrchat_fish", "track_scale_step", config.track_scale_step);
	setInt("vrchat_fish", "track_scale_refine_topk", config.track_scale_refine_topk);
	setDbl("vrchat_fish", "track_scale_refine_radius", config.track_scale_refine_radius);
	setDbl("vrchat_fish", "track_scale_refine_step", config.track_scale_refine_step);
	setDbl("vrchat_fish", "track_angle_min", config.track_angle_min);
	setDbl("vrchat_fish", "track_angle_max", config.track_angle_max);
	setDbl("vrchat_fish", "track_angle_step", config.track_angle_step);
	setInt("vrchat_fish", "track_angle_refine_topk", config.track_angle_refine_topk);
	setDbl("vrchat_fish", "track_angle_refine_radius", config.track_angle_refine_radius);
	setDbl("vrchat_fish", "track_angle_refine_step", config.track_angle_refine_step);
	setInt("vrchat_fish", "slider_detect_half_width", config.slider_detect_half_width);
	setInt("vrchat_fish", "slider_detect_merge_gap", config.slider_detect_merge_gap);
	setInt("vrchat_fish", "track_pad_y", config.track_pad_y);
	setInt("vrchat_fish", "track_lock_miss_multiplier", config.track_lock_miss_multiplier);
	setInt("vrchat_fish", "track_lock_miss_min_frames", config.track_lock_miss_min_frames);
	setInt("vrchat_fish", "miss_release_frames", config.miss_release_frames);
	setInt("vrchat_fish", "minigame_end_min_frames", config.minigame_end_min_frames);
	setInt("vrchat_fish", "slider_tpl_jump_threshold", config.slider_tpl_jump_threshold);
	setInt("vrchat_fish", "fish_jump_threshold", config.fish_jump_threshold);
	setInt("vrchat_fish", "slider_height_stable_min", config.slider_height_stable_min);
	setDbl("vrchat_fish", "slider_velocity_cap", config.slider_velocity_cap);
	setDbl("vrchat_fish", "fish_velocity_cap", config.fish_velocity_cap);
	setDbl("vrchat_fish", "base_dt_ms", config.base_dt_ms);
	setInt("vrchat_fish", "fish_bounce_predict", config.fish_bounce_predict);
	setDbl("vrchat_fish", "fish_accel_alpha", config.fish_accel_alpha);
	setDbl("vrchat_fish", "fish_vel_decay", config.fish_vel_decay);
	setDbl("vrchat_fish", "fish_accel_cap", config.fish_accel_cap);
	setInt("vrchat_fish", "reactive_override", config.reactive_override);
	setDbl("vrchat_fish", "reactive_dev_ratio", config.reactive_dev_ratio);
	setDbl("vrchat_fish", "reactive_grow_threshold", config.reactive_grow_threshold);
	setInt("vrchat_fish", "debug", config.vr_debug ? 1 : 0);
	setInt("vrchat_fish", "debug_pic", config.vr_debug_pic ? 1 : 0);
	setStr("vrchat_fish", "debug_dir", config.vr_debug_dir);
	setStr("vrchat_fish", "vr_log_file", config.vr_log_file);
	setInt("vrchat_fish", "vr_log_enabled", config.vr_log_enabled ? 1 : 0);
	setInt("vrchat_fish", "debug_console", config.debug_console ? 1 : 0);
	setInt("vrchat_fish", "osc_head_shake", config.osc_head_shake ? 1 : 0);
	setInt("vrchat_fish", "osc_anti_afk_mode", config.osc_anti_afk_mode);
	setInt("vrchat_fish", "osc_shake_duration_ms", config.osc_shake_duration_ms);
	setInt("vrchat_fish", "osc_shake_after_fails", config.osc_shake_after_fails);
	setInt("vrchat_fish", "osc_shake_post_delay_ms", config.osc_shake_post_delay_ms);
	setInt("gui", "preview_enabled", config.gui_preview_enabled ? 1 : 0);
	setInt("gui", "preview_boxes", config.gui_preview_boxes ? 1 : 0);
	setStr("gui", "language", config.language);

	ini.update();
	LOG_INFO("Config saved to config.ini");
}

// TplMatch struct defined in core/types.h

// Template matching: engine/matcher.h

// Input functions: infra/win/input_api.h

// Debug frame saving: engine/debug_frame.h
// Path utilities: infra/fs/path_utils.h

// VrFishState enum defined in core/types.h

static unsigned long long nowMs() {
	return GetTickCount64();
}

// Detection functions: engine/detectors.h
// FishSliderResult struct defined in core/types.h

// ML model: engine/ml_model.h

void fishVrchat() {
	g_fishStatus.running.store(true);
	g_fishStatus.state.store(0);
	VrFishState state = VrFishState::Cast;
	unsigned long long stateStart = nowMs();
	int biteOkFrames = 0;
	int minigameMissingFrames = 0;
	bool holding = false;
	bool castMouseMoved = false;
	int castMouseMoveDx = 0;
	int castMouseMoveDy = 0;
	unsigned long long lastCtrlLogMs = 0;
	int prevSliderY = 0;
	bool hasPrevSlider = false;
	unsigned long long lastMovementMs = 0;  // last time slider or fish moved
	double smoothVelocity = 0.0;   // EMA 平滑速度
	int prevFishY = 0;             // 上一帧鱼 Y（用于 fishVel）
	bool hasPrevFish = false;
	double smoothFishVel = 0.0;    // EMA 平滑鱼速度（px/基准帧）
	double prevSmoothFishVel = 0.0; // 上一帧的 smoothFishVel（用于算加速度）
	double smoothFishAccel = 0.0;  // EMA 平滑鱼加速度（px/基准帧^2）
	double prevDeviation = 0.0;    // 上一帧鱼与滑块中心偏差（用于方向D）
	bool hasPrevDeviation = false; // 是否有上一帧偏差
	unsigned long long prevCtrlTs = 0; // 上一帧时间戳（ms）
	bool hasPrevTs = false;
	double lastDtRatio = 1.0;      // 实际dt / 基准dt，用于MPC缩放
	double baseDtMs = config.base_dt_ms;
	if (baseDtMs < 1.0) baseDtMs = 1.0;
	int lastGoodSliderH = 0;       // 上次可信的滑块高度（用于 sH 异常时兜底）
	int lastGoodSliderCY = 0;      // 上次可信的滑块中心Y（用于 [tpl] 跳变检查）
	bool hasLastGoodPos = false;   // 是否有上次可信位置
	int consecutiveMiss = 0;       // 连续 MISS 帧数（用于 MISS 期间松开鼠标）
	int consecutiveFails = 0;      // consecutive failed casts (for OSC shake trigger)
	Rect fixedTrackRoi{};          // 首帧定位的固定轨道 ROI（整局不变）
	bool hasFixedTrack = false;    // 是否已定位轨道

			// 快速检测缓存：首帧多尺度确定最佳缩放和模板，后续帧复用
			double cachedTrackScale = 1.0;
			double cachedTrackAngle = 0.0;
			int    cachedFishTplIdx = 0;     // params.tpl_vr_fish_icons 的索引
			bool   hasCachedFishTpl = false;

		// ML 录制模式状态
		std::ofstream recordFile;
		int recordFrame = 0;
		std::deque<int> pressWindow;   // 滑动窗口：最近 N 帧 mousePressed (0/1)
		static const int PRESS_WINDOW_SIZE = 10;

		// VRChat 日志输出文件（用于拟合物理参数，避免手动复制控制台输出）
		std::ofstream vrLogFile;

		// ML 推理模式：加载权重
		if (config.ml_mode == 2 && !g_mlpModel.loaded) {
			if (!loadMlpWeights(config.ml_weights_file, g_mlpModel)) {
				LOG_ERROR("[ML] Cannot load weights, falling back to PD mode");
				config.ml_mode = 0;
			}
		}

		auto writeVrLogLine = [&](const std::string& line, bool alsoStdout = true) {
			if (alsoStdout) {
				LOG_DEBUG("%s", line.c_str());
			}
			if (vrLogFile.is_open()) {
				vrLogFile << line << '\n';
			}
		};

			if (config.vr_log_enabled && !config.vr_log_file.empty()) {
				// Make path absolute if relative
				std::string logPath = config.vr_log_file;
				if (logPath.size() < 2 || (logPath[1] != ':' && logPath[0] != '\\')) {
					char cwd[MAX_PATH];
					if (GetCurrentDirectoryA(MAX_PATH, cwd)) {
						logPath = std::string(cwd) + "\\" + logPath;
					}
				}
				std::replace(logPath.begin(), logPath.end(), '/', '\\');
				// Ensure directory exists
				size_t lastSep = logPath.find_last_of('\\');
				if (lastSep != std::string::npos && lastSep > 0) {
					ensureDirExists(logPath.substr(0, lastSep));
				}
				vrLogFile.open(logPath, std::ios::out | std::ios::app);
				if (!vrLogFile.is_open()) {
					LOG_WARN("[vrchat_fish] Failed to open vr_log_file=%s", logPath.c_str());
				}
				else {
					writeVrLogLine("[vrchat_fish] log start file=" + logPath, config.vr_debug);
				}
			}

		auto switchState = [&](VrFishState next) {
			if (config.vr_debug || vrLogFile.is_open()) {
				std::ostringstream oss;
				oss << "[vrchat_fish] state " << (int)state << " -> " << (int)next;
				writeVrLogLine(oss.str(), config.vr_debug);
			}
			state = next;
			stateStart = nowMs();
		};

	auto sleepWithPause = [&](int totalMs) {
		if (totalMs <= 0) {
			return;
		}
		int remaining = totalMs;
		while (remaining > 0) {
			if (isStopRequested()) return;
			while (isPaused() && !isStopRequested()) {
				if (holding) {
					mouseLeftUp();
					holding = false;
				}
				Sleep(100);
			}
			if (isStopRequested()) return;
			int chunk = remaining > 50 ? 50 : remaining;
			Sleep(chunk);
			remaining -= chunk;
		}
	};

		auto cleanupToNextRound = [&](const std::string& tag) {
			if (holding) {
				mouseLeftUp();
				holding = false;
			}
			if (config.vr_debug || vrLogFile.is_open()) {
				std::ostringstream oss;
				oss << "[vrchat_fish] cleanup tag=" << tag
					<< " wait_before=" << config.cleanup_wait_before_ms
					<< " clicks=" << config.cleanup_click_count
					<< " click_interval=" << config.cleanup_click_interval_ms
					<< " reel_key=" << config.cleanup_reel_key_name
					<< " wait_after=" << config.cleanup_wait_after_ms;
				writeVrLogLine(oss.str(), config.vr_debug);
			}

			sleepWithPause(config.cleanup_wait_before_ms);

		int clicks = config.cleanup_click_count;
		if (clicks < 0) clicks = 0;
		int intervalMs = config.cleanup_click_interval_ms;
		if (intervalMs < 0) intervalMs = 0;
		for (int i = 0; i < clicks; i++) {
			mouseLeftClickCentered();
			if (intervalMs > 0 && i + 1 < clicks) {
				sleepWithPause(intervalMs);
			}
		}

		if (config.cleanup_reel_key > 0) {
			keyTapVk((WORD)config.cleanup_reel_key);
		}

		sleepWithPause(config.cleanup_wait_after_ms);

			// 一轮流程结束后，将鼠标移动回去（与抛竿后的偏移相反）
			if (castMouseMoved) {
				mouseMoveRelative(-castMouseMoveDx, -castMouseMoveDy, "cast_mouse_restore");
				if (config.vr_debug || vrLogFile.is_open()) {
					std::ostringstream oss;
					oss << "[vrchat_fish] cast mouse restore dx=" << -castMouseMoveDx
						<< " dy=" << -castMouseMoveDy;
					writeVrLogLine(oss.str(), config.vr_debug);
				}
				castMouseMoved = false;
				castMouseMoveDx = 0;
				castMouseMoveDy = 0;
			}
	};

	while (true) {
		// Check stop command
		if (isStopRequested()) {
			if (holding) { mouseLeftUp(); holding = false; }
			break;
		}

		// Check pause/resume commands
		if (g_fishCmd.requestPause.load()) {
			g_fishCmd.requestPause.store(false);
			g_fishStatus.paused.store(true);
		}
		if (g_fishCmd.requestResume.load()) {
			g_fishCmd.requestResume.store(false);
			g_fishStatus.paused.store(false);
		}

		while (isPaused() && !isStopRequested()) {
			if (holding) {
				mouseLeftUp();
				holding = false;
			}
			Sleep(100);
		}
		if (isStopRequested()) break;

		// Update GUI state
		g_fishStatus.state.store((int)state);

		if (state == VrFishState::Cast) {
			if (holding) {
				mouseLeftUp();
				holding = false;
			}
			// 兜底：若上一轮未恢复，先恢复一次避免累积偏移
			if (castMouseMoved) {
				mouseMoveRelative(-castMouseMoveDx, -castMouseMoveDy, "cast_mouse_restore_before_cast");
				castMouseMoved = false;
				castMouseMoveDx = 0;
				castMouseMoveDy = 0;
			}
			if (config.osc_head_shake && consecutiveFails >= config.osc_shake_after_fails) {
				if (config.osc_anti_afk_mode == 0) {
					jumpOSC();
					if (config.vr_debug) {
						LOG_DEBUG("[vrchat_fish] OSC jump anti-AFK (fails=%d)", consecutiveFails);
					}
				} else {
					shakeHeadOSC();
					if (config.vr_debug) {
						LOG_DEBUG("[vrchat_fish] OSC head shake (fails=%d, duration=%dms)",
							consecutiveFails, config.osc_shake_duration_ms);
					}
				}
				if (config.osc_shake_post_delay_ms > 0) {
					sleepWithPause(config.osc_shake_post_delay_ms);
				}
			}
			mouseLeftClickCentered();

			if (config.cast_mouse_move_dx != 0 || config.cast_mouse_move_dy != 0) {
				// 1. Generate randomized final displacement
				static std::random_device rd;
				static std::mt19937 gen(rd());
				int range = config.cast_mouse_move_random_range;
				if (range < 0) {
					range = 0;
				}
				std::uniform_int_distribution<> dist(-range, range);
				int finalDx = config.cast_mouse_move_dx + dist(gen);
				int finalDy = config.cast_mouse_move_dy + dist(gen);

				// 2. Random delay before move
				if (config.cast_mouse_move_delay_max > 0) {
					std::uniform_int_distribution<> delayDist(0, config.cast_mouse_move_delay_max);
					int delayMs = delayDist(gen);
					sleepWithPause(delayMs);
				}

				// 3. Smooth movement (if duration configured)
				int durationMs = config.cast_mouse_move_duration_ms;
				int stepMs = config.cast_mouse_move_step_ms;
				if (durationMs > 0 && stepMs > 0) {
					int steps = (durationMs + stepMs - 1) / stepMs;
					if (steps < 1) steps = 1;

					int remainingDx = finalDx;
					int remainingDy = finalDy;

					for (int i = 0; i < steps; i++) {
						int stepDx, stepDy;
						if (i == steps - 1) {
							stepDx = remainingDx;
							stepDy = remainingDy;
						}
						else {
							double ratio = (double)stepMs / durationMs;
							stepDx = (int)(finalDx * ratio);
							stepDy = (int)(finalDy * ratio);
						}

						if (stepDx != 0 || stepDy != 0) {
							mouseMoveRelative(stepDx, stepDy, "cast_mouse_move_step");
						}

						remainingDx -= stepDx;
						remainingDy -= stepDy;

						if (i < steps - 1) {
							sleepWithPause(stepMs);
						}
					}

					castMouseMoved = true;
					castMouseMoveDx = finalDx;
					castMouseMoveDy = finalDy;
				}
				else {
					// Instant move (original logic)
					mouseMoveRelative(finalDx, finalDy, "cast_mouse_move");
					castMouseMoved = true;
					castMouseMoveDx = finalDx;
					castMouseMoveDy = finalDy;
				}

				// 4. Log output
				if (config.vr_debug || vrLogFile.is_open()) {
					std::ostringstream oss;
					oss << "[vrchat_fish] cast mouse move dx=" << finalDx
						<< " dy=" << finalDy
						<< " (base: " << config.cast_mouse_move_dx
						<< "," << config.cast_mouse_move_dy
						<< " range=" << range
						<< " delay=" << config.cast_mouse_move_delay_max << "ms";
					if (durationMs > 0) {
						oss << " smooth: dur=" << durationMs << "ms step=" << stepMs << "ms";
					}
					oss << ")";
					writeVrLogLine(oss.str(), config.vr_debug);
				}
			}
			Sleep(config.cast_delay_ms);
			biteOkFrames = 0;
			switchState(VrFishState::WaitBite);
			continue;
		}

		// After bite/autopull click: verify minigame UI actually appeared
		if (state == VrFishState::EnterMinigame) {
			// Wait for initial delay
			if (nowMs() - stateStart < (unsigned long long)config.minigame_enter_delay_ms) {
				Sleep(config.capture_interval_ms);
				continue;
			}
			// No separate verification — ControlMinigame handles it via track lock timeout
			minigameMissingFrames = 0;
			hasPrevSlider = false;
			hasPrevFish = false;
				smoothVelocity = 0.0;
				smoothFishVel = 0.0;
				hasFixedTrack = false;
				cachedTrackScale = 1.0;
				cachedTrackAngle = 0.0;
				hasCachedFishTpl = false;
				hasPrevTs = false;
				lastDtRatio = 1.0;
				lastGoodSliderH = 0;
				lastGoodSliderCY = 0;
			hasLastGoodPos = false;
			consecutiveMiss = 0;

			if (config.ml_mode == 1) {
				// 录制模式：打开 CSV
				recordFrame = 0;
				pressWindow.clear();
				if (!recordFile.is_open()) {
					recordFile.open(config.ml_record_csv, std::ios::app);
					// 如果文件为空，写表头
					recordFile.seekp(0, std::ios::end);
					if (recordFile.tellp() == 0) {
						recordFile << "frame,timestamp_ms,fishY,sliderY,dy,sliderVel,fishVel,sliderY_norm,mousePressed,duty_label" << endl;
					}
				}
				LOG_INFO("[ML] Record mode: started, manually control slider");
			}
			lastMovementMs = 0;
			switchState(VrFishState::ControlMinigame);
			continue;
		}

		Mat frame = getSrc(params.hwnd, params.rect);
		// Update preview frame for GUI (will be overwritten with annotated version during ControlMinigame)
		if (config.gui_preview_enabled) {
			Mat preview = frame.clone();
			if (config.gui_preview_boxes) {
				int cx = preview.cols / 2;
				int cy = preview.rows / 2;
				// Semi-transparent crosshair at click center (white, alpha blended)
				Mat overlay = preview.clone();
				int arm = 12;
				line(overlay, Point(cx - arm, cy), Point(cx + arm, cy), Scalar(255, 255, 255), 1);
				line(overlay, Point(cx, cy - arm), Point(cx, cy + arm), Scalar(255, 255, 255), 1);
				// If mouse offset active, show offset position (yellow)
				if (castMouseMoved) {
					int ox = cx + castMouseMoveDx;
					int oy = cy + castMouseMoveDy;
					line(overlay, Point(ox - arm, oy), Point(ox + arm, oy), Scalar(0, 255, 255), 1);
					line(overlay, Point(ox, oy - arm), Point(ox, oy + arm), Scalar(0, 255, 255), 1);
				}
				addWeighted(overlay, 0.4, preview, 0.6, 0, preview);
			}
			std::lock_guard<std::mutex> lock(g_fishStatus.frameMutex);
			g_fishStatus.latestFrame = preview;
			g_fishStatus.frameUpdated = true;
		}
		Mat gray;
		cvtColor(frame, gray, COLOR_BGR2GRAY);

		// Lambda to update preview with detection boxes
		auto updatePreviewWithBoxes = [&](const Rect& trackRoi, const FishSliderResult& det, bool detOk) {
			if (!config.gui_preview_enabled) return;
			Mat preview = frame.clone();
			if (config.gui_preview_boxes) {
				// Draw track ROI (blue)
				if (trackRoi.width > 0 && trackRoi.height > 0) {
					rectangle(preview, trackRoi, Scalar(255, 150, 0), 2);
				}
				if (detOk) {
					// Draw fish icon position (green circle)
					circle(preview, Point(det.fishX, det.fishY), 8, Scalar(0, 255, 0), 2);
					// Draw slider bounds (red rectangle)
					if (det.hasBounds) {
						Rect sliderBox(det.sliderCenterX - 10, det.sliderTop, 20, det.sliderBottom - det.sliderTop);
						rectangle(preview, sliderBox, Scalar(0, 0, 255), 2);
					} else {
						// No color bounds, draw slider center as circle
						circle(preview, Point(det.sliderCenterX, det.sliderCenterY), 6, Scalar(0, 128, 255), 2);
					}
				}
			}
			std::lock_guard<std::mutex> lock(g_fishStatus.frameMutex);
			g_fishStatus.latestFrame = preview;
			g_fishStatus.frameUpdated = true;
		};

		if (state == VrFishState::WaitBite) {
			TplMatch m{};
			bool ok = detectBite(gray, &m);
			// Update preview with bite detection box
			if (config.gui_preview_enabled && config.gui_preview_boxes && m.rect.width > 0 && m.rect.height > 0) {
				Mat preview = frame.clone();
				Scalar color = ok ? Scalar(0, 255, 0) : Scalar(0, 128, 255);
				rectangle(preview, m.rect, color, 2);
				std::lock_guard<std::mutex> lock(g_fishStatus.frameMutex);
				g_fishStatus.latestFrame = preview;
				g_fishStatus.frameUpdated = true;
			}
			if (config.vr_debug) {
				LOG_DEBUG("[vrchat_fish] bite score=%.3f ok=%d", m.score, ok ? 1 : 0);
			}
			biteOkFrames = ok ? (biteOkFrames + 1) : 0;
			if (biteOkFrames >= config.bite_confirm_frames) {
				saveDebugFrame(frame, "bite", m.rect);
				mouseLeftClickCentered();
				hasPrevSlider = false;
				hasFixedTrack = false;
				switchState(VrFishState::EnterMinigame);
				continue;
			}
			unsigned long long elapsed = nowMs() - stateStart;
			// Auto-pull fallback: click to enter minigame after wait time
			if (config.bite_autopull && elapsed > (unsigned long long)config.bite_autopull_ms) {
				LOG_INFO("[vrchat_fish] autopull after %dms -> entering minigame", config.bite_autopull_ms);
				saveDebugFrame(frame, "bite_autopull");
				mouseLeftClickCentered();
				hasPrevSlider = false;
				hasFixedTrack = false;
				switchState(VrFishState::EnterMinigame);
				continue;
			}
			// Normal timeout: give up and recast
			if (elapsed > (unsigned long long)config.bite_timeout_ms) {
				if (config.vr_debug) {
					LOG_INFO("[vrchat_fish] bite timeout -> recast");
				}
				saveDebugFrame(frame, "bite_timeout");
				cleanupToNextRound("bite_timeout");
				consecutiveFails++;
				sleepWithPause(config.recast_fail_delay_ms);
				switchState(VrFishState::Cast);
				continue;
			}
			Sleep(config.capture_interval_ms);
			continue;
		}

		if (state == VrFishState::ControlMinigame) {
			unsigned long long loopStart = nowMs();
			auto sleepControlInterval = [&]() {
				int intervalMs = config.control_interval_ms;
				if (intervalMs < 1) intervalMs = 1;
				unsigned long long elapsedMs = nowMs() - loopStart;
				if (elapsedMs < (unsigned long long)intervalMs) {
					Sleep((DWORD)((unsigned long long)intervalMs - elapsedMs));
				}
				else {
					Sleep(1);
				}
			};

				Rect searchRoi = centerThirdStripRoi(gray.size());
				// ── 首帧：用 minigame_bar_full 模板定位完整轨道，取右半部分作为滑块/鱼检测 ROI ──
				if (!hasFixedTrack) {
					double barScale = 1.0;
					double barAngle = 0.0;
					TplMatch barMatch = matchBestRoiTrackBarAutoScale(
						gray,
						params.tpl_vr_minigame_bar_full,
						searchRoi,
						TM_CCOEFF_NORMED,
						&barScale,
						&barAngle
					);
					if (barMatch.score >= config.minigame_threshold) {
						// 轨道定位成功：取匹配区域的右半部分作为滑块和鱼的检测区域
						int trackX = barMatch.rect.x;
						int trackW = barMatch.rect.width;
						int trackY = barMatch.rect.y;
						int trackH = barMatch.rect.height;
						// 右半区域起点 = 匹配区域中点，宽度 = 一半宽度
						int halfX = trackX + trackW / 2;
						int halfW = trackW - trackW / 2;
						// 垂直方向多扩展一些（鱼可能超出轨道模板范围）
						int padY = config.track_pad_y;
						if (padY < 0) padY = 0;
						fixedTrackRoi = Rect(
							halfX,
							trackY - padY,
							halfW,
							trackH + padY * 2
						);
						fixedTrackRoi = clampRect(fixedTrackRoi, gray.size());
						hasFixedTrack = true;
						cachedTrackScale = barScale;
						cachedTrackAngle = barAngle;
						// debug: 蓝框=搜索区域, 绿框=模板匹配位置, 红框=最终锁定ROI
						saveDebugFrame(frame, "track_lock", searchRoi, barMatch.rect, fixedTrackRoi);
						if (config.vr_debug || vrLogFile.is_open()) {
							std::ostringstream oss;
							oss << "[ctrl] track locked (full tpl): x=" << fixedTrackRoi.x
								<< " y=" << fixedTrackRoi.y
								<< " w=" << fixedTrackRoi.width
								<< " h=" << fixedTrackRoi.height
								<< " (bar score=" << barMatch.score
								<< " scale=" << barScale
								<< " angle=" << barAngle << ")";
							writeVrLogLine(oss.str(), config.vr_debug);
						}
					} else {
						// debug: 蓝框=搜索区域, 绿框=匹配到的(错误)位置
						saveDebugFrame(frame, "track_miss", searchRoi, barMatch.rect);
						minigameMissingFrames++;
						// 轨道长期定位不上（>= end_confirm_frames * N 帧）：放弃，退出小游戏
						int trackLockMaxMiss = config.game_end_confirm_frames * config.track_lock_miss_multiplier;
						if (trackLockMaxMiss < config.track_lock_miss_min_frames) trackLockMaxMiss = config.track_lock_miss_min_frames;
						if (config.vr_debug || vrLogFile.is_open()) {
							std::ostringstream oss;
							oss << "[ctrl] track detect MISS (score=" << barMatch.score
								<< " scale=" << barScale
								<< " angle=" << barAngle
								<< ") miss=" << minigameMissingFrames << "/" << trackLockMaxMiss;
							writeVrLogLine(oss.str(), config.vr_debug);
						}
						if (minigameMissingFrames >= trackLockMaxMiss) {
							if (holding) { mouseLeftUp(); holding = false; }
							saveDebugFrame(frame, "track_lock_timeout", searchRoi);
							switchState(VrFishState::PostMinigame);
						}
						// Verify: if track not locked within verify timeout, retract and recast
						if (!hasFixedTrack && config.minigame_verify_timeout_ms > 0) {
							unsigned long long elapsed = nowMs() - stateStart;
							if (elapsed >= (unsigned long long)config.minigame_verify_timeout_ms) {
								LOG_INFO("[verify_minigame] timeout %dms, no track lock -> recast", config.minigame_verify_timeout_ms);
								saveDebugFrame(frame, "verify_minigame_timeout");
								if (holding) { mouseLeftUp(); holding = false; }
								mouseLeftClickCentered(); // retract rod, then Cast will cast again
								consecutiveFails++;
								sleepWithPause(config.recast_fail_delay_ms);
								switchState(VrFishState::Cast);
							}
						}
						sleepControlInterval();
						continue;
					}
				}
	
				Rect matchRoi = fixedTrackRoi;
	
				FishSliderResult det;
				bool ok = false;
				bool didFullDetect = false;
				// 首帧：在已锁定轨道的 scale/angle 下，选择最匹配的鱼模板索引
				// 后续帧：固定 scale/angle + 单模板快速检测
				// 只在快速路径失败时才回退到全检测（可能鱼图标模板变了）
				if (!hasCachedFishTpl) {
					int bestIdx = 0;
					ok = detectFishAndSliderFull(gray, matchRoi, &det, cachedTrackScale, cachedTrackAngle, &bestIdx);
					if (ok) {
						cachedFishTplIdx = bestIdx;
						hasCachedFishTpl = true;
					}
					didFullDetect = true;
				} else {
					ok = detectFishAndSliderFast(gray, matchRoi, &det, cachedTrackScale, cachedTrackAngle, cachedFishTplIdx);
					if (!ok) {
						int bestIdx = 0;
						ok = detectFishAndSliderFull(gray, matchRoi, &det, cachedTrackScale, cachedTrackAngle, &bestIdx);
						if (ok) {
							cachedFishTplIdx = bestIdx;
						}
						didFullDetect = true;
					}
				}
	
				unsigned long long detectMs = nowMs() - loopStart;

			if (!ok) {
				// Update preview showing track ROI but no detection
				updatePreviewWithBoxes(fixedTrackRoi, det, false);
				if (config.vr_debug || vrLogFile.is_open()) {
					std::ostringstream oss;
					oss << "[ctrl] " << detectMs << "ms"
						<< (didFullDetect ? " [full]" : " [fast]")
						<< " MISS fs=" << det.fishScore
						<< " ss=" << det.sliderScore
						<< " hold=" << (holding ? 1 : 0);
					writeVrLogLine(oss.str(), config.vr_debug);
				}
				minigameMissingFrames++;
				consecutiveMiss++;
				// 连续 MISS >= N 帧才松开鼠标（避免单次全检测失败导致松手）
				int missReleaseFrames = config.miss_release_frames;
				if (missReleaseFrames < 1) missReleaseFrames = 1;
				if (consecutiveMiss >= missReleaseFrames && holding) {
					mouseLeftUp();
					holding = false;
				}
				int endFrames = config.game_end_confirm_frames;
				if (endFrames < config.minigame_end_min_frames) endFrames = config.minigame_end_min_frames;
				if (minigameMissingFrames >= endFrames) {
					if (holding) {
						mouseLeftUp();
						holding = false;
					}
					saveDebugFrame(frame, "minigame_end", fixedTrackRoi);
					switchState(VrFishState::PostMinigame);
				}
				sleepControlInterval();
				continue;
			}

			// MISS 恢复后重置速度估算（MISS 期间位置跳变，速度不可信）
			bool wasLongMiss = (consecutiveMiss >= 2);
			minigameMissingFrames = 0;
			consecutiveMiss = 0;

			// ── [tpl] 位置跳变检查：模板匹配位置不可信时用上次好值 ──
			if (!det.hasBounds && hasLastGoodPos) {
				// 颜色检测失败（[tpl]），检查 sCY 是否跳变太大
				int scyJump = abs(det.sliderCenterY - lastGoodSliderCY);
				if (scyJump > config.slider_tpl_jump_threshold) {
					// sCY 跳变过大，用上次好位置替代
					det.sliderCenterY = lastGoodSliderCY;
					det.sliderTop = lastGoodSliderCY - lastGoodSliderH / 2;
					det.sliderBottom = lastGoodSliderCY + lastGoodSliderH / 2;
					det.sliderHeight = lastGoodSliderH;
				}
			}

			// ── fishY 跳变保护：鱼移动缓慢，单帧大跳变必为检测错误 ──
			if (hasPrevFish) {
				int fishJump = abs(det.fishY - prevFishY);
				if (fishJump > config.fish_jump_threshold) {
					det.fishY = prevFishY; // 用上一帧值替代
				}
			}

			// ── 滑块高度修正：颜色检测不稳定时用历史值兜底 ──
			if (det.sliderHeight >= config.slider_height_stable_min) {
				lastGoodSliderH = det.sliderHeight;
			} else if (lastGoodSliderH > 0) {
				det.sliderHeight = lastGoodSliderH;
				det.sliderTop = det.sliderCenterY - lastGoodSliderH / 2;
				det.sliderBottom = det.sliderCenterY + lastGoodSliderH / 2;
			}

			// 记录可信位置（仅 [color] 检测且位置合理时更新）
			if (det.hasBounds) {
				lastGoodSliderCY = det.sliderCenterY;
				hasLastGoodPos = true;
			}

			// trackRoi 已被 fixedTrackRoi 取代，无需动态更新

			// Update preview with detection boxes
			updatePreviewWithBoxes(fixedTrackRoi, det, true);

			// ── 计算速度特征（时间归一化到基准帧间隔） ──
				{
					int fishY = det.fishY;
					int sliderCY = det.sliderCenterY;
					int sliderH = det.sliderHeight;

					unsigned long long t = nowMs();

					// 计算实际 dt 和时间缩放比
					double dtMs = baseDtMs;
					if (hasPrevTs && t > prevCtrlTs) {
						dtMs = (double)(t - prevCtrlTs);
						if (dtMs < 1.0) dtMs = 1.0;
						if (dtMs > 1000.0) dtMs = 1000.0; // cap at 1s
					}

					// 输出 ctrl 日志（包含 dt，便于离线拟合按真实 dt 归一化）
					if (config.vr_debug || vrLogFile.is_open()) {
						std::ostringstream oss;
						oss << "[ctrl] " << detectMs << "ms"
							<< (didFullDetect ? " [full]" : " [fast]")
							<< " dt=" << (int)dtMs << "ms"
							<< " t=" << t
							<< " fishY=" << fishY
							<< " sCY=" << sliderCY
							<< " sH=" << sliderH
							<< (det.hasBounds ? " [color]" : " [tpl]")
							<< " hold=" << (holding ? 1 : 0);
						writeVrLogLine(oss.str(), config.vr_debug);
					}

					lastDtRatio = dtMs / baseDtMs;
					prevCtrlTs = t;
					hasPrevTs = true;

				// EMA 平滑 slider 速度（归一化到 px/基准帧）
				double alpha = config.velocity_ema_alpha;
				if (alpha < 0.05) alpha = 0.05;
				if (alpha > 1.0) alpha = 1.0;

				// 位置大跳变：速度估算直接作废（避免 [tpl]↔[color] 切换导致 sv 饱和）
				if (hasPrevSlider) {
					int jumpThresh = config.slider_tpl_jump_threshold;
					if (jumpThresh < 50) jumpThresh = 50;
					if (abs(sliderCY - prevSliderY) > jumpThresh) {
						hasPrevSlider = false;
						smoothVelocity = 0.0;
					}
				}

				// MISS 恢复后或 dt 过大时：衰减速度（完全归零会让 MPC 误判）
				if (wasLongMiss || dtMs > 300.0) {
					// 按 dt 衰减：dt 越长衰减越多，但保留方向信息
					double decayFactor = 0.3; // 保留 30% 速度
					if (dtMs > 500.0) decayFactor = 0.1;
					if (dtMs > 800.0) decayFactor = 0.0;
					smoothVelocity *= decayFactor;
					smoothFishVel *= decayFactor;
					smoothFishAccel *= decayFactor;
					prevSmoothFishVel = smoothFishVel;
					hasPrevDeviation = false;
					hasPrevSlider = false;
					hasPrevFish = false;
				}

				// 原始位移归一化到基准帧间隔
				double rawV = hasPrevSlider ? (double)(sliderCY - prevSliderY) / lastDtRatio : 0.0;
				if (!hasPrevSlider) {
					smoothVelocity = 0.0;
				} else {
					smoothVelocity = alpha * rawV + (1.0 - alpha) * smoothVelocity;
					// 钳制速度到合理范围（防止检测跳变导致极端值）
					double maxVel = config.slider_velocity_cap;
					if (maxVel < 1.0) maxVel = 1.0;
					if (smoothVelocity > maxVel) smoothVelocity = maxVel;
					if (smoothVelocity < -maxVel) smoothVelocity = -maxVel;
				}

				// EMA 平滑 fish 速度（归一化到 px/基准帧）
				double rawFV = hasPrevFish ? (double)(fishY - prevFishY) / lastDtRatio : 0.0;
				if (!hasPrevFish) {
					smoothFishVel = 0.0;
				} else {
					smoothFishVel = alpha * rawFV + (1.0 - alpha) * smoothFishVel;
					double fishVelCap = config.fish_velocity_cap;
					if (fishVelCap < 1.0) fishVelCap = 1.0;
					if (smoothFishVel > fishVelCap) smoothFishVel = fishVelCap;
					if (smoothFishVel < -fishVelCap) smoothFishVel = -fishVelCap;
				}

				// ── 方向A：鱼加速度 EMA 追踪 ──
				{
					double accelAlpha = config.fish_accel_alpha;
					if (accelAlpha < 0.05) accelAlpha = 0.05;
					if (accelAlpha > 1.0) accelAlpha = 1.0;
					double rawAccel = smoothFishVel - prevSmoothFishVel;
					smoothFishAccel = accelAlpha * rawAccel + (1.0 - accelAlpha) * smoothFishAccel;
					double accelCap = config.fish_accel_cap;
					if (accelCap < 0.5) accelCap = 0.5;
					if (smoothFishAccel > accelCap) smoothFishAccel = accelCap;
					if (smoothFishAccel < -accelCap) smoothFishAccel = -accelCap;
					prevSmoothFishVel = smoothFishVel;
				}

				if (config.ml_mode == 1) {
					// ══ 录制模式：只读鼠标状态，写 CSV ══
					int dy = sliderCY - fishY;
					double sliderYNorm = (gray.rows > 0) ? (double)sliderCY / gray.rows : 0.5;
					int mousePressed = KEY_PRESSING(VK_LBUTTON) ? 1 : 0;
					pressWindow.push_back(mousePressed);
					if ((int)pressWindow.size() > PRESS_WINDOW_SIZE)
						pressWindow.pop_front();

					double dutyLabel = -1.0;
					if ((int)pressWindow.size() >= PRESS_WINDOW_SIZE) {
						int sum = 0;
						for (int v : pressWindow) sum += v;
						dutyLabel = (double)sum / PRESS_WINDOW_SIZE;
					}

					if (recordFile.is_open()) {
						recordFile << recordFrame << ","
							<< t << ","
							<< fishY << ","
							<< sliderCY << ","
							<< dy << ","
							<< smoothVelocity << ","
							<< smoothFishVel << ","
							<< sliderYNorm << ","
							<< mousePressed << ","
							<< dutyLabel << endl;
					}
					recordFrame++;

					if (config.vr_debug && (t - lastCtrlLogMs >= 500)) {
						LOG_DEBUG("[ML:record] frame=%d dy=%d sv=%d fv=%d sH=%d mouse=%d",
							recordFrame, dy, (int)smoothVelocity, (int)smoothFishVel, sliderH, mousePressed);
						lastCtrlLogMs = t;
					}
				} else {
					// ══ MPC 控制器：轨迹模拟 + 选择最优动作 ══
					//
					// 物理模型（每步）:
					//   velocity = velocity * drag + (pressing ? thrust : gravity)
					//   position += velocity
					//
					// 每帧模拟两条轨迹（按住 vs 松开），选择使鱼
					// 留在滑块安全区内时间更长的那个动作

					double gravity = config.bb_gravity;     // 正值，向下
					double thrust  = config.bb_thrust;      // 负值，向上
					double drag    = config.bb_drag;
					if (drag < 0.5) drag = 0.5;
					if (drag > 1.0) drag = 1.0;
					int horizon = config.bb_sim_horizon;
					if (horizon < 1) horizon = 1;
					if (horizon > 30) horizon = 30;

					double marginRatio = config.bb_margin_ratio;
					if (marginRatio < 0.0) marginRatio = 0.0;
					if (marginRatio > 0.45) marginRatio = 0.45;
					double margin = sliderH * marginRatio;

					// 模拟一条轨迹，返回累积代价（越小越好）
					// 代价 = sum of (鱼到安全区中心的距离 if 在区外, 0 if 在区内)
					double fishVelDecay = config.fish_vel_decay;
					if (fishVelDecay < 0.5) fishVelDecay = 0.5;
					if (fishVelDecay > 1.0) fishVelDecay = 1.0;
					bool fishBounce = (config.fish_bounce_predict != 0);

					auto simCost = [&](bool press) -> double {
						double sVel = smoothVelocity;   // 滑块当前速度
						double sCY  = (double)sliderCY; // 滑块中心 Y
						double fY   = (double)fishY;    // 鱼 Y
						double fVel = smoothFishVel;    // 鱼速度
						double fAcc = smoothFishAccel;  // 鱼加速度
						double cost = 0.0;

						// 轨道物理边界（固定ROI上下各扩展 padY 用于检测，这里扣回）
						// 滑块中心碰到物理边界即触发反弹
						double trackPadY = (double)std::max(0, config.track_pad_y);
						double trackCYMin = (double)fixedTrackRoi.y + trackPadY;
						double trackCYMax = (double)(fixedTrackRoi.y + fixedTrackRoi.height) - trackPadY;
						if (trackCYMin > trackCYMax) trackCYMin = trackCYMax = (trackCYMin + trackCYMax) / 2.0;

						for (int step = 0; step < horizon; step++) {
							// 滑块物理更新
							double accel = press ? thrust : gravity;
							sVel = sVel * drag + accel;
							sCY += sVel;

							// 轨道边界反弹：碰壁后速度反向
							if (sCY < trackCYMin) {
								sCY = trackCYMin;
								sVel = -sVel * 0.8; // 反弹，损失部分动能
							} else if (sCY > trackCYMax) {
								sCY = trackCYMax;
								sVel = -sVel * 0.8;
							}

							// ── 方向A：鱼位置更新（加速度预测 + 速度衰减 + 边界反弹） ──
							fVel += fAcc;                  // 加速度影响速度
							fVel *= fishVelDecay;          // 远期速度逐步衰减（不确定性增大）
							fY += fVel;

							// 鱼碰轨道边界反弹
							if (fishBounce) {
								if (fY < trackCYMin) {
									fY = trackCYMin;
									fVel = -fVel * 0.5;
									fAcc = 0.0; // 反弹后加速度失效
								} else if (fY > trackCYMax) {
									fY = trackCYMax;
									fVel = -fVel * 0.5;
									fAcc = 0.0;
								}
							}

							// 滑块边界
							double halfH = (double)sliderH / 2.0;
							double sTop = sCY - halfH + margin;
							double sBot = sCY + halfH - margin;

							// 代价：鱼偏离安全区的程度
							if (sTop >= sBot) {
								// 滑块太短，按中心距离算代价
								cost += abs(fY - sCY);
							} else if (fY < sTop) {
								cost += (sTop - fY);
							} else if (fY > sBot) {
								cost += (fY - sBot);
							}
							// 在安全区内 → cost += 0
						}

						// 边界减速惩罚：滑块以较高速度接近轨道边界时施加额外代价
						// 目的：让 MPC 主动选择提前制动，避免高速碰壁后剧烈反弹
						double bZone   = config.bb_boundary_zone;
						double bWeight = config.bb_boundary_weight;
						if (bZone > 0.0 && bWeight > 0.0) {
							double distTop = sCY - trackCYMin;
							double distBot = trackCYMax - sCY;
							double distMin = distTop < distBot ? distTop : distBot;
							if (distMin < bZone) {
								double penetration = (bZone - distMin) / bZone; // 0~1，越靠近惩罚越大
								cost += abs(sVel) * penetration * bWeight;
							}
						}
						return cost;
					};

					double costPress   = simCost(true);
					double costRelease = simCost(false);

					bool wantHold;
					if (abs(costPress - costRelease) < 0.5) {
						// 两个动作代价差不多 → 维持当前状态（减少振荡）
						wantHold = holding;
					} else {
						wantHold = (costPress < costRelease);
					}

					// ── 方向D：偏差快速响应 ──
					// 当鱼与滑块偏差大且在快速增长时，直接覆盖MPC决策
					bool reactiveTriggered = false;
					if (config.reactive_override) {
						double deviation = (double)(fishY - sliderCY); // 正=鱼在下方
						double absDev = abs(deviation);
						double devThreshold = (double)sliderH * config.reactive_dev_ratio;
						if (devThreshold < 10.0) devThreshold = 10.0;

						if (absDev > devThreshold && hasPrevDeviation) {
							// 偏差增长速度（归一化到基准帧）
							double devGrowth = (absDev - abs(prevDeviation)) / lastDtRatio;
							if (devGrowth > config.reactive_grow_threshold) {
								// 鱼在下方且偏差在增大 → 应该松开（让滑块往下）
								// 鱼在上方且偏差在增大 → 应该按住（让滑块往上）
								bool reactiveHold = (deviation < 0); // 鱼在上方
								if (reactiveHold != wantHold) {
									wantHold = reactiveHold;
									reactiveTriggered = true;
								}
							}
						}
						prevDeviation = deviation;
						hasPrevDeviation = true;
					}

					if (wantHold && !holding) {
						mouseLeftDown();
						holding = true;
					} else if (!wantHold && holding) {
						mouseLeftUp();
						holding = false;
					}

					if (config.vr_debug || vrLogFile.is_open()) {
						int logIntervalMs = config.bb_log_interval_ms;
						if (logIntervalMs < 0) logIntervalMs = 0;
						if (logIntervalMs == 0 || t - lastCtrlLogMs >= (unsigned long long)logIntervalMs) {
							std::ostringstream oss;
							oss << "[MPC] dt=" << (int)(lastDtRatio * baseDtMs) << "ms"
								<< " fishY=" << fishY
								<< " sCY=" << sliderCY
								<< " sH=" << sliderH
								<< " sv=" << (int)smoothVelocity
								<< " fv=" << (int)smoothFishVel
								<< " fa=" << (int)smoothFishAccel
								<< " cP=" << (int)costPress
								<< " cR=" << (int)costRelease
								<< " hold=" << (holding ? 1 : 0)
								<< (reactiveTriggered ? " [reactive]" : "");
							writeVrLogLine(oss.str(), config.vr_debug);
							lastCtrlLogMs = t;
						}
					}
					}

				// Track movement for static detection
				if (hasPrevSlider || hasPrevFish) {
					if (abs(sliderCY - prevSliderY) > 2 || abs(fishY - prevFishY) > 2) {
						lastMovementMs = nowMs();
					}
				} else {
					lastMovementMs = nowMs(); // first frame
				}

				prevSliderY = sliderCY;
				hasPrevSlider = true;
				prevFishY = fishY;
				hasPrevFish = true;

				// Static for 30s → false track lock, recast
				if (lastMovementMs > 0 && (nowMs() - lastMovementMs) >= 30000) {
					LOG_INFO("[ctrl] static detection for 30s -> recast");
					saveDebugFrame(frame, "static_timeout", fixedTrackRoi);
					if (holding) { mouseLeftUp(); holding = false; }
					mouseLeftClickCentered();
					consecutiveFails++;
					sleepWithPause(config.recast_fail_delay_ms);
					switchState(VrFishState::Cast);
				}
			}

			sleepControlInterval();
			continue;
		}

		if (state == VrFishState::PostMinigame) {
			saveDebugFrame(frame, "post_minigame");
			// 录制模式：flush CSV
			if (config.ml_mode == 1 && recordFile.is_open()) {
				recordFile.flush();
				LOG_INFO("[ML] Round recording done, %d frames written", recordFrame);
			}
			// 回合结束后不再依赖 xp.png 识别：固定等待 -> 多次点击入包 -> 按 T 强制收杆 -> 再等待 -> 下一轮
			cleanupToNextRound("post_minigame");
			consecutiveFails = 0;
			switchState(VrFishState::Cast);
			g_fishStatus.catchCount.fetch_add(1);
			continue;
		}
	}
	g_fishStatus.running.store(false);
	LOG_INFO("Fishing stopped");
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow) {
	// Initialize WinSock for OSC UDP
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);

	// Load config
	loadConfig();

	// Load language
	{
		std::string langPath = "lang/" + config.language + ".ini";
		if (!Lang::instance().load(langPath)) {
			Lang::instance().load("lang/en.ini"); // fallback
		}
		Lang::instance().currentLang = config.language;
	}

	// Optionally attach debug console
	if (config.debug_console) {
		AllocConsole();
		freopen("CONOUT$", "w", stdout);
		freopen("CONOUT$", "w", stderr);
	}

	// Initialize GUI
	if (!InitGui(hInstance, nCmdShow)) {
		MessageBoxW(nullptr, L"Failed to initialize GUI (DirectX11 required)", L"VRC FISH Error", MB_OK | MB_ICONERROR);
		return 1;
	}

	LOG_INFO("VRChat FISH! Auto-Fisher ready");
	LOG_INFO("Recommended window size: %d*%d", config.target_width, config.target_height);

	// Run GUI loop (blocks until window is closed)
	int result = RunGuiLoop();

	// Cleanup: stop fishing thread
	if (g_fishStatus.running.load()) {
		stopFishing();
	}

	CleanupGui();
	WSACleanup();
	return result;
}
