<div align="center">
  <img src="banner.jpg" alt="VRChat FISHǃ 自动钓鱼 横幅" width="100%" />
</div>

<div align="center">
  <h1>VRChat FISHǃ 自动钓鱼（辅助向）</h1>
  <p>Windows · C++ · OpenCV 4.6.0 · 模板匹配 · 物理模型 + MPC（可选 ML）</p>
  <p>
    <a href="README.md">English</a> · <b>中文</b>
  </p>
  <p>
    <img alt="Platform" src="https://img.shields.io/badge/Platform-Windows-0078D6?style=flat-square" />
    <img alt="Language" src="https://img.shields.io/badge/Language-C%2B%2B-00599C?style=flat-square" />
    <img alt="OpenCV" src="https://img.shields.io/badge/OpenCV-4.6.0-5C3EE8?style=flat-square" />
    <img alt="License" src="https://img.shields.io/badge/License-GPL--3.0-blue?style=flat-square" />
  </p>
</div>

> 说明：本项目最初是我为一位**运动功能障碍**的朋友编写的辅助工具，公开出来主要用于**学习交流/研究**（授权以 `LICENSE` 为准）。
> 钓鱼是一件放松的事情，希望大家**以休闲为主**，并遵守 VRChat 及相关服务的规则/条款。

<details>
  <summary><b>目录</b></summary>

- [致谢](#致谢)
- [项目简介](#项目简介)
- [演示视频](#演示视频)
- [功能特性](#功能特性)
- [适用世界](#适用世界)
- [下载](#下载)
- [快速开始](#快速开始)
- [自行调节与适配](#自行调节与适配)
- [构建](#构建)
- [配置说明](#配置说明)
- [日志与调试](#日志与调试)
- [实验脚本（可选）](#实验脚本可选)
- [目录结构](#目录结构)
- [支持与参与](#支持与参与)
- [免责声明](#免责声明)
- [License](#license)

</details>

---

## 致谢

- 感谢我的好友 **aflotia**（VRChat 玩家）调试更加优秀的参数以及贡献素材图片

---

## 项目简介

一个 Windows 自动钓鱼工具：通过 OpenCV 模板匹配和亮度检测识别 VRChat 钓鱼界面，配合基于游戏真实物理参数的 MPC 控制器完成小游戏操作。所有设置均可在 Dear ImGui 图形界面中调整——无需手动编辑配置文件。

## 演示视频

4倍速：

<video src="https://github.com/user-attachments/assets/06b362be-6b1d-4f4d-ab5d-a4811a0f8a64" controls></video>

## 功能特性

- **自动循环**：抛竿 → 等咬钩 → 点击咬钩 → 控制小游戏 → 结算/清理 → 下一轮
- **图形界面**：Dear ImGui + DirectX11 窗口，包含：
  - 状态栏（连接状态、钓鱼状态、捕获计数）
  - 开始/停止、暂停/恢复、连接窗口按钮
  - 完整配置编辑器，可折叠分组（50+ 参数）
  - 彩色滚动日志面板
  - 实时截图预览，可选检测框叠加显示
  - 所有设置实时可调，一键保存
- **多语言**：英文/中文/日文（GUI 内切换）
- **识别方式**：
  - 多尺度模板匹配：咬钩感叹号、小游戏轨道、鱼图标、滑块模板（备用）
  - 亮度阈值检测滑块边界（优先）
  - 轨道多尺度 + 角度旋转搜索
- **控制方式**：基于游戏真实物理参数（重力 1.25、推力 3.75、反弹 0.3）的 MPC 模拟决策，带快速偏移紧急覆盖
- **后台输入**：默认开启——钓鱼时可自由使用鼠标/键盘（通过 `PostMessage`）
- **OSC 防挂机**：摇头或跳跃模式，通过 VRChat OSC API（UDP 127.0.0.1:9000）防止 FISHǃ 世界的 AFK 检测触发
- **自动恢复**：小游戏验证（失败自动重抛）、咬钩超时自动拉杆、静态检测超时
- **随机抛竿移动**：可配置偏移、随机范围、平滑多步鼠标移动
- **F9 全局热键** 切换开始/停止
- **可选 ML 流程**：录制数据/推理模式 + 分析拟合脚本

## 适用世界

本项目适配 VRChat 世界 **FISHǃ**：
- World URL: https://vrchat.com/home/world/wrld_ae001ea3-ed05-42f0-adf2-3d47efd10a77
- World ID: `wrld_ae001ea3-ed05-42f0-adf2-3d47efd10a77`

`Resource-VRChat/` 下的模板截图和默认阈值基于该世界的钓鱼 UI。若世界更新 UI，可能需要重新截取模板并调整阈值。

## 下载

前往 [Releases](https://github.com/abligail/vrc-fish/releases) 页面下载最新版本的预编译可执行文件。

## 快速开始

1. **（可选）** 在 VRChat 显示/图形设置中将分辨率设为 `1280×960`，可获得最佳模板匹配精度。其他分辨率也能使用，但可能需要调整阈值。
2. 进入 VRChat 世界 **FISHǃ**，确保你已在钓鱼点位，钓鱼 UI 可见。
3. 抛竿后，找一个合适的站位/视角，确保"上钩提醒"（感叹号）和小游戏的"滑块轨道"完整出现在屏幕内（不出屏/不遮挡）。
4. 运行 `vrc-fish.exe`，GUI 窗口打开，包含状态栏、控制按钮、配置编辑器、日志面板和实时截图预览。
5. 点击 **连接窗口** 连接 VRChat，然后点击 **开始钓鱼**（或按 <kbd>F9</kbd>）开始。
6. 可随时点击 **暂停**/**恢复**。所有设置均可在 GUI 中实时调整，点击 **保存配置** 保存。

提示：
- 程序可能需要**管理员权限**（清单中设置了 `RequireAdministrator`）。
- **后台输入** 默认开启——钓鱼时可自由使用鼠标和键盘。
- 首次运行时如果 `config.ini` 不存在，会自动创建默认配置。程序从当前工作目录加载 `config.ini` 和 `Resource-VRChat/`。
- **OSC 防挂机**（摇头）默认开启，防止 FISHǃ 世界的 AFK 检测触发。

## 自行调节与适配

仓库中的模板和默认参数基于**椰子湾木栈桥尽头**（初始岛屿），模型高度 **1.1 米**。默认参数已经能稳定钓起绝大部分鱼。

> **建议**：首次使用时先到上述位点验证程序正常运行，确认无误后再尝试其他位点。

如果识别效果不佳，按以下优先级调整。以下所有设置均可在 **GUI 配置面板**中直接修改——无需手动编辑 `config.ini`。

### 1. 调整轨道模板缩放范围（最常用）

不同站位、模型高度、鱼竿和分辨率/UI 缩放会导致滑块轨道大小不同。程序通过多尺度模板匹配适配，相关设置：

- **Track Scale Min**（默认 0.8）
- **Track Scale Max**（默认 2.0）
- **Track Scale Step**（默认 0.2）

**调整方法**：在 GUI 中开启 **Debug**，观察日志面板中轨道匹配的 `scale` 值（如 `scale=1.4`），围绕观察值缩小范围、减小步长。例如观察到 scale 在 `1.2~1.6`：

- Track Scale Min → 1.0
- Track Scale Max → 1.8
- Track Scale Step → 0.1

匹配值接近边界时需扩大范围。步长越小越精确但越慢，`0.1~0.2` 通常足够。

### 2. 保持滑块轨道尽量垂直

程序假设轨道接近垂直。如果视角导致倾斜：

- **推荐**：调整游戏内站位或视角让轨道垂直。
- **通过设置补偿**：在 GUI 中启用角度搜索：
  - Track Angle Min → -5.0
  - Track Angle Max → 5.0
  - Track Angle Step → 1.0

  通过日志中的 `angle` 值确认偏转，再缩小范围。角度搜索会增加计算量，仅在需要时开启。

### 3. 其他调整

- **日志中频繁出现 miss**：最有效的方法是在你的位点截图/抠图，替换 `Resource-VRChat/` 中的模板。也可在 GUI 中调整匹配阈值（Bite Threshold、Fish Icon Threshold 等）。
- **不同电脑/显示环境**：识别效果与分辨率和渲染设置相关，可能需要根据环境调整阈值和控制参数。

## 构建

1. 用 Visual Studio 打开 `vrc-fish.sln`
2. 选择 `x64` + `Release`（或 `Debug`）
3. 编译生成 `vrc-fish.exe`

依赖说明：
- OpenCV 4.6.0：`include/` + `lib/` 组织，运行时 DLL（`opencv_*460.dll`）放在可执行文件同目录（仓库根目录已包含）。
- 确保运行时能找到 `config.ini` 与 `Resource-VRChat/`（设置工作目录为仓库根目录，或拷贝到可执行文件目录）。

## 配置说明

所有设置均可在 **GUI 配置面板**中实时编辑，点击 **保存配置** 持久化到 `config.ini`。

以下为各设置的配置文件键名参考：

| 分类 | Key | 说明 |
|---|---|---|
| 窗口 | `window_class` / `window_title_contains` | VRChat 窗口匹配 |
| 窗口 | `force_resolution` / `target_width` / `target_height` | 可选：强制 VRChat 客户区分辨率 |
| 窗口 | `background_input` | 后台输入（默认开启） |
| 时序 | `capture_interval_ms` / `control_interval_ms` | 截图和控制循环间隔 |
| 抛竿 | `cast_mouse_move_dx/dy` | 抛竿后鼠标偏移（一轮结束反向移回） |
| 抛竿 | `cast_mouse_move_random_range` | 偏移随机 ± 像素范围 |
| 抛竿 | `cast_mouse_move_duration_ms` / `cast_mouse_move_step_ms` | 平滑多步移动时序 |
| 防挂机 | `osc_head_shake` / `osc_anti_afk_mode` | OSC 防挂机（0=跳跃, 1=摇头） |
| 防挂机 | `osc_shake_after_fails` | 连续失败 N 次后触发 |
| 检测 | `bite_threshold` / `minigame_threshold` / `fish_icon_threshold` / `slider_threshold` | 模板匹配置信度阈值（0–1） |
| 检测 | `bite_scale_min/max/step` | 多尺度咬钩感叹号搜索范围 |
| 检测 | `track_scale_min/max/step` / `track_angle_min/max/step` | 轨道缩放和旋转搜索 |
| 物理 | `bb_gravity` / `bb_thrust` | MPC 物理倍率（1.0 = 游戏真实值） |
| 物理 | `bb_sim_horizon` | MPC 前向模拟步数 |
| 清理 | `cleanup_wait_before_ms` / `cleanup_click_count` / `cleanup_reel_key` / `cleanup_wait_after_ms` | 捕获后清理流程 |
| 恢复 | `bite_autopull` / `bite_autopull_ms` | 未检测到咬钩时自动拉杆 |
| 恢复 | `minigame_verify_timeout_ms` | 小游戏未出现时超时重抛 |
| ML | `ml_mode` | 0=MPC 自动, 1=录制数据, 2=ML 推理 |
| 调试 | `debug` / `debug_pic` / `debug_dir` / `vr_log_file` | 日志、截图、CSV 输出 |

资源模板：
- 默认目录：`Resource-VRChat/`
- 鱼图标自动加载 `fish_icon_alt*.png`，无需逐个配置

## 日志与调试

- 所有日志实时显示在 **GUI 日志面板**中（按级别彩色标记）
- **Debug** 开关：启用详细日志（识别分数、状态切换、控制信息）
- **Debug Pic**：在调试目录下保存关键帧截图
- **Debug Console**：打开独立控制台窗口显示原始日志
- **CSV Log**：追加写入日志文件（见 `data/logs/` 示例）

## 实验脚本（可选）

脚本目录：`scripts/`

- `fit_physics.py`：从调试日志拟合滑块物理参数
- `analyze_log.py` / `analyze_oscillation.py`：分析 MPC 冲顶/震荡行为
- `train_bc.py`：行为克隆训练（需要 `numpy`），输出权重供 `ml_mode=2` 推理

## 目录结构

- `vrc-fish.cpp`：主程序（窗口捕获、模板匹配/检测、状态机、MPC 控制、清理流程）
- `gui/`：Dear ImGui 图形界面（面板、配置编辑器、预览、日志）
- `infra/`：平台层（输入模拟、OSC 通信）
- `lang/`：多语言翻译文件（en/zh/ja）
- `config.ini`：配置（首次运行自动创建）
- `Resource-VRChat/`：FISHǃ 世界钓鱼 UI 模板图片
- `data/`：日志与样例数据、ML 权重文件
- `scripts/`：分析/拟合/训练工具

## 支持与参与

如果觉得这个项目对你有帮助，欢迎给一个 Star 支持一下！

个人精力有限，可能没办法及时维护更新，非常欢迎感兴趣的朋友一起继续构建和完善代码 :)

## 免责声明

- 本项目非 VRChat 官方作品，与 VRChat 没有任何关联。
- 请遵守 VRChat 及相关服务的规则/条款；使用本项目带来的一切后果由使用者自行承担。
- 该代码按"学习与研究"目的公开，不提供任何保证；请勿用于破坏他人体验、违反服务条款或其他不当场景。
- 上述"使用建议/免责声明"仅为提醒，不构成对 `LICENSE`（GPL-3.0）条款的额外限制。

## License

本项目源码采用 GPL-3.0，详见 `LICENSE`。仓库中包含的第三方组件/资源可能适用不同许可或权利声明，详见 `THIRD_PARTY_NOTICES.md`。
