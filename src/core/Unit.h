#pragma once

#include "Types.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace synera {

class SkillContext;

// Unit 是玩家植物和敌人僵尸共用的运行时对象。
// 它保存生命、法力、星级、装备、羁绊、位置等状态；具体技能由派生类实现。
class Unit {
public:
    Unit(std::string name,
         Owner owner,
         int maxHp,
         int atk,
         int range,
         int maxMana,
         std::vector<std::string> traits = {},
         std::string visualKey = {});
    Unit(std::string definitionId,
         std::string name,
         Owner owner,
         UnitStats baseStats,
         std::vector<std::string> traits,
         std::string visualKey,
         std::string factoryKey);
    virtual ~Unit() = default;

    Unit(const Unit&) = delete;
    Unit& operator=(const Unit&) = delete;

    const std::string& name() const;
    const std::string& definitionId() const;
    const std::string& visualKey() const;
    const std::string& factoryKey() const;
    // 下面这些访问器把“基础数值”和“计算后数值”统一封装起来。
    // 外部战斗代码读取 hp/atk/range 等当前有效值即可，不需要知道星级、装备和羁绊如何叠加。
    Owner owner() const;
    const std::vector<std::string>& traits() const;
    int star() const;
    std::uint64_t acquireSeq() const;
    const UnitStats& baseStats() const;
    const UnitStats& effectiveStats() const;
    std::optional<ItemId> equippedItemId() const;

    int hp() const;
    int maxHp() const;
    int atk() const;
    int range() const;
    int maxMana() const;
    int initialMana() const;
    int manaRegenPerSecond() const;
    int skillManaCost() const;
    int skillCooldownTicks() const;
    int attackInterval() const;
    int moveInterval() const;
    int mana() const;
    UnitState state() const;
    const Placement& placement() const;

    bool isAlive() const;
    // setHp/setMana 会在实现中做范围裁剪，保证生命和法力不会越过 0 或对应上限。
    void setHp(int hp);
    void setMana(int mana);
    // acquireSeq 用来在三合一时确定“最新获得”的单位，避免同名单位合成时结果随机。
    void setStar(int star);
    void setAcquireSeq(std::uint64_t acquireSeq);
    void setBaseStats(UnitStats stats);
    void setEffectiveStats(UnitStats stats);
    void setEquippedItemId(std::optional<ItemId> itemId);
    void setState(UnitState state);
    // Unit 只记录自己认为的位置；Board/Bench 的占用表必须由 GameState 同步更新。
    void setPlacement(Placement placement);

    // archetype 用于展示和存档调试，castSkill 由战斗循环在法力满时调用。
    virtual std::string archetype() const;
    virtual bool hasActiveSkill() const;
    virtual void castSkill(SkillContext& context);

private:
    // definitionId/factoryKey/visualKey 都是稳定键；显示名可能包含历史乱码，不参与逻辑判断。
    std::string definitionId_;
    std::string name_;
    std::string visualKey_;
    std::string factoryKey_;
    Owner owner_;
    int hp_;
    int mana_ = 0;
    int star_ = 1;
    std::uint64_t acquireSeq_ = 0;
    UnitStats baseStats_;
    UnitStats effectiveStats_;
    std::optional<ItemId> equippedItemId_;
    // traits 存放稳定的 traitId，例如 shooter/sun/healer；GUI 展示名由目录定义决定。
    std::vector<std::string> traits_;
    // state_ 既给规则层判断死亡/施法，也给 GUI 画不同状态提示。
    UnitState state_ = UnitState::Idle;
    Placement placement_ = Placement::none();
};

// 无特殊主动技能的默认单位。
class BasicUnit : public Unit {
public:
    using Unit::Unit;

    std::string archetype() const override;
    bool hasActiveSkill() const override;
    void castSkill(SkillContext& context) override;
};

// 对当前目标造成一次高额单体伤害。
class PeaBurst : public Unit {
public:
    using Unit::Unit;

    std::string archetype() const override;
    bool hasActiveSkill() const override;
    void castSkill(SkillContext& context) override;
};

// 对目标所在整行的敌人造成范围伤害。
class FumeLineCaster : public Unit {
public:
    using Unit::Unit;

    std::string archetype() const override;
    bool hasActiveSkill() const override;
    void castSkill(SkillContext& context) override;
};

// 治疗自己周围一定半径内的友军。
class SunHealer : public Unit {
public:
    using Unit::Unit;

    std::string archetype() const override;
    bool hasActiveSkill() const override;
    void castSkill(SkillContext& context) override;
};

}  // namespace synera
