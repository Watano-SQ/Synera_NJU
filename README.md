# Synera: PvZ Auto Arena

Synera 是一个使用 C++20 和 Qt Widgets 实现的单机 PvE 自走棋原型。项目保留原有 `synera` 命名空间、CMake target 和 `SYNERA_SAVE 1` 存档格式；当前玩法和素材主题是植物大战僵尸。

## 当前功能

- 8x8 棋盘，并按敌方半场/玩家半场分区。
- 玩家 Bench、棋盘部署、棋盘移动、棋盘回 Bench。
- `Prep`、`Combat`、`Resolve`、`GameOver` 四阶段循环。
- 五格商店、金币购买、商店刷新。
- 人口等级与上场单位上限。
- PvE 敌人波次生成。
- 核心战斗循环：移动、攻击冷却、法力、技能、胜败结算、战斗后恢复玩家棋盘。
- 三合一升为二星，同时保留基础 `definitionId`、traits、factory 类型和技能行为。
- 装备库存、装备属性、胜利掉落装备。
- shooter、nut、sun、healer、fungus、spike 羁绊。
- 文本存档/读档，并在读档成功前完成校验。
- Qt GUI：棋盘、Bench、商店、装备栏、羁绊面板、单位 Inspector 和 PvZ 风格资源。

## 项目结构

```text
src/core/      游戏规则、状态所有权、战斗、商店、存档、目录、单位
src/gui/       Qt Widgets GUI、资源加载、拖拽、面板、主窗口
tests/         阶段测试和 GUI 布局测试
tools/         资源生成脚本
assets/        GUI 和资源测试使用的运行时资源
pictures/      tools/build_assets.py 使用的源素材池
```

`GameState` 是核心状态入口。GUI widgets 读取 `GameState`，并通过 `PlacementController` 或 `MainWindow` 调用 `GameState` 命令来修改状态。

## 构建要求

- CMake 3.20 或更新版本
- 支持 C++20 的编译器
- Qt 6 Widgets
- 仅在重新生成资源时需要 Python 和 Pillow

在仓库根目录配置并构建：

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug
```

如果 CMake 无法自动找到 Qt，可以传入本机 Qt CMake 前缀：

```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH="<Qt install>\lib\cmake"
cmake --build build --config Debug
```

## 测试

```powershell
.\build\stage1_tests.exe
.\build\stage2_tests.exe
.\build\stage3_tests.exe
```

无界面环境中运行 GUI 布局 smoke test：

```powershell
$env:QT_QPA_PLATFORM = "offscreen"
.\build\gui_layout_tests.exe
Remove-Item Env:\QT_QPA_PLATFORM
```

阶段测试基于 `assert`，通过时通常不会输出内容。

## 运行 GUI

```powershell
.\build\synera_gui.exe
```

GUI 会按顺序从可执行文件旁的 `assets/`、项目 `assets/` 和当前工作目录的 `assets/` 查找资源。

## 重新生成资源

运行时资源位于 `assets/`，源素材池位于 `pictures/`。

```powershell
python .\tools\build_assets.py
```

脚本会生成单位图、敌人图、装备图标、羁绊徽章、商店卡、UI 元素和日/夜棋盘背景。

## 文档

- `PA说明文档.pdf`：课程 PA 规格与 checklist。
- `CODE_FAMILIARIZATION_GUIDE.md`：面向当前实现的完整代码熟悉指南。
