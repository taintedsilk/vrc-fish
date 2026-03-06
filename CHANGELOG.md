# Changelog

All notable changes to this project will be documented in this file.

## [2.3.0] - 2026-03-06

### Added
- Modular code refactoring: split monolithic vrc-fish.cpp into core/, engine/, infra/, runtime/ modules
- OSC head shake only triggers after N consecutive failures (`osc_shake_after_fails`, default 1)
- Configurable delay after OSC shake before casting (`osc_shake_post_delay_ms`, default 500ms)
- Separate configurable delay for failed recasts (`recast_fail_delay_ms`, default 1000ms)
- Background mode mouse movement support via PostMessage WM_MOUSEMOVE
- Preview crosshair overlay showing click center and mouse offset position
- GUI controls and translations (EN/ZH/JA) for all new settings

### Changed
- OSC head shake disabled by default

## [2.2.0] - 2026-03-05

### Added
- Randomized cast mouse movement with configurable offset range, random delay, and smooth multi-step movement (ported from upstream)
- GUI controls and translations (EN/ZH/JA) for new cast movement settings

### Changed
- Cast mouse move DX default increased from 10 to 20

## [2.1.0] - 2026-03-05

### Added
- Background Input mode (PostMessage): fish without losing mouse/keyboard control
- Auto-create config.ini with defaults when the file is missing
- Minigame verification: after hook/autopull click, verifies the minigame UI actually appeared; retracts and recasts if not (configurable timeout, default 3s)

### Fixed
- Rod not properly recasting after catching a fish (cleanup sequence leaving game in wrong state)

## [2.0.0] - 2026-03-04

### Added
- Dear ImGui + DirectX11 GUI replacing console window
  - Status bar (VRChat connection, fishing state, catch count)
  - Start/Stop/Pause/Resume/Reconnect control buttons
  - Full config editor with collapsible sections and appropriate widgets
  - Scrollable color-coded log panel
  - Live capture preview panel with optional detection bounding boxes
  - Config save button (writes back to config.ini)
- F9 global hotkey to toggle start/stop
- Preview overlay showing detection bounding boxes (track ROI, fish position, slider bounds)
- Checkboxes to toggle preview and detection box rendering
- CSV log file enable/disable checkbox with auto directory creation
- Auto-pull fallback: optionally click after configurable wait time if no bite detected
- Toggleable debug console checkbox (live AllocConsole/FreeConsole)
- Multi-language support with translation files (`lang/en.ini`, `lang/zh.ini`, `lang/ja.ini`)
- Language selector dropdown in config panel (switches instantly)
- CJK font loading for Chinese and Japanese character support
- Human-readable UI values (seconds instead of milliseconds, percentages for thresholds/ratios)
- Build script (build.bat) for VS2026 command-line builds

### Changed
- Screen capture: replaced `StretchBlt` with `PrintWindow(PW_CLIENTONLY | PW_RENDERFULLCONTENT)` to fix capture requiring window resize
- Entry point changed from console (`main`) to Windows GUI (`wWinMain`)
- All `cout`/`printf` output replaced with thread-safe `LOG_*` macros routed to GUI log panel
- Project subsystem changed from Console to Windows

### Fixed
- Screen capture not working until window was resized (DWM recomposition issue)
- Log file directory creation now uses absolute paths for reliability
- Save Config causing "Not Responding" freeze (infinite loop in ini parser when key names appeared in comments)
- Window resizing when force_resolution is off (`ShowWindow(SW_RESTORE)` on maximized windows)

## [1.1.0] - 2026-03-03

### Added
- Cast rod head-shake to prevent getting stuck
- Slider rotation correction and color-based detection
- Dynamic fish icon template loading
- Multi-scale template matching config options (scale/angle range and step)
- Fish prediction and reactive deviation response
- GitHub Actions CI build workflow

### Fixed
- Slider detection logic optimization
- GBK source/execution charset for cross-platform build

## [1.0.1] - 2026-03-01

### Added
- Multi-scale track matching
- Log directory auto-creation

## [1.0.0] - 2026-02-28

### Added
- Initial release
- VRChat FISH! minigame auto-fishing via screen capture and template matching
- Configurable detection thresholds, timing, and physics parameters
- MPC-based slider control with velocity estimation
- ML mode support (recording and inference)
- config.ini based configuration
