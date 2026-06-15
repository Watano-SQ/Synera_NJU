# Synera: PvZ Auto Arena

这是一个使用 C++20 和 Qt Widgets 实现的单机 PvE 自走棋原型。项目保留 `synera` 命名空间、CMake target 和 `SYNERA_SAVE 1` 存档格式，玩法内容已切换为植物大战僵尸主题。

## 当前内容

- 8x8 棋盘、玩家半场/敌方半场、Bench 与 Board 拖拽部署。
- 准备、守家、结算、游戏结束四阶段循环。
- 阳光商店、人口升级、装备掉落与装备栏。
- 三合一升星：二星只改变属性倍率和显示贴图，不改变 `definitionId`、`traits`、`factoryKey` 或技能类型。
- Qt GUI 使用 PvZ 资源：日/夜半场背景、种子包商店卡牌、Zombieline 装备托盘、顶部脑子/阳光/波次图标和风格化 Bench。

## 植物单位模型

商店只出售基础植物：

- `peashooter` / 豌豆射手
- `sunflower` / 向日葵
- `wallnut` / 坚果墙
- `puffshroom` / 小喷菇
- `fumeshroom` / 大喷菇
- `spikeweed` / 地刺

二星视觉形态不进入 `unitCatalog()`，也不会出现在 `shopOffers()`：

- `peashooter -> units/repeater`
- `sunflower -> units/twin_sunflower`
- `wallnut -> units/tallnut`
- `puffshroom -> units/scaredyshroom`
- `fumeshroom -> units/gloomshroom`
- `spikeweed -> units/spikerock`

显示单位实例时使用 `displayVisualKey(const Unit&)`。敌人始终使用自身 `visualKey`；玩家单位按 `star1VisualKey` / `star2VisualKey` 显示。

## 资源管线

`pictures/` 是临时素材池，运行时只依赖 `assets/`。

```powershell
python .\tools\build_assets.py
```

脚本会生成：

- `assets/units/`：单位视觉资源池，包含基础单位和二星贴图，不等于商店单位池。
- `assets/shop_cards/`：基础植物的商店种子包卡牌，上半为可购买态，下半为禁用态。
- `assets/ui/`：商店标题、按钮、脑子、阳光、旗帜进度、`zombieline.jpg` 装备托盘。
- `assets/backgrounds/day_board.jpg` 和 `assets/backgrounds/night_board.jpg`：同屏分半场使用，玩家半场白天，敌方半场夜晚。

暂不接入 `Zombie_football_helmet.png`、`Zombie_jackbox_box.png`、`chocolate.png`、`1.png`、`2.png` 等杂项素材。

## 构建与测试

```powershell
& 'D:\Apps\Qt\Tools\CMake_64\bin\cmake.exe' --build .\build --config Debug
.\build\stage1_tests.exe
.\build\stage2_tests.exe
.\build\stage3_tests.exe
```

运行 GUI：

```powershell
$env:PATH='D:\Apps\Qt\6.11.1\mingw_64\bin;D:\Apps\Qt\Tools\mingw1310_64\bin;' + $env:PATH
.\build\synera_gui.exe
```
