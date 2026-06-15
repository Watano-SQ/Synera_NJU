#pragma once

#include "core/GameState.h"

#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

#include <vector>

namespace synera::gui {

class SynergyPanel : public QWidget {
public:
    explicit SynergyPanel(const GameState* game, QWidget* parent = nullptr);

    void refreshFromState();

private:
    const GameState* game_;
    QVBoxLayout* listLayout_ = nullptr;
};

}  // namespace synera::gui
