#include "MainWindow.h"

#include "PaintUtils.h"
#include "core/Catalog.h"
#include "core/Unit.h"

#include <QCoreApplication>
#include <QDebug>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QPainter>
#include <QPixmap>
#include <QScrollArea>
#include <QSize>
#include <QSizePolicy>
#include <QStatusBar>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

namespace synera::gui {
namespace {

std::unique_ptr<Unit> makeCatalogUnit(const std::string& definitionId) {
    // 初始阵容从 Catalog 创建，避免 GUI 示例绕过正式单位定义。
    const UnitDefinition* definition = findUnitDefinition(definitionId);
    return definition != nullptr ? createUnitFromDefinition(*definition, Owner::PlayerCtrl) : nullptr;
}

void styleStatusValue(QLabel* label) {
    // 顶栏数值统一加粗，避免每个 label 单独设置样式。
    QFont font = label->font();
    font.setBold(true);
    font.setPointSize(std::max(10, font.pointSize() + 1));
    label->setFont(font);
    label->setMinimumWidth(34);
}

QWidget* makeStatusGroup(QWidget* parent, QWidget* icon, QLabel* value) {
    // 图标 + 数值的小组件用于生命、波次等状态组。
    auto* group = new QWidget(parent);
    group->setFixedHeight(34);
    group->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    auto* layout = new QHBoxLayout(group);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(5);
    layout->addWidget(icon, 0, Qt::AlignVCenter);
    layout->addWidget(value, 0, Qt::AlignVCenter);
    return group;
}

void styleActionButton(QPushButton* button) {
    // 顶栏动作按钮保持固定尺寸，避免文本变化导致布局跳动。
    button->setFixedHeight(30);
    button->setFixedWidth(88);
    button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    button->setStyleSheet(
        "QPushButton { background: #684a2a; border: 1px solid #2b1d12; color: #fff0bd; font-weight: 700; }"
        "QPushButton:disabled { background: #4a4036; color: #a79a87; }"
        "QPushButton:hover { background: #785734; }");
}

}  // namespace

class StatusIconWidget : public QWidget {
public:
    StatusIconWidget(AssetManager* assets, std::string visualKey, QSize fixedSize, QWidget* parent = nullptr)
        : QWidget(parent), assets_(assets), visualKey_(std::move(visualKey)) {
        setFixedSize(fixedSize);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    }

protected:
    void paintEvent(QPaintEvent*) override {
        // 仅绘制一张资源图，资源缺失时保持透明。
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        const QPixmap* pixmap = assets_ != nullptr ? assets_->pixmapFor(visualKey_) : nullptr;
        if (pixmap != nullptr) {
            drawPixmapAspectFit(painter, rect(), *pixmap);
        }
    }

private:
    AssetManager* assets_;
    std::string visualKey_;
};

class FlagMeterWidget : public QWidget {
public:
    explicit FlagMeterWidget(AssetManager* assets, QWidget* parent = nullptr) : QWidget(parent), assets_(assets) {
        setFixedSize(120, 16);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    }

    void setProgress(int value, int maximum) {
        // value/maximum 表示当前波次进度，绘制时再换算为宽度。
        value_ = std::max(0, value);
        maximum_ = std::max(1, maximum);
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        const QRect meter = rect();
        const QPixmap* empty = assets_ != nullptr ? assets_->pixmapFor("ui/flag_meter_empty") : nullptr;
        const QPixmap* full = assets_ != nullptr ? assets_->pixmapFor("ui/flag_meter_full") : nullptr;

        if (empty != nullptr) {
            drawPixmapAspectFit(painter, meter, *empty);
        } else {
            painter.setPen(QPen(QColor("#3a332a"), 1));
            painter.setBrush(QColor("#5e554a"));
            painter.drawRoundedRect(meter.adjusted(0, 0, -1, -1), 4, 4);
        }

        const int fillWidth = meter.width() * std::clamp(value_, 0, maximum_) / maximum_;
        // 有满格素材时裁剪满格图；没有素材时用纯色填充。
        if (fillWidth <= 0) {
            return;
        }

        if (full != nullptr) {
            const QRect fitted = aspectFitRect(meter, full->size());
            const int fittedFillWidth = fitted.width() * std::clamp(value_, 0, maximum_) / maximum_;
            if (fittedFillWidth > 0) {
                const QRect target(fitted.left(), fitted.top(), fittedFillWidth, fitted.height());
                const QRect source(0, 0, full->width() * fittedFillWidth / std::max(1, fitted.width()), full->height());
                painter.drawPixmap(target, *full, source);
            }
        } else {
            painter.fillRect(QRect(meter.left(), meter.top(), fillWidth, meter.height()), QColor("#ffd34e"));
        }
    }

private:
    AssetManager* assets_;
    int value_ = 1;
    int maximum_ = 5;
};

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      placementController_(&game_),
      assets_(QString::fromUtf8(SYNERA_PROJECT_ROOT)) {
    // 先初始化 GameState，再创建依赖它读取尺寸和状态的控件。
    initializeGame();

    auto* central = new QWidget(this);
    auto* root = new QVBoxLayout(central);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(8);

    auto* statusBarWidget = new QWidget(central);
    statusBarWidget->setFixedHeight(44);
    statusBarWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    auto* statusRow = new QHBoxLayout(statusBarWidget);
    statusRow->setContentsMargins(8, 4, 8, 4);
    statusRow->setSpacing(8);

    brainIconWidget_ = new StatusIconWidget(&assets_, "ui/brain", QSize(28, 28), statusBarWidget);
    hpValueLabel_ = new QLabel(statusBarWidget);
    flagMeterWidget_ = new FlagMeterWidget(&assets_, statusBarWidget);
    roundValueLabel_ = new QLabel(statusBarWidget);
    levelValueLabel_ = new QLabel(statusBarWidget);
    populationValueLabel_ = new QLabel(statusBarWidget);
    phaseLabel_ = new QLabel(statusBarWidget);
    upgradeButton_ = new QPushButton(QString::fromUtf8("升级人口"), statusBarWidget);
    saveButton_ = new QPushButton(QString::fromUtf8("存档"), statusBarWidget);
    loadButton_ = new QPushButton(QString::fromUtf8("读档"), statusBarWidget);
    startCombatButton_ = new QPushButton(QString::fromUtf8("开始守家"), statusBarWidget);
    resolveButton_ = new QPushButton(QString::fromUtf8("结算"), statusBarWidget);

    styleStatusValue(hpValueLabel_);
    styleStatusValue(roundValueLabel_);
    styleStatusValue(levelValueLabel_);
    styleStatusValue(populationValueLabel_);
    styleStatusValue(phaseLabel_);

    styleActionButton(upgradeButton_);
    styleActionButton(saveButton_);
    styleActionButton(loadButton_);
    styleActionButton(startCombatButton_);
    styleActionButton(resolveButton_);

    auto* hpGroup = makeStatusGroup(statusBarWidget, brainIconWidget_, hpValueLabel_);
    auto* waveGroup = new QWidget(statusBarWidget);
    waveGroup->setFixedHeight(34);
    waveGroup->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    auto* waveLayout = new QHBoxLayout(waveGroup);
    waveLayout->setContentsMargins(0, 0, 0, 0);
    waveLayout->setSpacing(6);
    waveLayout->addWidget(flagMeterWidget_, 0, Qt::AlignVCenter);
    waveLayout->addWidget(roundValueLabel_, 0, Qt::AlignVCenter);

    statusRow->addWidget(hpGroup);
    statusRow->addWidget(waveGroup);
    statusRow->addSpacing(8);
    statusRow->addWidget(phaseLabel_);
    statusRow->addWidget(levelValueLabel_);
    statusRow->addWidget(populationValueLabel_);
    statusRow->addStretch();
    statusRow->addWidget(upgradeButton_);
    statusRow->addWidget(saveButton_);
    statusRow->addWidget(loadButton_);
    statusRow->addWidget(startCombatButton_);
    statusRow->addWidget(resolveButton_);
    root->addWidget(statusBarWidget);

    auto* contentRow = new QHBoxLayout();
    contentRow->setContentsMargins(0, 0, 0, 0);
    contentRow->setSpacing(16);

    auto* leftContainer = new QWidget(central);
    leftContainer->setFixedWidth(536);
    leftContainer->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    auto* leftColumn = new QVBoxLayout(leftContainer);
    leftColumn->setContentsMargins(14, 0, 14, 0);
    leftColumn->setSpacing(8);

    auto* rightContainer = new QWidget(central);
    rightContainer->setFixedWidth(780);
    rightContainer->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    auto* rightColumn = new QVBoxLayout(rightContainer);
    rightColumn->setContentsMargins(0, 0, 0, 0);
    rightColumn->setSpacing(8);

    boardWidget_ = new BoardWidget(&game_, &assets_, leftContainer);
    benchWidget_ = new BenchWidget(&game_, &assets_, leftContainer);
    inspectorPanel_ = new InspectorPanel(&game_, &assets_, rightContainer);
    shopPanel_ = new ShopPanel(&game_, &assets_, rightContainer);
    equipmentPanel_ = new EquipmentPanel(&game_, &assets_, rightContainer);
    synergyPanel_ = new SynergyPanel(&game_, &assets_, rightContainer);

    // 左侧是棋盘和 Bench，右侧是商店、装备、羁绊和单位详情。
    leftColumn->addWidget(boardWidget_, 0, Qt::AlignHCenter | Qt::AlignTop);
    leftColumn->addWidget(benchWidget_, 0, Qt::AlignHCenter | Qt::AlignTop);
    leftColumn->addStretch();

    auto* synergyScroll = new QScrollArea(rightContainer);
    synergyScroll->setWidgetResizable(true);
    synergyScroll->setFixedHeight(176);
    synergyScroll->setFrameShape(QFrame::NoFrame);
    synergyScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    synergyScroll->setWidget(synergyPanel_);

    auto* inspectorScroll = new QScrollArea(rightContainer);
    inspectorScroll->setWidgetResizable(true);
    inspectorScroll->setMinimumHeight(220);
    inspectorScroll->setFrameShape(QFrame::NoFrame);
    inspectorScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    inspectorScroll->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    inspectorScroll->setWidget(inspectorPanel_);

    rightColumn->addWidget(shopPanel_, 0, Qt::AlignTop);
    rightColumn->addWidget(equipmentPanel_, 0, Qt::AlignTop);
    rightColumn->addWidget(synergyScroll, 0, Qt::AlignTop);
    rightColumn->addWidget(inspectorScroll, 1);

    contentRow->addWidget(leftContainer, 0, Qt::AlignTop);
    contentRow->addWidget(rightContainer, 0, Qt::AlignTop);
    contentRow->addStretch();
    root->addLayout(contentRow, 1);
    setCentralWidget(central);

    boardWidget_->setUnitSelectedCallback([this](std::optional<UnitId> id) { setSelectedUnit(id); });
    benchWidget_->setUnitSelectedCallback([this](std::optional<UnitId> id) { setSelectedUnit(id); });
    boardWidget_->setBoardDropCallback([this](const UnitDragData& data, Position target) {
        applyPlacementResult(placementController_.dropOnBoard(data, target));
    });
    benchWidget_->setBenchDropCallback([this](const UnitDragData& data, int targetSlot) {
        applyPlacementResult(placementController_.dropOnBench(data, targetSlot));
    });
    shopPanel_->setPurchaseCallback([this](std::size_t index) { purchaseShopSlot(index); });
    shopPanel_->setRefreshCallback([this]() { refreshShop(); });
    equipmentPanel_->setItemSelectedCallback([this](std::optional<ItemId> itemId) { setSelectedItem(itemId); });
    // 所有按钮最终都调用 GameState 命令，然后统一 refreshFromState。
    connect(upgradeButton_, &QPushButton::clicked, this, &MainWindow::upgradePopulation);
    connect(saveButton_, &QPushButton::clicked, this, &MainWindow::saveGame);
    connect(loadButton_, &QPushButton::clicked, this, &MainWindow::loadGame);
    connect(startCombatButton_, &QPushButton::clicked, this, &MainWindow::startCombat);
    connect(resolveButton_, &QPushButton::clicked, this, &MainWindow::resolveRound);

    combatTimer_ = new QTimer(this);
    combatTimer_->setInterval(16);
    // 约 60 FPS 推进战斗 tick，直到 GameState 进入 Resolve 或命令失败。
    connect(combatTimer_, &QTimer::timeout, this, &MainWindow::advanceCombat);

    setWindowTitle("Synera - PvZ Auto Arena");
    setMinimumSize(1360, 760);
    resize(1360, 760);
    startBackgroundMusic();
    refreshFromState();
    statusBar()->showMessage("Ready");
}

MainWindow::~MainWindow() = default;

void MainWindow::refreshFromState() {
    // 被合成或读档删除的单位可能仍处于选中状态，这里先清理悬空选择。
    if (selectedUnit_.has_value() && game_.unit(*selectedUnit_) == nullptr) {
        selectedUnit_.reset();
    }

    hpValueLabel_->setText(QString::number(game_.player().hp()));
    roundValueLabel_->setText(QString("Wave %1").arg(game_.player().currentRound()));
    const int maxRound = std::max(1, game_.combatConfig().maxRound);
    flagMeterWidget_->setProgress(std::clamp(game_.player().currentRound(), 1, maxRound), maxRound);
    levelValueLabel_->setText(QString("Level %1").arg(game_.player().level()));
    populationValueLabel_->setText(
        QString("Pop %1/%2").arg(game_.deployedPlayerUnitCount()).arg(game_.player().unitCap()));
    phaseLabel_->setText("Phase: " + phaseText());

    const bool canDrag = game_.phase() == GamePhase::Prep;
    const bool canManage = game_.phase() == GamePhase::Prep;
    // 根据阶段控制按钮和拖拽能力，避免 UI 发出核心层会拒绝的操作。
    startCombatButton_->setEnabled(game_.phase() == GamePhase::Prep && game_.matchResult() == MatchResult::Ongoing);
    resolveButton_->setEnabled(game_.phase() == GamePhase::Resolve);
    upgradeButton_->setEnabled(canManage);
    saveButton_->setEnabled(game_.phase() != GamePhase::Combat);
    loadButton_->setEnabled(game_.phase() != GamePhase::Combat);
    boardWidget_->setDragEnabled(canDrag);
    benchWidget_->setDragEnabled(canDrag);
    boardWidget_->setSelectedUnit(selectedUnit_);
    benchWidget_->setSelectedUnit(selectedUnit_);
    inspectorPanel_->setSelectedUnit(selectedUnit_);
    boardWidget_->refreshFromState();
    benchWidget_->refreshFromState();
    inspectorPanel_->refreshFromState();
    shopPanel_->refreshFromState();
    equipmentPanel_->setSelectedItem(selectedItem_);
    equipmentPanel_->refreshFromState();
    synergyPanel_->refreshFromState();
}

void MainWindow::initializeGame() {
    // 新游戏给 8 阳光和 3 个初始单位，并默认部署一个豌豆射手。
    game_.player().setGold(8);
    game_.player().setLevel(1);
    game_.player().setUnitCap(3);

    const UnitId starter = game_.addUnitToBench(makeCatalogUnit("peashooter"));
    game_.addUnitToBench(makeCatalogUnit("sunflower"));
    game_.addUnitToBench(makeCatalogUnit("wallnut"));
    game_.deployFromBench(0, Position{7, 3}, PlacementPolicy::Reject);
    selectedUnit_ = starter;
}

void MainWindow::setSelectedUnit(std::optional<UnitId> unitId) {
    // 若当前先选了装备，再点击单位，则把这次点击解释为穿戴装备。
    if (unitId.has_value() && tryEquipSelectedItem(*unitId)) {
        selectedUnit_ = unitId;
        refreshFromState();
        return;
    }
    selectedUnit_ = unitId;
    refreshFromState();
}

void MainWindow::applyPlacementResult(const PlacementResult& result) {
    statusBar()->showMessage(result.message, 2500);
    refreshFromState();
}

void MainWindow::purchaseShopSlot(std::size_t index) {
    // 下列命令函数都把核心层返回消息展示到状态栏，并刷新所有面板。
    const ActionResult result = game_.purchaseShopSlot(index);
    statusBar()->showMessage(QString::fromStdString(result.message), 2500);
    refreshFromState();
}

void MainWindow::refreshShop() {
    const ActionResult result = game_.refreshShop();
    statusBar()->showMessage(QString::fromStdString(result.message), 2500);
    refreshFromState();
}

void MainWindow::upgradePopulation() {
    const ActionResult result = game_.upgradePopulation();
    statusBar()->showMessage(QString::fromStdString(result.message), 2500);
    refreshFromState();
}

void MainWindow::setSelectedItem(std::optional<ItemId> itemId) {
    selectedItem_ = itemId;
    refreshFromState();
}

bool MainWindow::tryEquipSelectedItem(UnitId unitId) {
    if (!selectedItem_.has_value()) {
        return false;
    }
    // 穿戴成功后清空选中装备，失败则保留选择让玩家可继续尝试。
    const ActionResult result = game_.equipItem(*selectedItem_, unitId);
    statusBar()->showMessage(QString::fromStdString(result.message), 2500);
    if (result.success) {
        selectedItem_.reset();
    }
    return result.success;
}

void MainWindow::saveGame() {
    // 保存和读档都禁止在 Combat 中触发，按钮状态在 refreshFromState 中控制。
    const QString path = QFileDialog::getSaveFileName(this, "Save Synera", "synera_save.txt", "Synera Save (*.txt)");
    if (path.isEmpty()) {
        return;
    }
    const ActionResult result = game_.saveToFile(path.toStdString());
    statusBar()->showMessage(QString::fromStdString(result.message), 2500);
    refreshFromState();
}

void MainWindow::loadGame() {
    // 读档前停止战斗计时器，避免读档过程中 tickCombat 修改状态。
    const QString path = QFileDialog::getOpenFileName(this, "Load Synera", QString(), "Synera Save (*.txt)");
    if (path.isEmpty()) {
        return;
    }
    combatTimer_->stop();
    const ActionResult result = game_.loadFromFile(path.toStdString());
    if (result.success) {
        selectedUnit_.reset();
        selectedItem_.reset();
    }
    statusBar()->showMessage(QString::fromStdString(result.message), 2500);
    refreshFromState();
}

void MainWindow::startCombat() {
    // startCombat 成功进入 Combat 后才启动计时器。
    const ActionResult result = game_.startCombat();
    statusBar()->showMessage(QString::fromStdString(result.message), 2500);
    if (result.success && game_.phase() == GamePhase::Combat) {
        combatTimer_->start();
    }
    refreshFromState();
}

void MainWindow::advanceCombat() {
    // 每次 timer timeout 推进一个核心战斗 tick，并在进入 Resolve 时停表。
    const ActionResult result = game_.tickCombat();
    if (!result.success) {
        combatTimer_->stop();
        statusBar()->showMessage(QString::fromStdString(result.message), 2500);
    } else if (game_.phase() == GamePhase::Resolve) {
        combatTimer_->stop();
        statusBar()->showMessage(QString::fromStdString(result.message), 2500);
    }
    refreshFromState();
}

void MainWindow::resolveRound() {
    // 手动结算按钮用于 Resolve 阶段，把玩家带回 Prep 或 GameOver。
    combatTimer_->stop();
    const ActionResult result = game_.resolveRound();
    statusBar()->showMessage(QString::fromStdString(result.message), 2500);
    refreshFromState();
}

void MainWindow::startBackgroundMusic() {
    if (qEnvironmentVariable("SYNERA_DISABLE_BGM") == "1") {
        return;
    }

    const QString appAssetPath = QCoreApplication::applicationDirPath() + "/assets/audio/grasswalk.mp3";
    const QString projectAssetPath = QString::fromUtf8(SYNERA_PROJECT_ROOT) + "/assets/audio/grasswalk.mp3";
    const QString musicPath = QFileInfo::exists(appAssetPath) ? appAssetPath : projectAssetPath;
    if (!QFileInfo::exists(musicPath)) {
        const QString message = "Background music not found: " + musicPath;
        qWarning() << message;
        statusBar()->showMessage(message, 3500);
        return;
    }

    backgroundAudio_ = new QAudioOutput(this);
    backgroundAudio_->setVolume(0.45);

    backgroundMusic_ = new QMediaPlayer(this);
    backgroundMusic_->setAudioOutput(backgroundAudio_);
    backgroundMusic_->setLoops(QMediaPlayer::Infinite);
    backgroundMusic_->setSource(QUrl::fromLocalFile(musicPath));

    connect(backgroundMusic_, &QMediaPlayer::errorOccurred, this,
            [this](QMediaPlayer::Error, const QString& errorString) {
                const QString message = "Background music error: " + errorString;
                qWarning() << message;
                statusBar()->showMessage(message, 3500);
            });

    backgroundMusic_->play();
}

QString MainWindow::phaseText() const {
    switch (game_.phase()) {
        case GamePhase::Prep:
            return "Prep";
        case GamePhase::Combat:
            return "Combat";
        case GamePhase::Resolve:
            return "Resolve";
        case GamePhase::GameOver:
            return "Game Over";
    }
    return QString::fromStdString(toString(game_.phase()));
}

}  // namespace synera::gui
