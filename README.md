# Synera: PvZ Auto Arena

## 基本信息

- 姓名：邹少乾
- 学号：251880509
- 项目名称：Synera: PvZ Auto Arena
- 项目类型：C++20 / Qt 6 Widgets 单机 PvE 自走棋
- 阶段完成度：
  - 阶段一：棋盘、备战区、单位部署、合法性检查已完成。
  - 阶段二：商店、金币、人口等级、敌人波次和自动战斗流程已完成。
  - 阶段三：星级合成、装备系统、羁绊系统、存档读档已完成。
  - 阶段四：Qt 图形界面、PvZ 主题资源、拖拽部署、背景音乐和完整可执行版已完成。

## 运行方式

推荐直接运行发布版：

```text
release/Synera/Synera.exe
```

双击 `Synera.exe` 即可进入图形界面。发布目录中已经包含运行所需的 Qt 动态库、插件、`assets/` 资源目录和背景音乐文件。

如需从源码构建：

```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH="<Qt install>\lib\cmake"
cmake --build build --config Debug
```

构建完成后可运行：

```powershell
.\build\Synera.exe
```

## 文件树结构

```text
CppPA_re/
├─ README.md                         项目说明、运行方式、核心设计和 AI 使用说明
├─ PA说明文档.pdf                    课程 PA 要求
├─ CMakeLists.txt                    CMake 构建脚本
├─ CODE_FAMILIARIZATION_GUIDE.md     代码熟悉指南
├─ assets/                           游戏运行时资源
│  ├─ audio/                         背景音乐
│  ├─ backgrounds/                   棋盘背景
│  ├─ enemies/                       敌人图像
│  ├─ items/                         装备图标
│  ├─ shop_cards/                    商店卡牌
│  ├─ traits/                        羁绊徽章
│  ├─ ui/                            UI 素材
│  └─ units/                         玩家单位图像
├─ src/
│  ├─ core/                          游戏规则、状态、单位、战斗、商店、存档
│  ├─ gui/                           Qt Widgets 界面、拖拽、绘制、面板和主窗口
│  └─ main.cpp                       CLI 规则演示入口
├─ tests/                            阶段测试和 GUI smoke test
└─ release/Synera/                   最终可双击运行的发布版
```

按照 PA 提交要求，最终压缩包只保留源码、资源、文档和游戏最终可执行文件；不包含 `build/`、`build-ascii/`、对象文件、CMake 缓存、截图产物、临时文件或旧素材生成中间文件。

## 核心类和数据结构

- `GameState`：游戏状态总入口，负责阶段流转、购买、部署、战斗、结算、合成、装备、存档读档。
- `Board`：维护 M x N 棋盘占用关系，检查格子合法性、单位位置和玩家/敌方半场限制。
- `Bench`：维护玩家备战区槽位，支持购买后暂存、上阵、交换和撤回。
- `Unit` / `BasicUnit`：单位抽象与基础实现，保存 HP、攻击、射程、法力、星级、羁绊、装备等属性。
- `UnitManager`：用稳定 `UnitId` 管理单位生命周期，避免 UI 和战斗逻辑直接持有易失效指针。
- `Catalog` / `UnitDefinition` / `ItemDefinition`：集中保存单位、装备和羁绊的静态定义，是商店、战斗和 UI 展示共享的数据来源。
- `SkillContext`：为单位技能提供可控的战斗上下文，封装伤害、治疗、目标搜索、状态改动和法力变化。
- `EncounterGenerator`：按回合生成 PvE 敌方阵容，并随着阶段推进提高敌人强度。
- `PlacementController`：连接 GUI 拖拽操作与核心部署规则，把界面动作转化为 `GameState` 命令。
- `MainWindow`：Qt 主窗口，组合棋盘、备战区、商店、装备栏、羁绊面板、Inspector 和背景音乐。
- `AssetManager`：按 `assets/` 目录查找并缓存运行时图像资源，为 GUI 绘制提供统一入口。

## 算法描述

- 寻路与目标锁定：战斗 tick 中先寻找最近敌方目标；若不在攻击距离内，则按棋盘坐标距离选择下一步移动候选，避开已占用格子并逐步接近目标。
- 自动战斗循环：`GameState::tickCombat` 按固定时间步推进移动、攻击冷却、受击、法力增长、技能释放和死亡清理；一方单位清空后进入 `Resolve`。
- 羁绊计算：从玩家棋盘单位收集 trait 计数，根据阈值生成当前激活羁绊，再统一刷新玩家单位有效属性。
- 三合一升星：购买或读档恢复后按 `definitionId` 和星级寻找同类单位，满足三合一条件时保留一个单位并提升星级。
- 商店刷新：根据等级、金币和单位池生成五格商店，购买时扣除金币并把单位放入备战区。
- 存档读档：文本格式以 `SYNERA_SAVE 1` 开头；读档先写入临时 `GameState` 并完整校验，成功后再替换当前状态，避免坏存档污染运行中的游戏。

## 辅助函数

- `toString` / `parseOwner` / `parsePhase` 等转换函数：在存档、日志和测试中把枚举与文本互转。
- `findUnitDefinition` / `createUnitFromDefinition`：从目录定义创建正式单位，避免 GUI 和测试绕过统一单位配置。
- `calculateSynergies` / `applySynergyBonuses`：统计羁绊数量并把加成同步到玩家棋盘单位。
- `drawPixmapAspectFit` / `aspectFitRect`：在 Qt 绘制中按比例适配资源，防止贴图变形。
- `encodeDragData` / `decodeDragData`：把拖拽来源、单位 id 和位置编码进 Qt MIME 数据。
- `findNearestEnemy` / `chooseStepToward`：辅助战斗中的目标选择和格子移动。

## 测试

```powershell
.\build\stage1_tests.exe
.\build\stage2_tests.exe
.\build\stage3_tests.exe
```

GUI 布局测试可在 Qt platform plugin 可用时运行：

```powershell
$env:QT_QPA_PLATFORM_PLUGIN_PATH = "<Qt install>\plugins\platforms"
$env:QT_QPA_PLATFORM = "offscreen"
.\build\gui_layout_tests.exe
Remove-Item Env:\QT_QPA_PLATFORM
Remove-Item Env:\QT_QPA_PLATFORM_PLUGIN_PATH
```

阶段测试基于 `assert`，通过时通常不会输出内容。

## AI 使用说明

本项目开发中使用 AI 辅助进行需求拆分、代码走读、注释整理、README 整理、测试思路检查和部分实现方案评审。所有代码都需要由提交者理解后再提交，验收时应能解释核心模块的状态流、数据所有权和算法设计。

AI 辅助较多的部分包括：

1. `GameState` 的阶段流转、存档读档与规则整合。该模块把棋盘、备战区、玩家经济、单位管理、装备和羁绊集中在一个状态入口中，对外暴露命令式接口，例如购买、部署、开始战斗、结算、保存和读取。
2. Qt GUI 的面板拆分与拖拽交互。`MainWindow` 负责组合控件和转发命令，`BoardWidget`、`BenchWidget`、`ShopPanel`、`EquipmentPanel`、`SynergyPanel` 分别展示不同区域；拖拽数据通过 `DragData` 编码为稳定的 MIME payload，再由 `PlacementController` 转换成核心层部署命令。
3. 文档和注释整理。AI 帮助把实现结构整理成 README、代码熟悉指南和中文解释性注释，但最终功能、验收演示和代码解释仍由提交者自行掌握。
