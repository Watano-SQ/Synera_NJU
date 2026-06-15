# Synera: PvZ Auto Arena

本项目是一个使用 C++20 和 Qt Widgets 实现的单机 PvE 自走棋。当前版本保留 `synera`
命名空间、CMake target 和 `SYNERA_SAVE 1` 存档头，但游戏内容已经切换为植物大战僵尸
主题：植物单位、僵尸敌人、阳光经济、植物装备、PvZ 羁绊和草坪棋盘资源。

## 完成度

- [x] 8x8 棋盘、玩家半场/敌方半场、地块占用规则。
- [x] Bench 与 Board 的 `UnitId` 同步，所有状态变更通过 `GameState`。
- [x] 统一 `Unit` 类型，用 `Owner` 区分玩家单位和僵尸单位。
- [x] 准备、守家、结算、游戏结束四阶段循环。
- [x] 自动索敌、攻击、回蓝、寻路、死亡清格和胜负结算。
- [x] 多态技能：`PeaBurst`、`FumeLineCaster`、`SunHealer`。
- [x] 5 个商店位、阳光购买、刷新、人口升级。
- [x] 6 种 PvZ 羁绊：`shooter`、`nut`、`sun`、`healer`、`fungus`、`spike`。
- [x] 三合一升星、装备掉落/穿戴、存档/读档。
- [x] Qt GUI 展示草坪棋盘、单位图标、商店、装备栏、羁绊、详情面板。

## PvZ 内容

单位目录定义在 `src/core/Catalog.cpp`：

- 豌豆射手、双重射手、向日葵、双胞向日葵、坚果墙、高坚果。
- 小喷菇、大喷菇、忧郁菇、地刺、地刺王。

装备保持 PA 要求的四种基础效果：

- 植物营养剂：攻击力 +15。
- 南瓜护壳：最大生命 +150。
- 园艺手套：攻击间隔 -20%。
- 叶绿素：最大法力 -30。

僵尸波次由 `EncounterGenerator` 的模板控制：

- 第 1 波：普通僵尸。
- 第 2 波：普通僵尸 + 路障僵尸。
- 第 3 波：普通僵尸 + 路障僵尸 + 铁门僵尸。
- 第 4 波：读报僵尸 + 路障僵尸 + 铁门僵尸。
- 第 5 波及之后：橄榄球僵尸 + 小丑僵尸 + 混合精英。

## 资源管线

`pictures/` 只是临时素材池。正式 GUI 只依赖 `assets/`。

生成资源：

```powershell
$py='C:\Users\SeinoQ\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
& $py .\tools\build_assets.py
```

脚本会将 GIF 首帧转换为 96x96 透明 PNG，把装备/羁绊输出为 64x64 图标，并裁剪白天/夜晚棋盘背景。

## 文件结构

```text
src/core/     游戏核心：单位、棋盘、战斗、商店、装备、羁绊、存档
src/gui/      Qt Widgets GUI，只读取状态并调用 GameState API
tests/        三阶段断言测试
tools/        资源生成脚本
assets/       正式运行资源，由 tools/build_assets.py 生成
pictures/     临时素材池
```

## 构建与测试

本机可使用 Qt 自带 CMake：

```powershell
& 'D:\Apps\Qt\Tools\CMake_64\bin\cmake.exe' --build .\build --config Debug
```

运行测试时，`stage3_tests` 使用 `std::filesystem`，需要 Qt MinGW 运行时目录在 PATH 中：

```powershell
$env:PATH='D:\Apps\Qt\Tools\mingw1310_64\bin;' + $env:PATH
.\build\stage1_tests.exe
.\build\stage2_tests.exe
.\build\stage3_tests.exe
```

运行 GUI：

```powershell
$env:PATH='D:\Apps\Qt\6.11.1\mingw_64\bin;D:\Apps\Qt\Tools\mingw1310_64\bin;' + $env:PATH
.\build\synera_gui.exe
```

## AI 使用说明

AI 辅助用于规划 PvZ 主题替换、生成资源管线、同步核心逻辑/GUI/测试/文档。核心设计仍围绕
`GameState`：它是唯一改变游戏事实的位置；GUI 只展示状态并发出命令。验收时应重点能解释：

- `Board` / `Bench` 为什么只保存 `UnitId`。
- `Owner` 与 `traits` 的区别。
- 多态技能如何通过 `castSkill(SkillContext&)` 统一结算。
- 装备、星级、羁绊为什么从 `baseStats` 重算到 `effectiveStats`。
- `sun` 羁绊为什么在胜利结算中处理，而不是在属性重算中处理。
