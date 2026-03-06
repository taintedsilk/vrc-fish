#define NOMINMAX
#include "gui_panels.h"
#include "gui_logger.h"
#include "gui_state.h"
#include "gui_main.h"
#include "gui_config_def.h"
#include "gui_lang.h"
#include "imgui.h"
#include <cstring>
#include <d3d11.h>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

// --- Preview texture ---
static ID3D11ShaderResourceView* g_previewSRV = nullptr;
static int g_previewW = 0, g_previewH = 0;

static void UpdatePreviewTexture() {
	auto* device = GetD3DDevice();
	if (!device) return;

	std::lock_guard<std::mutex> lock(g_fishStatus.frameMutex);
	if (!g_fishStatus.frameUpdated || g_fishStatus.latestFrame.empty()) return;
	g_fishStatus.frameUpdated = false;

	cv::Mat rgba;
	cv::cvtColor(g_fishStatus.latestFrame, rgba, cv::COLOR_BGR2RGBA);

	if (g_previewSRV) { g_previewSRV->Release(); g_previewSRV = nullptr; }

	D3D11_TEXTURE2D_DESC desc = {};
	desc.Width = rgba.cols;
	desc.Height = rgba.rows;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	D3D11_SUBRESOURCE_DATA initData = {};
	initData.pSysMem = rgba.data;
	initData.SysMemPitch = (UINT)rgba.step;

	ID3D11Texture2D* tex = nullptr;
	device->CreateTexture2D(&desc, &initData, &tex);
	if (tex) {
		device->CreateShaderResourceView(tex, nullptr, &g_previewSRV);
		g_previewW = rgba.cols;
		g_previewH = rgba.rows;
		tex->Release();
	}
}

// --- Helper: SliderInt stored as ms, displayed as seconds ---
static bool SliderMsAsSeconds(const char* label, int* ms, int minMs, int maxMs) {
	float sec = *ms / 1000.0f;
	float minSec = minMs / 1000.0f;
	float maxSec = maxMs / 1000.0f;
	if (ImGui::SliderFloat(label, &sec, minSec, maxSec, "%.1fs")) {
		*ms = (int)(sec * 1000.0f + 0.5f);
		return true;
	}
	return false;
}

// --- Helper: SliderFloat for double config values ---
static bool SliderDouble(const char* label, double* v, float vmin, float vmax, const char* fmt = "%.3f", bool asPercent = false) {
	if (asPercent) {
		float pct = (float)*v * 100.0f;
		if (ImGui::SliderFloat(label, &pct, vmin * 100.0f, vmax * 100.0f, fmt)) {
			*v = pct / 100.0;
			return true;
		}
		return false;
	}
	float f = (float)*v;
	if (ImGui::SliderFloat(label, &f, vmin, vmax, fmt)) {
		*v = f;
		return true;
	}
	return false;
}
static bool InputDouble(const char* label, double* v, float step = 0.0f, float stepFast = 0.0f, const char* fmt = "%.3f") {
	float f = (float)*v;
	if (ImGui::InputFloat(label, &f, step, stepFast, fmt)) {
		*v = f;
		return true;
	}
	return false;
}

// --- Status Bar ---
static void RenderStatusBar() {
	bool connected = g_fishStatus.windowConnected.load();
	bool running = g_fishStatus.running.load();
	bool paused = g_fishStatus.paused.load();
	int stateIdx = g_fishStatus.state.load();

	// Connection indicator
	if (connected) {
		ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "%s", T("status_connected"));
	} else {
		ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s", T("status_not_connected"));
	}

	ImGui::SameLine(220.0f);

	// Fishing state
	if (!running) {
		ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%s", T("status_stopped"));
	} else if (paused) {
		ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "%s", T("status_paused"));
	} else if (stateIdx >= 0 && stateIdx < 5) {
		ImGui::Text("%s %s", T("status_state"), g_stateNames[stateIdx]);
	} else {
		ImGui::Text("%s", T("status_unknown"));
	}

	ImGui::SameLine(460.0f);

	// Catch counter
	ImGui::Text("%s %d", T("status_catches"), g_fishStatus.catchCount.load());

	ImGui::Separator();
}

// --- Control Buttons ---
static void RenderControlButtons() {
	bool running = g_fishStatus.running.load();
	bool paused = g_fishStatus.paused.load();

	if (!running) {
		if (ImGui::Button(T("btn_start"), ImVec2(130, 28))) {
			startFishing();
		}
	} else {
		if (ImGui::Button(T("btn_stop"), ImVec2(70, 28))) {
			stopFishing();
		}
		ImGui::SameLine();
		if (paused) {
			if (ImGui::Button(T("btn_resume"), ImVec2(80, 28))) {
				g_fishCmd.requestResume.store(true);
			}
		} else {
			if (ImGui::Button(T("btn_pause"), ImVec2(80, 28))) {
				g_fishCmd.requestPause.store(true);
			}
		}
	}

	ImGui::SameLine();
	if (ImGui::Button(T("btn_connect"), ImVec2(130, 28))) {
		reconnectWindow();
	}

	ImGui::SameLine();
	if (ImGui::Button(T("btn_save"), ImVec2(110, 28))) {
		saveConfigToIni();
	}

	ImGui::SameLine();
	ImGui::TextDisabled("%s", T("hint_hotkey"));

	ImGui::Separator();
}

// --- Config Editor ---
static void RenderConfigEditor() {
	// String input buffers
	static char bufWindowClass[256] = {};
	static char bufTitleContains[256] = {};
	static char bufReelKey[64] = {};
	static char bufResourceDir[512] = {};
	static char bufMlCsv[512] = {};
	static char bufMlWeights[512] = {};
	static char bufDebugDir[512] = {};
	static char bufLogFile[512] = {};
	static bool buffersInited = false;

	if (!buffersInited) {
		strncpy(bufWindowClass, config.window_class.c_str(), sizeof(bufWindowClass) - 1);
		strncpy(bufTitleContains, config.window_title_contains.c_str(), sizeof(bufTitleContains) - 1);
		strncpy(bufReelKey, config.cleanup_reel_key_name.c_str(), sizeof(bufReelKey) - 1);
		strncpy(bufResourceDir, config.resource_dir.c_str(), sizeof(bufResourceDir) - 1);
		strncpy(bufMlCsv, config.ml_record_csv.c_str(), sizeof(bufMlCsv) - 1);
		strncpy(bufMlWeights, config.ml_weights_file.c_str(), sizeof(bufMlWeights) - 1);
		strncpy(bufDebugDir, config.vr_debug_dir.c_str(), sizeof(bufDebugDir) - 1);
		strncpy(bufLogFile, config.vr_log_file.c_str(), sizeof(bufLogFile) - 1);
		buffersInited = true;
	}

	// === Window ===
	if (ImGui::CollapsingHeader(T("sec_window"), ImGuiTreeNodeFlags_DefaultOpen)) {
		if (ImGui::InputText(T("cfg_window_class"), bufWindowClass, sizeof(bufWindowClass)))
			config.window_class = bufWindowClass;
		if (ImGui::InputText(T("cfg_title_contains"), bufTitleContains, sizeof(bufTitleContains)))
			config.window_title_contains = bufTitleContains;

		bool forceRes = config.force_resolution != 0;
		if (ImGui::Checkbox(T("cfg_force_res"), &forceRes))
			config.force_resolution = forceRes ? 1 : 0;
		ImGui::InputInt(T("cfg_target_w"), &config.target_width);
		ImGui::InputInt(T("cfg_target_h"), &config.target_height);
		ImGui::Separator();
		ImGui::Checkbox(T("cfg_background_input"), &config.background_input);
		ImGui::TextDisabled("%s", T("cfg_background_hint"));
	}

	// === Timing ===
	if (ImGui::CollapsingHeader(T("sec_timing"))) {
		SliderMsAsSeconds(T("cfg_capture_interval"), &config.capture_interval_ms, 5, 200);
		SliderMsAsSeconds(T("cfg_cast_delay"), &config.cast_delay_ms, 0, 2000);
		ImGui::InputInt(T("cfg_cast_dx"), &config.cast_mouse_move_dx);
		ImGui::InputInt(T("cfg_cast_dy"), &config.cast_mouse_move_dy);
		ImGui::InputInt(T("cfg_cast_random_range"), &config.cast_mouse_move_random_range);
		SliderMsAsSeconds(T("cfg_cast_delay_max"), &config.cast_mouse_move_delay_max, 0, 3000);
		SliderMsAsSeconds(T("cfg_cast_move_duration"), &config.cast_mouse_move_duration_ms, 0, 2000);
		SliderMsAsSeconds(T("cfg_cast_move_step"), &config.cast_mouse_move_step_ms, 5, 200);
		ImGui::Checkbox(T("cfg_osc_head_shake"), &config.osc_head_shake);
		if (config.osc_head_shake) {
			SliderMsAsSeconds(T("cfg_osc_shake_duration"), &config.osc_shake_duration_ms, 5, 200);
			ImGui::InputInt(T("cfg_osc_after_fails"), &config.osc_shake_after_fails);
			if (config.osc_shake_after_fails < 0) config.osc_shake_after_fails = 0;
			SliderMsAsSeconds(T("cfg_osc_post_delay"), &config.osc_shake_post_delay_ms, 0, 3000);
		}
		SliderMsAsSeconds(T("cfg_bite_timeout"), &config.bite_timeout_ms, 5000, 60000);
		ImGui::Checkbox(T("cfg_autopull"), &config.bite_autopull);
		if (config.bite_autopull) {
			SliderMsAsSeconds(T("cfg_autopull_after"), &config.bite_autopull_ms, 1000, 30000);
			ImGui::TextDisabled("%s", T("cfg_autopull_hint"));
		}
		SliderMsAsSeconds(T("cfg_minigame_delay"), &config.minigame_enter_delay_ms, 0, 2000);
		SliderMsAsSeconds(T("cfg_minigame_verify"), &config.minigame_verify_timeout_ms, 0, 10000);
		SliderMsAsSeconds(T("cfg_recast_fail_delay"), &config.recast_fail_delay_ms, 0, 5000);
		if (config.minigame_verify_timeout_ms > 0)
			ImGui::TextDisabled("%s", T("cfg_minigame_verify_hint"));
		SliderMsAsSeconds(T("cfg_cleanup_wait_before"), &config.cleanup_wait_before_ms, 0, 5000);
		ImGui::SliderInt(T("cfg_cleanup_clicks"), &config.cleanup_click_count, 0, 10);
		SliderMsAsSeconds(T("cfg_cleanup_interval"), &config.cleanup_click_interval_ms, 0, 1000);
		if (ImGui::InputText(T("cfg_cleanup_key"), bufReelKey, sizeof(bufReelKey))) {
			config.cleanup_reel_key_name = bufReelKey;
			config.cleanup_reel_key = getKeyVal(config.cleanup_reel_key_name);
		}
		SliderMsAsSeconds(T("cfg_cleanup_wait_after"), &config.cleanup_wait_after_ms, 0, 10000);
		SliderMsAsSeconds(T("cfg_control_interval"), &config.control_interval_ms, 5, 200);
	}

	// === Detection Thresholds ===
	if (ImGui::CollapsingHeader(T("sec_detection"))) {
		SliderDouble(T("cfg_bite_thresh"), &config.bite_threshold, 0.0f, 1.0f, "%.0f%%", true);
		SliderDouble(T("cfg_minigame_thresh"), &config.minigame_threshold, 0.0f, 1.0f, "%.0f%%", true);
		SliderDouble(T("cfg_fish_thresh"), &config.fish_icon_threshold, 0.0f, 1.0f, "%.0f%%", true);
		SliderDouble(T("cfg_slider_thresh"), &config.slider_threshold, 0.0f, 1.0f, "%.0f%%", true);
		ImGui::SliderInt(T("cfg_bite_frames"), &config.bite_confirm_frames, 1, 20);
		ImGui::SliderInt(T("cfg_end_frames"), &config.game_end_confirm_frames, 1, 30);
	}

	// === Slider Color Detection ===
	if (ImGui::CollapsingHeader(T("sec_slider_color"))) {
		ImGui::SliderInt(T("cfg_bright_thresh"), &config.slider_bright_thresh, 50, 255);
		ImGui::SliderInt(T("cfg_min_slider_h"), &config.slider_min_height, 5, 100);
		ImGui::SliderInt(T("cfg_detect_half_w"), &config.slider_detect_half_width, 1, 20);
		ImGui::SliderInt(T("cfg_merge_gap"), &config.slider_detect_merge_gap, 0, 100);
	}

	// === Multi-Scale Matching ===
	if (ImGui::CollapsingHeader(T("sec_multiscale"))) {
		ImGui::Text("%s", T("cfg_track_autoscale"));
		InputDouble(T("cfg_scale_min"), &config.track_scale_min, 0.05f, 0.1f, "%.2f");
		InputDouble(T("cfg_scale_max"), &config.track_scale_max, 0.05f, 0.1f, "%.2f");
		InputDouble(T("cfg_scale_step"), &config.track_scale_step, 0.01f, 0.05f, "%.3f");
		ImGui::InputInt(T("cfg_scale_topk"), &config.track_scale_refine_topk);
		InputDouble(T("cfg_scale_radius"), &config.track_scale_refine_radius, 0.01f, 0.05f, "%.3f");
		InputDouble(T("cfg_scale_rstep"), &config.track_scale_refine_step, 0.005f, 0.01f, "%.3f");

		ImGui::Separator();
		ImGui::Text("%s", T("cfg_angle_search"));
		InputDouble(T("cfg_angle_min"), &config.track_angle_min, 0.5f, 1.0f, "%.1f");
		InputDouble(T("cfg_angle_max"), &config.track_angle_max, 0.5f, 1.0f, "%.1f");
		InputDouble(T("cfg_angle_step"), &config.track_angle_step, 0.05f, 0.1f, "%.2f");
		ImGui::InputInt(T("cfg_angle_topk"), &config.track_angle_refine_topk);
		InputDouble(T("cfg_angle_radius"), &config.track_angle_refine_radius, 0.05f, 0.1f, "%.2f");
		InputDouble(T("cfg_angle_rstep"), &config.track_angle_refine_step, 0.01f, 0.05f, "%.3f");
	}

	// === Track Locking ===
	if (ImGui::CollapsingHeader(T("sec_track_lock"))) {
		ImGui::InputInt(T("cfg_track_pady"), &config.track_pad_y);
		ImGui::InputInt(T("cfg_lock_miss_mult"), &config.track_lock_miss_multiplier);
		ImGui::InputInt(T("cfg_lock_miss_min"), &config.track_lock_miss_min_frames);
		ImGui::InputInt(T("cfg_miss_release"), &config.miss_release_frames);
		ImGui::InputInt(T("cfg_end_min_frames"), &config.minigame_end_min_frames);
		ImGui::InputInt(T("cfg_slider_jump"), &config.slider_tpl_jump_threshold);
		ImGui::InputInt(T("cfg_fish_jump"), &config.fish_jump_threshold);
		ImGui::InputInt(T("cfg_slider_stable"), &config.slider_height_stable_min);
	}

	// === Velocity ===
	if (ImGui::CollapsingHeader(T("sec_velocity"))) {
		InputDouble(T("cfg_slider_vcap"), &config.slider_velocity_cap, 1.0f, 5.0f, "%.1f");
		InputDouble(T("cfg_fish_vcap"), &config.fish_velocity_cap, 1.0f, 5.0f, "%.1f");
		InputDouble(T("cfg_base_dt"), &config.base_dt_ms, 5.0f, 10.0f, "%.0f");
		SliderDouble(T("cfg_vel_alpha"), &config.velocity_ema_alpha, 0.0f, 1.0f, "%.0f%%", true);
	}

	// === Physics / MPC ===
	if (ImGui::CollapsingHeader(T("sec_physics"))) {
		InputDouble(T("cfg_gravity"), &config.bb_gravity, 0.1f, 0.5f, "%.2f");
		InputDouble(T("cfg_thrust"), &config.bb_thrust, 0.1f, 0.5f, "%.2f");
		SliderDouble(T("cfg_drag"), &config.bb_drag, 0.5f, 1.0f, "%.3f");
		ImGui::SliderInt(T("cfg_horizon"), &config.bb_sim_horizon, 1, 30);
		SliderDouble(T("cfg_margin"), &config.bb_margin_ratio, 0.0f, 0.45f, "%.0f%%", true);
		InputDouble(T("cfg_boundary_zone"), &config.bb_boundary_zone, 5.0f, 10.0f, "%.0f");
		SliderDouble(T("cfg_boundary_weight"), &config.bb_boundary_weight, 0.0f, 2.0f, "%.2f");
		SliderMsAsSeconds(T("cfg_log_interval"), &config.bb_log_interval_ms, 0, 2000);
	}

	// === Fish Prediction ===
	if (ImGui::CollapsingHeader(T("sec_fish_predict"))) {
		bool fishBounce = config.fish_bounce_predict != 0;
		if (ImGui::Checkbox(T("cfg_bounce"), &fishBounce))
			config.fish_bounce_predict = fishBounce ? 1 : 0;
		SliderDouble(T("cfg_accel_alpha"), &config.fish_accel_alpha, 0.0f, 1.0f, "%.0f%%", true);
		SliderDouble(T("cfg_vel_decay"), &config.fish_vel_decay, 0.0f, 1.0f, "%.0f%%", true);
		InputDouble(T("cfg_accel_cap"), &config.fish_accel_cap, 1.0f, 5.0f, "%.1f");
	}

	// === Reactive Override ===
	if (ImGui::CollapsingHeader(T("sec_reactive"))) {
		bool reactiveOn = config.reactive_override != 0;
		if (ImGui::Checkbox(T("cfg_reactive_enable"), &reactiveOn))
			config.reactive_override = reactiveOn ? 1 : 0;
		SliderDouble(T("cfg_dev_ratio"), &config.reactive_dev_ratio, 0.0f, 1.0f, "%.0f%%", true);
		InputDouble(T("cfg_grow_thresh"), &config.reactive_grow_threshold, 0.5f, 1.0f, "%.1f");
	}

	// === Templates ===
	if (ImGui::CollapsingHeader(T("sec_templates"))) {
		if (ImGui::InputText(T("cfg_resource_dir"), bufResourceDir, sizeof(bufResourceDir)))
			config.resource_dir = bufResourceDir;
		ImGui::TextDisabled("%s", T("cfg_tpl_hint"));
	}

	// === ML ===
	if (ImGui::CollapsingHeader(T("sec_ml"))) {
		ImGui::SliderInt(T("cfg_ml_mode"), &config.ml_mode, 0, 2);
		ImGui::SameLine(); ImGui::TextDisabled("%s", T("cfg_ml_hint"));
		if (ImGui::InputText(T("cfg_ml_csv"), bufMlCsv, sizeof(bufMlCsv)))
			config.ml_record_csv = bufMlCsv;
		if (ImGui::InputText(T("cfg_ml_weights"), bufMlWeights, sizeof(bufMlWeights)))
			config.ml_weights_file = bufMlWeights;
	}

	// === Debug ===
	if (ImGui::CollapsingHeader(T("sec_debug"))) {
		ImGui::Checkbox(T("cfg_debug_log"), &config.vr_debug);
		ImGui::Checkbox(T("cfg_debug_pic"), &config.vr_debug_pic);
		if (ImGui::InputText(T("cfg_debug_dir"), bufDebugDir, sizeof(bufDebugDir)))
			config.vr_debug_dir = bufDebugDir;
		ImGui::Checkbox(T("cfg_csv_log"), &config.vr_log_enabled);
		if (config.vr_log_enabled) {
			if (ImGui::InputText(T("cfg_log_file"), bufLogFile, sizeof(bufLogFile)))
				config.vr_log_file = bufLogFile;
		}
		if (ImGui::Checkbox(T("cfg_debug_console"), &config.debug_console)) {
			if (config.debug_console) {
				AllocConsole();
				freopen("CONOUT$", "w", stdout);
				freopen("CONOUT$", "w", stderr);
				printf("Debug console enabled\n");
			} else {
				FreeConsole();
			}
		}
		ImGui::SameLine();
		ImGui::TextDisabled("%s", T("cfg_console_hint"));
	}

	// === Language ===
	ImGui::Separator();
	{
		ImGui::Text("%s", T("cfg_language"));
		ImGui::SameLine();

		// Cache available languages
		static std::vector<std::string> langs;
		static bool langsLoaded = false;
		if (!langsLoaded) {
			langs = Lang::instance().listLanguages();
			langsLoaded = true;
		}

		// Find current index
		int curIdx = 0;
		for (int i = 0; i < (int)langs.size(); i++) {
			if (langs[i] == Lang::instance().currentLang) { curIdx = i; break; }
		}

		if (!langs.empty()) {
			if (ImGui::BeginCombo("##lang_combo", langs[curIdx].c_str())) {
				for (int i = 0; i < (int)langs.size(); i++) {
					bool selected = (i == curIdx);
					if (ImGui::Selectable(langs[i].c_str(), selected)) {
						std::string langPath = "lang/" + langs[i] + ".ini";
						if (Lang::instance().load(langPath)) {
							Lang::instance().currentLang = langs[i];
							config.language = langs[i];
						}
					}
					if (selected) ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
		}
	}
}

// --- Log Panel ---
static void RenderLogPanel() {
	ImGui::Text("%s", T("panel_log"));
	ImGui::SameLine(ImGui::GetContentRegionAvail().x - 120);
	static bool autoScroll = true;
	ImGui::Checkbox(T("chk_autoscroll"), &autoScroll);

	ImGui::Separator();

	// Calculate available height: leave room for preview if enabled
	float availH = ImGui::GetContentRegionAvail().y;
	float logH = (g_previewSRV != nullptr) ? availH * 0.65f : availH;

	ImGui::BeginChild("LogScroll", ImVec2(0, logH), ImGuiChildFlags_Borders,
		ImGuiWindowFlags_HorizontalScrollbar);

	auto& logger = GuiLogger::instance();
	auto entries = logger.getEntries();

	ImGuiListClipper clipper;
	clipper.Begin((int)entries.size());
	while (clipper.Step()) {
		for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
			const auto& e = entries[i];
			ImVec4 color;
			switch (e.level) {
				case LogLevel::Debug: color = ImVec4(0.6f, 0.6f, 0.6f, 1.0f); break;
				case LogLevel::Info:  color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); break;
				case LogLevel::Warn:  color = ImVec4(1.0f, 1.0f, 0.0f, 1.0f); break;
				case LogLevel::Error: color = ImVec4(1.0f, 0.3f, 0.3f, 1.0f); break;
			}
			// Format timestamp
			unsigned long long ms = e.timestamp % 86400000ULL;
			int h = (int)(ms / 3600000); ms %= 3600000;
			int m = (int)(ms / 60000);   ms %= 60000;
			int s = (int)(ms / 1000);

			ImGui::PushStyleColor(ImGuiCol_Text, color);
			ImGui::Text("[%02d:%02d:%02d] %s", h, m, s, e.message.c_str());
			ImGui::PopStyleColor();
		}
	}

	if (autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 20.0f)
		ImGui::SetScrollHereY(1.0f);

	ImGui::EndChild();

	if (ImGui::Button(T("btn_clear_log"))) {
		logger.clear();
	}
}

// --- Preview Panel ---
static void RenderPreviewPanel() {
	if (ImGui::CollapsingHeader(T("sec_preview"))) {
		ImGui::Checkbox(T("cfg_preview_enable"), &config.gui_preview_enabled);
		ImGui::SameLine();
		ImGui::Checkbox(T("cfg_preview_boxes"), &config.gui_preview_boxes);

		if (config.gui_preview_enabled) {
			UpdatePreviewTexture();
			if (g_previewSRV) {
				float availW = ImGui::GetContentRegionAvail().x;
				float scale = availW / (float)g_previewW;
				float dispH = (float)g_previewH * scale;
				ImGui::Image((ImTextureID)g_previewSRV, ImVec2(availW, dispH));
			}
		} else {
			ImGui::TextDisabled("%s", T("cfg_preview_off"));
		}
	}
}

// --- Main Window (combines all panels) ---
void RenderMainWindow() {
	// Full-window ImGui window
	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
	ImGui::Begin("##MainWindow", nullptr,
		ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoBringToFrontOnFocus);

	// Title
	ImGui::PushFont(nullptr);
	ImGui::Text("%s", T("app_title"));
	ImGui::PopFont();
	ImGui::Separator();

	// Status bar
	RenderStatusBar();

	// Control buttons
	RenderControlButtons();

	// Split: left = config, right = log + preview
	float availW = ImGui::GetContentRegionAvail().x;
	float configW = availW * 0.42f;

	// Config panel (left)
	ImGui::BeginChild("ConfigPanel", ImVec2(configW, 0), ImGuiChildFlags_Borders);
	ImGui::Text("%s", T("panel_config"));
	ImGui::Separator();
	RenderConfigEditor();
	ImGui::EndChild();

	ImGui::SameLine();

	// Right panel (log + preview)
	ImGui::BeginChild("RightPanel", ImVec2(0, 0), ImGuiChildFlags_Borders);
	RenderLogPanel();
	RenderPreviewPanel();
	ImGui::EndChild();

	ImGui::End();
}
