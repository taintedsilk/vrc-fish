<div align="center">
  <img src="banner.jpg" alt="VRChat FISHǃ Auto Fishing Assistant Banner" width="100%" />
</div>

<div align="center">
  <h1>VRChat FISHǃ Auto Fishing Assistant</h1>
  <p>Windows · C++ · OpenCV 4.6.0 · Template Matching · Physics + MPC (Optional ML)</p>
  <p>
    <b>English</b> · <a href="README.zh.md">中文</a>
  </p>
  <p>
    <img alt="Platform" src="https://img.shields.io/badge/Platform-Windows-0078D6?style=flat-square" />
    <img alt="Language" src="https://img.shields.io/badge/Language-C%2B%2B-00599C?style=flat-square" />
    <img alt="OpenCV" src="https://img.shields.io/badge/OpenCV-4.6.0-5C3EE8?style=flat-square" />
    <img alt="License" src="https://img.shields.io/badge/License-GPL--3.0-blue?style=flat-square" />
  </p>
</div>

> Note: This project was originally built as an **assistive tool** for a friend with **motor impairments**, and is shared mainly for **learning/research** (see `LICENSE` for licensing terms).
> Fishing should be relaxing—please keep it casual and follow VRChat and related service rules/terms.

<details>
  <summary><b>Table of Contents</b></summary>

- [Acknowledgements](#acknowledgements)
- [Overview](#overview)
- [Demo Video](#demo-video)
- [Features](#features)
- [Target World](#target-world)
- [Download](#download)
- [Quick Start](#quick-start)
- [Tuning \& Adaptation](#tuning--adaptation)
- [Build](#build)
- [Configuration](#configuration)
- [Logging \& Debugging](#logging--debugging)
- [Experimental Scripts (Optional)](#experimental-scripts-optional)
- [Project Layout](#project-layout)
- [Support \& Contributing](#support--contributing)
- [Disclaimer](#disclaimer)
- [License](#license)

</details>

---

## Acknowledgements

- Thanks to my good friend **aflotia** (VRChat player) for tuning better parameters and contributing template images

---

## Overview

A Windows tool that automates the VRChat fishing minigame using OpenCV template matching, brightness-based detection, and a game-accurate physics MPC controller. Everything is controlled through a Dear ImGui GUI — no manual config file editing required.

## Demo Video

4× speed:

<video src="https://github.com/user-attachments/assets/06b362be-6b1d-4f4d-ab5d-a4811a0f8a64" controls></video>

## Features

- **Fully automated loop**: cast → wait for bite → hook → control minigame → cleanup → next round
- **GUI**: Dear ImGui + DirectX11 window with:
  - Status bar (connection state, fishing state, catch count)
  - Start/Stop, Pause/Resume, Connect Window buttons
  - Full config editor with collapsible sections (50+ parameters)
  - Color-coded scrollable log panel
  - Live capture preview with optional detection bounding boxes
  - All settings adjustable live, saved with one click
- **Multi-language**: English / Chinese / Japanese (switchable in GUI)
- **Detection**:
  - Multi-scale template matching for bite exclamation mark, minigame track, fish icon(s), slider (fallback)
  - Brightness-based slider boundary detection (primary)
  - Multi-scale + angle search for track locking
- **Control**: game-accurate physics simulation (gravity 1.25, thrust 3.75, bounce 0.3) + MPC to decide hold/release, with reactive override for fast fish movement
- **Background input**: enabled by default — fish without losing mouse/keyboard control (uses `PostMessage`)
- **OSC anti-AFK**: head shake or jump mode via VRChat OSC API (UDP 127.0.0.1:9000) to prevent the FISHǃ world's AFK detection
- **Auto-recovery**: minigame verification with auto-recast on failed hook, bite autopull fallback, static detection timeout
- **Randomized cast movement**: configurable offset, random range, smooth multi-step mouse movement
- **F9 global hotkey** to toggle start/stop
- **Optional ML workflow**: record data / inference mode + analysis & fitting scripts

## Target World

This project is tuned for **FISHǃ**:
- World URL: https://vrchat.com/home/world/wrld_ae001ea3-ed05-42f0-adf2-3d47efd10a77
- World ID: `wrld_ae001ea3-ed05-42f0-adf2-3d47efd10a77`

The bundled templates under `Resource-VRChat/` and default thresholds are calibrated for this world's UI. If the world updates its UI, you may need to re-capture templates and adjust thresholds.

## Download

Head to the [Releases](https://github.com/abligail/vrc-fish/releases) page to download the latest pre-built executable.

## Quick Start

1. **(Optional)** In VRChat display/graphics settings, set the resolution to `1280×960` for best template matching accuracy. Other resolutions work but may need threshold tuning.
2. Enter **FISHǃ** in VRChat. Make sure you are at a fishing spot and the fishing UI is visible.
3. After casting, choose a position/view so that the bite indicator (exclamation mark) and the full minigame slider track are visible on screen (not cropped or occluded).
4. Run `vrc-fish.exe`. The GUI window opens with status bar, controls, config editor, log panel, and live capture preview.
5. Click **Connect Window** to attach to VRChat, then click **Start Fishing** (or press <kbd>F9</kbd>) to begin.
6. Use **Pause**/**Resume** to pause mid-session. All settings can be adjusted live in the GUI and saved with the **Save Config** button.

Notes:
- The app may require **admin privileges** (manifest has `RequireAdministrator`).
- **Background input** is enabled by default — you can use your mouse and keyboard normally while the bot runs.
- `config.ini` is auto-created with defaults on first run if it doesn't exist. The app loads it from the current working directory alongside `Resource-VRChat/`.
- **OSC anti-AFK** (head shake) is enabled by default to prevent the FISHǃ world's AFK detection from triggering.

## Tuning & Adaptation

The bundled templates and defaults are based on the **end of the wooden pier at Coconut Bay** (starting island), with an avatar height of **1.1 m**. The defaults already reliably catch the vast majority of fish.

> **Tip**: First try the same spot described above to verify everything works in your environment, then adjust for other locations.

If detection is not working well, adjust in the following priority. All settings below can be changed directly in the **GUI config panel** — no need to edit `config.ini` manually.

### 1. Adjust Track Template Scale Range (Most Common)

Different positions, avatar heights, fishing rods, and resolution/UI scaling cause the slider track to appear at different sizes on screen. The program uses multi-scale template matching to handle this. The relevant settings are:

- **Track Scale Min** (default 0.8)
- **Track Scale Max** (default 2.0)
- **Track Scale Step** (default 0.2)

**How to adjust**: Enable **Debug** in the GUI and watch the log panel for the `scale` value when the track is matched (e.g. `scale=1.4`). Narrow the range around your observed value and reduce the step for better precision. For example, if you see scales around `1.2~1.6`:

- Track Scale Min → 1.0
- Track Scale Max → 1.8
- Track Scale Step → 0.1

If the matched scale is near the min/max boundary, expand the range. A smaller step is more precise but slower — `0.1~0.2` is usually sufficient.

### 2. Keep the Slider Track as Vertical as Possible

The program assumes the track is roughly vertical. If your view angle causes noticeable tilt:

- **Recommended**: Adjust your in-game position or camera angle so the track appears vertical.
- **Compensate via settings**: Enable angle search in the GUI:
  - Track Angle Min → -5.0
  - Track Angle Max → 5.0
  - Track Angle Step → 1.0

  Check the `angle` value in the log to confirm, then narrow the range. Angle search multiplies computation cost, so only enable when needed.

### 3. Other Adjustments

- **Frequent "miss" in logs**: The most effective fix is to screenshot your own fishing spot, crop the UI elements, and replace the images in `Resource-VRChat/`. You can also adjust matching thresholds (Bite Threshold, Fish Icon Threshold, etc.) in the GUI.
- **Different PC / display environments**: Detection is sensitive to screen resolution and rendering settings. Thresholds and control parameters may need adjustment for your setup.

## Build

1. Open `vrc-fish.sln` with Visual Studio
2. Select `x64` + `Release` (or `Debug`)
3. Build `vrc-fish.exe`

Dependencies:
- OpenCV 4.6.0: headers/libs under `include/` + `lib/`, runtime DLLs (`opencv_*460.dll`) next to the executable (included in repo root).
- Make sure the runtime can locate `config.ini` and `Resource-VRChat/` (set working directory to repo root, or copy them next to the executable).

## Configuration

All settings can be edited live in the **GUI config panel** and saved with the **Save Config** button. Settings are persisted to `config.ini`.

For reference, the key settings and their config file keys:

| Category | Key | Description |
|---|---|---|
| Window | `window_class` / `window_title_contains` | VRChat window matching |
| Window | `force_resolution` / `target_width` / `target_height` | Optional: force VRChat client-area resolution |
| Window | `background_input` | Background input via PostMessage (default: on) |
| Timing | `capture_interval_ms` / `control_interval_ms` | Capture and control loop intervals |
| Cast | `cast_mouse_move_dx/dy` | Post-cast mouse offset (reversed at end of round) |
| Cast | `cast_mouse_move_random_range` | Random ± pixel range added to cast offset |
| Cast | `cast_mouse_move_duration_ms` / `cast_mouse_move_step_ms` | Smooth multi-step movement timing |
| Anti-AFK | `osc_head_shake` / `osc_anti_afk_mode` | OSC anti-AFK (0=jump, 1=head shake) |
| Anti-AFK | `osc_shake_after_fails` | Trigger after N consecutive failed casts |
| Detection | `bite_threshold` / `minigame_threshold` / `fish_icon_threshold` / `slider_threshold` | Template matching confidence thresholds (0–1) |
| Detection | `bite_scale_min/max/step` | Multi-scale bite exclamation search range |
| Detection | `track_scale_min/max/step` / `track_angle_min/max/step` | Track template scale and rotation search |
| Physics | `bb_gravity` / `bb_thrust` | MPC physics multipliers (1.0 = game-accurate) |
| Physics | `bb_sim_horizon` | MPC forward simulation steps |
| Cleanup | `cleanup_wait_before_ms` / `cleanup_click_count` / `cleanup_reel_key` / `cleanup_wait_after_ms` | Post-catch cleanup sequence |
| Recovery | `bite_autopull` / `bite_autopull_ms` | Auto-pull fallback if no bite detected |
| Recovery | `minigame_verify_timeout_ms` | Recast if minigame doesn't appear within timeout |
| ML | `ml_mode` | 0=MPC auto, 1=record data, 2=ML inference |
| Debug | `debug` / `debug_pic` / `debug_dir` / `vr_log_file` | Logging, screenshots, CSV output |

Templates:
- Default directory: `Resource-VRChat/`
- Fish icons: auto-loads `fish_icon_alt*.png` without config entries

## Logging & Debugging

- All log output is shown in the **GUI log panel** in real time (color-coded by level)
- **Debug** toggle in GUI: enables verbose logging (scores, state transitions, control info)
- **Debug Pic**: saves key-frame screenshots under the debug directory
- **Debug Console**: opens a separate console window for raw log output
- **CSV Log**: appends runtime data to a log file (see `data/logs/` for examples)

## Experimental Scripts (Optional)

Scripts in `scripts/`:

- `fit_physics.py`: fit slider physics parameters from debug logs
- `analyze_log.py` / `analyze_oscillation.py`: analyze overshoot/oscillation behavior
- `train_bc.py`: behavior cloning training (requires `numpy`), outputs weights for `ml_mode=2`

## Project Layout

- `vrc-fish.cpp`: main app (capture, detection, state machine, MPC control, cleanup)
- `gui/`: Dear ImGui GUI (panels, config editor, preview, logger)
- `infra/`: platform layer (input simulation, OSC communication)
- `lang/`: translation files (en/zh/ja)
- `config.ini`: configuration (auto-created on first run)
- `Resource-VRChat/`: UI templates for FISHǃ
- `data/`: logs, sample data, ML weights
- `scripts/`: analysis / fitting / training utilities

## Support & Contributing

If you find this project helpful, a Star would be much appreciated!

I have limited time to maintain this regularly, so contributions from anyone interested are very welcome :)

## Disclaimer

- This is NOT an official VRChat project and is not affiliated with VRChat.
- Please follow VRChat and related service terms/rules. Use at your own risk.
- This repository is shared for learning/research purposes with no warranty. Please do not use it to harm others' experience, violate service terms, or for other improper purposes.
- The above guidance is informational and is not an additional restriction on the GPL-3.0 license terms.

## License

The source code of this project is licensed under GPL-3.0. See `LICENSE`. Third-party components/assets in this repository may be under different licenses or rights notices; see `THIRD_PARTY_NOTICES.md`.
