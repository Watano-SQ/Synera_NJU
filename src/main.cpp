#include "core/GameState.h"
#include "core/Unit.h"

#include <iostream>
#include <memory>
#include <string>
#include <vector>

using namespace synera;

namespace {

// CLI 入口只作为一个最小可运行示例存在，真正提交给用户使用的是 Qt GUI。
// 这里仍然保留一条清晰的演示路径：创建单位 -> 放入备战区 -> 部署到棋盘 ->
// 生成敌人 -> 打印当前状态。这样即使没有打开图形界面，也能快速验证核心规则层可用。
std::unique_ptr<Unit> makePlayerUnit(const std::string& name, std::vector<std::string> traits) {
    // BasicUnit 没有主动技能，适合做命令行示例中的“干净样本”。
    // 参数顺序依次是：显示名、归属、生命、攻击、射程、法力上限、羁绊、贴图 key。
    // visualKey 仍然写成资源目录里的稳定键，方便 CLI 与 GUI 共用同一套目录约定。
    return std::make_unique<BasicUnit>(name, Owner::PlayerCtrl, 320, 35, 1, 60, std::move(traits),
                                       "units/peashooter");
}

void printUnit(const GameState& game, UnitId id) {
    // 所有外部展示都通过 GameState 查询 Unit 指针，避免调用方绕过 UnitManager 的所有权边界。
    const Unit* unit = game.unit(id);
    if (unit == nullptr) {
        // 打印函数是调试辅助，遇到已经不存在的 id 直接跳过，不把调试输出变成程序错误。
        return;
    }

    std::cout << "#" << id << " " << unit->name() << " owner=" << toString(unit->owner())
              << " hp=" << unit->hp() << "/" << unit->maxHp() << " atk=" << unit->atk()
              << " range=" << unit->range() << " mana=" << unit->mana() << "/"
              << unit->maxMana() << " at " << toString(unit->placement()) << "\n";
}

void printBench(const GameState& game) {
    std::cout << "\nBench:\n";
    for (std::size_t slot = 0; slot < game.bench().capacity(); ++slot) {
        std::cout << "  [" << slot << "] ";
        // Bench 只保存 UnitId，不拥有 Unit 对象；真正的单位数据仍由 GameState/UnitManager 提供。
        const auto occupant = game.bench().occupant(slot);
        if (occupant.has_value()) {
            printUnit(game, *occupant);
        } else {
            std::cout << "empty\n";
        }
    }
}

void printBoard(const GameState& game) {
    std::cout << "\nBoard occupancy:\n";
    for (int row = 0; row < game.board().rows(); ++row) {
        std::cout << "  row " << row << ": ";
        for (int col = 0; col < game.board().cols(); ++col) {
            // 棋盘打印故意使用紧凑的一字符阵营前缀，方便在控制台里观察站位和敌我半场。
            const auto occupant = game.board().occupant(Position{row, col});
            if (!occupant.has_value()) {
                std::cout << ". ";
                continue;
            }

            const Unit* unit = game.unit(*occupant);
            std::cout << (unit->owner() == Owner::PlayerCtrl ? "P" : "E") << *occupant << " ";
        }
        std::cout << "\n";
    }
}

void printActiveSet(const GameState& game, const std::string& label, const std::vector<UnitId>& ids) {
    std::cout << "\n" << label << ":\n";
    for (UnitId id : ids) {
        std::cout << "  ";
        printUnit(game, id);
    }
}

}  // namespace

int main() {
    GameState game;

    // addUnitToBench 会把 unique_ptr 的所有权交给 UnitManager，并在 Bench 中记录新 id。
    // 之后调用方只保存 UnitId，避免悬空指针或重复释放。
    const UnitId peashooter = game.addUnitToBench(makePlayerUnit("Peashooter", {"shooter"}));
    const UnitId sunflower = game.addUnitToBench(makePlayerUnit("Sunflower", {"sun", "healer"}));

    std::cout << "Created player units: #" << peashooter << ", #" << sunflower << "\n";

    // 玩家只能部署到自己的下半场；第二次调用故意选择敌方半场，用来展示规则层会拒绝非法位置。
    const bool placedPeashooter = game.deployFromBench(0, Position{7, 3});
    const bool illegalSunflowerPlacement = game.deployFromBench(1, Position{0, 0});
    const bool placedSunflower = game.deployFromBench(1, Position{6, 4});

    std::cout << "Deploy peashooter to player half: " << (placedPeashooter ? "ok" : "failed") << "\n";
    std::cout << "Deploy sunflower to enemy half: " << (illegalSunflowerPlacement ? "ok" : "rejected")
              << "\n";
    std::cout << "Deploy sunflower to player half: " << (placedSunflower ? "ok" : "failed") << "\n";

    // 敌人生成由 EncounterGenerator 根据回合数写入 GameState。
    // CLI 示例只生成不进入完整战斗循环，便于把“布阵状态”打印出来给读代码的人检查。
    const std::vector<UnitId> enemies = game.generateEnemiesForRound(1);
    std::cout << "Generated " << enemies.size() << " enemies for wave "
              << game.player().currentRound() << ".\n";

    printBench(game);
    printBoard(game);
    printActiveSet(game, "Player combat units", game.activePlayerUnits());
    printActiveSet(game, "Enemy combat units", game.activeEnemyUnits());

    return 0;
}
