# Synera: Synergy Auto-Arena

本仓库当前实现 PA 第一阶段逻辑核心与 Qt Widgets GUI。代码采用 C++ OOP 风格，把棋盘、备战区、单位、玩家与敌方轮次生成拆成独立模型，GUI 只通过 `GameState` 公开 API 触发状态变化。

## 阶段一完成度

- [x] M x N 棋盘、玩家半场/敌方半场与地块占用规则
- [x] Bench 与棋盘之间的数据同步
- [x] `Unit` 基类，包含 HP/ATK/Range/Max Mana/Mana 等字段
- [x] 我方/敌方统一 `Unit` 类型，仅用 `owner` 区分控制归属
- [x] `traits` 仅作为羁绊标签，不参与敌我判断
- [x] 玩家实体与敌方轮次生成 `Er`
- [x] Qt GUI 展示棋盘/备战区/单位信息，并支持玩家单位拖拽摆放

## 文件结构

```text
src/
  main.cpp                  命令行 smoke demo
  core/
    Types.*                 公共枚举、UnitId、坐标与位置类型
    Unit.*                  Unit 基类与 BasicUnit
    Player.*                玩家经营实体
    Board.*                 M x N 棋盘与半场规则
    Bench.*                 一维备战区
    UnitManager.*           UnitId 到 Unit 对象的集中管理
    GameState.*             统一执行 Bench/Board/Unit 三方同步
    EncounterGenerator.*    按轮次生成敌方集合 Er
  gui/
    MainWindow.*            Qt 主窗口、状态栏与刷新协调
    PlacementController.*   将 drop 请求转换为 GameState API 调用
    BoardWidget.*           只读绘制棋盘与处理棋盘 drop
    BenchWidget.*           只读绘制备战区与处理 Bench drop
    InspectorPanel.*        只读显示选中单位属性
    AssetManager.*          加载并缓存 GUI 图片资源
tests/
  test_stage1.cpp           阶段一断言测试
```

## 核心设计

- `UnitManager` 独占所有 `Unit` 对象，`Board` 与 `Bench` 只保存 `UnitId`，避免单位被复制后状态不同步。
- `Board` 与 `Bench` 的写入函数是私有的，所有移动通过 `GameState` 完成，保证容器占用和 `Unit::placement` 原子更新。
- 默认棋盘为 8 x 8，Bench 为 8 格，也支持构造自定义尺寸。
- 坐标使用 0-based `Position{row, col}`。敌方半场为 `row < rows / 2`，玩家半场为 `row >= rows / 2`。
- `Owner::PlayerCtrl` 与 `Owner::EnemyCtrl` 是唯一敌我来源，`traits` 只保留给后续羁绊系统。
- `PlacementPolicy::Reject` 用于非法放置回弹，`PlacementPolicy::Swap` 用于后续 GUI 拖拽交换。
- GUI 展示 widget 只持有 `const GameState*`，不缓存 `Unit` 副本，也不直接修改 `Board` / `Bench` / `UnitManager`。
- `PlacementController` 是拖拽落点到核心 API 的唯一转换层，失败时不做任何状态补偿，只返回 message。

## GUI 拖拽规则

- Qt Drag & Drop payload 使用 `QByteArray + QDataStream`，包含 `UnitId`、来源类型、来源 Bench slot 或 Board position。
- 只有 `owner == PlayerCtrl` 且 GUI 阶段为 `Prep` 的单位可以拖动；敌方单位只能选中查看。
- Bench 到空 Board：调用 `deployFromBench(..., Reject)`。
- Bench 到占用 Board：阶段一直接 Reject，避免目标单位无处可去。
- Board 到空己方 Board：调用 `moveBoardUnit(..., Reject)`。
- Board 到己方占用 Board：调用 `moveBoardUnit(..., Swap)`，交换两个单位位置。
- Board 到空 Bench：调用 `returnToBench`；目标 Bench 已占用则 Reject。
- 非法放置保证 `Board` 占用、`Bench` 占用和 `Unit::placement` 不变，状态栏显示 `Invalid placement` 或更具体提示。

## 资源目录

GUI 会从可执行文件旁的 `assets/` 和项目根目录的 `assets/` 查找 PNG/JPG/JPEG 图片。`Unit::visualKey` 只用于 GUI 查找素材，不参与 owner、traits、战斗或部署规则。

`AssetManager` 会缓存 `QPixmap`，不会在 `paintEvent()` 中反复读文件。没有 `assets/` 目录或找不到素材时，GUI 自动回退到纯色方块、文本、HP 条和 Mana 条。当前阶段不要求 GIF 动画，后续可用 `QMovie` 扩展。

## 编译与运行

当前环境可用 MSYS2 g++ 编译命令行版本：

```powershell
& 'D:\Apps\MSYS2\ucrt64\bin\g++.exe' -std=c++20 -Wall -Wextra -pedantic -Isrc src\main.cpp src\core\*.cpp -o synera_demo.exe
.\synera_demo.exe
```

运行测试：

```powershell
& 'D:\Apps\MSYS2\ucrt64\bin\g++.exe' -std=c++20 -Wall -Wextra -pedantic -Isrc tests\test_stage1.cpp src\core\*.cpp -o stage1_tests.exe
.\stage1_tests.exe
```

测试程序无输出即表示所有断言通过。

Qt GUI 推荐使用 Qt 自带 CMake 和 MinGW：

```powershell
& 'D:\Apps\Qt\Tools\CMake_64\bin\cmake.exe' -S . -B build -G "MinGW Makefiles" `
  -DCMAKE_PREFIX_PATH='D:\Apps\Qt\6.11.1\mingw_64' `
  -DCMAKE_CXX_COMPILER='D:\Apps\Qt\Tools\mingw1310_64\bin\g++.exe' `
  -DCMAKE_MAKE_PROGRAM='D:\Apps\Qt\Tools\mingw1310_64\bin\mingw32-make.exe'
& 'D:\Apps\Qt\Tools\CMake_64\bin\cmake.exe' --build build
$env:PATH='D:\Apps\Qt\6.11.1\mingw_64\bin;D:\Apps\Qt\Tools\mingw1310_64\bin;' + $env:PATH
.\build\synera_gui.exe
```

## AI 使用说明

本阶段由 AI 辅助完成工程规划与代码初稿。核心模块包括：

- `GameState`：集中处理 Bench、Board、Unit 三者之间的位置同步，避免 GUI 或其它系统直接修改占用表。
- `EncounterGenerator`：根据当前轮次生成 `EnemyCtrl` 单位并部署到敌方半场，形成文档中的 `Er`。

后续继续开发时，需要确保自己能解释每个核心类的职责和移动流程，再在其上添加 GUI、战斗状态机、寻路、商店、羁绊、装备和存档。
