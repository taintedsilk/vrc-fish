# Changelog

All notable changes to this project will be documented in this file.

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
