#pragma once

#include "core/GameState.h"

#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

#include <vector>

namespace synera::gui {

class AssetManager;

// 羁绊面板读取 GameState 当前 activeSynergies，展示计数、阈值和效果描述。
class SynergyPanel : public QWidget {
public:
    explicit SynergyPanel(const GameState* game, AssetManager* assets, QWidget* parent = nullptr);

    void refreshFromState();

private:
    const GameState* game_;
    AssetManager* assets_;
    QVBoxLayout* listLayout_ = nullptr;
};

}  // namespace synera::gui
