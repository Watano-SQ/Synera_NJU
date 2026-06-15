#pragma once

#include "core/GameState.h"

#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

#include <vector>

namespace synera::gui {

class AssetManager;

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
