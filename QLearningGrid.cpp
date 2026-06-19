#include "QLearningGrid.h"

#include "LdtkLoader.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <queue>
#include <stdexcept>

//========================================
// QLearningGrid 実装
//========================================
// Q テーブルの更新、マップ状態、履歴集計をこのファイルでまとめて扱います。
// 外部からは「学習を進める」「状態を読む」という形で使えるように整理しています。

namespace {

//========================================
// ローカル補助関数
//========================================

constexpr int kTacticalBucketCount = 3;
constexpr int kHeadquartersStateCount =
    kTacticalBucketCount *
    kTacticalBucketCount *
    kTacticalBucketCount *
    kTacticalBucketCount *
    kTacticalBucketCount;
constexpr int kDivisionStateCount =
    kBattleLaneCount *
    kHeadquartersCommandCount *
    kTacticalBucketCount *
    kTacticalBucketCount *
    kTacticalBucketCount;

/* UTF-8 文字列を簡易ワイド化 */
std::wstring ToWide(const std::string& text) {
    return std::wstring(text.begin(), text.end());
}

/* IntGrid 値を Tile へ変換 */
Tile ConvertIntGridValue(int value) {
    switch (value) {
    case 1:
        return Tile::Wall;
    case 2:
        return Tile::Goal;
    case 3:
        return Tile::Pit;
    case 4:
        return Tile::Start;
    default:
        return Tile::Empty;
    }
}

/* Start 識別子判定 */
bool IsStartIdentifier(const std::string& identifier) {
    return identifier == "Start";
}

/* Goal 識別子判定 */
bool IsGoalIdentifier(const std::string& identifier) {
    return identifier == "Goal";
}

/* Pit 識別子判定 */
bool IsPitIdentifier(const std::string& identifier) {
    return identifier == "Pit";
}

/* Wall 識別子判定 */
bool IsWallIdentifier(const std::string& identifier) {
    return identifier == "Wall";
}

/* Enemy 識別子判定 */
bool IsEnemyIdentifier(const std::string& identifier) {
    return identifier == "Enemy" || identifier == "EnemyStart";
}

/* Battle Unit 識別子判定 */
bool TryParseBattleUnitIdentifier(
    const std::string& identifier,
    UnitFaction& faction,
    UnitClass& unitClass) {
    if (identifier == "PlayerCavalry") {
        faction = UnitFaction::Player;
        unitClass = UnitClass::Cavalry;
        return true;
    }

    if (identifier == "PlayerInfantry") {
        faction = UnitFaction::Player;
        unitClass = UnitClass::Infantry;
        return true;
    }

    if (identifier == "PlayerArcher") {
        faction = UnitFaction::Player;
        unitClass = UnitClass::Archer;
        return true;
    }

    if (identifier == "EnemyCavalry") {
        faction = UnitFaction::Enemy;
        unitClass = UnitClass::Cavalry;
        return true;
    }

    if (identifier == "EnemyInfantry" || IsEnemyIdentifier(identifier)) {
        faction = UnitFaction::Enemy;
        unitClass = UnitClass::Infantry;
        return true;
    }

    if (identifier == "EnemyArcher") {
        faction = UnitFaction::Enemy;
        unitClass = UnitClass::Archer;
        return true;
    }

    return false;
}

/* 兵科ごとの既定人数 */
int DefaultUnitCount(UnitClass unitClass) {
    switch (unitClass) {
    case UnitClass::Cavalry:
        return 60;
    case UnitClass::Infantry:
        return 100;
    case UnitClass::Archer:
        return 70;
    default:
        break;
    }

    return 80;
}

/* General識別子判定 */
bool IsGeneralIdentifier(const std::string& identifier) {
    return identifier == "PlayerGeneral" || identifier == "EnemyGeneral" || identifier == "General";
}

/* Generalから陣営を取り出す */
UnitFaction GeneralFaction(const std::string& identifier) {
    if (identifier == "EnemyGeneral") {
        return UnitFaction::Enemy;
    }
    return UnitFaction::Player;
}

/* 相手陣営を返す */
UnitFaction OppositeFaction(UnitFaction faction) {
    return faction == UnitFaction::Player ? UnitFaction::Enemy : UnitFaction::Player;
}

}  // namespace

//========================================
// 生成とマップ読み込み
//========================================

QLearningGrid::QLearningGrid() : rng_(std::random_device{}()) {
    /* 既定の LDtk パス設定 */
    configuredLdtkPath_ = std::filesystem::path("maps") / "qlearning_demo.ldtk";
    configuredLevelIndex_ = 0;

    /* まず組み込みマップで初期化 */
    ResetMap();
    ResetLearningState();

    /* 外部マップがあれば上書きで読み込む */
    try {
        if (std::filesystem::exists(configuredLdtkPath_)) {
            LoadMapFromLdtk(configuredLdtkPath_, configuredLevelIndex_);
        }
    } catch (const std::exception& exception) {
        /* 外部マップが壊れていても起動自体は続けられるよう、既定マップへ切り替えます。 */
        ResetMap();
        ResetLearningState();
        usingLdtkMap_ = false;
        loadedLevelName_ = "BuiltIn";
        std::cout << "LDtk map load skipped: " << exception.what() << '\n';
    }
}

void QLearningGrid::ReloadMapFromLdtk() {
    /* パス未設定は再読み込みできない */
    if (configuredLdtkPath_.empty()) {
        throw std::runtime_error("LDtk file path is not configured.");
    }

    LoadMapFromLdtk(configuredLdtkPath_, configuredLevelIndex_);
}

void QLearningGrid::LoadMapFromLdtk(
    const std::filesystem::path& filePath,
    int levelIndex) {
    //========================================
    // プロジェクト読み込み
    //========================================

    const LdtkProjectData project = LoadLdtkProject(filePath);
    if (levelIndex < 0 || levelIndex >= static_cast<int>(project.levels.size())) {
        throw std::runtime_error("LDtk level index is out of range.");
    }

    //========================================
    // 対象レベル抽出
    //========================================

    const LdtkLevelData& level = project.levels[static_cast<std::size_t>(levelIndex)];

    //========================================
    // 内部マップへ反映
    //========================================

    ApplyLoadedMap(
        level.identifier,
        level.gridWidth,
        level.gridHeight,
        level.intGridCsv,
        level.entities);

    configuredLdtkPath_ = filePath;
    configuredLevelIndex_ = levelIndex;
    usingLdtkMap_ = true;

    /* Q テーブルと履歴は読み込み直し時にリセット */
    ResetLearningState();
}

//========================================
// 学習更新
//========================================

void QLearningGrid::Train(int steps) {
    /* 指定回数だけ内部 Step を回す */
    for (int index = 0; index < steps; ++index) {
        Step();
    }
}

//========================================
// 公開参照
//========================================

Tile QLearningGrid::GetTile(int x, int y) const {
    return tiles_[CellIndex({x, y})];
}

GridPoint QLearningGrid::GetAgent() const {
    return agent_;
}

GridPoint QLearningGrid::GetEnemy() const {
    if (enemies_.empty()) {
        return start_;
    }
    return SelectPrimaryEnemy(agent_, enemies_);
}

const std::vector<GridPoint>& QLearningGrid::GetEnemies() const {
    return enemies_;
}

const std::vector<BattleUnit>& QLearningGrid::GetBattleUnits() const {
    return battleUnits_;
}

const std::vector<BattleAttackTrace>& QLearningGrid::GetBattleAttackTraces() const {
    return battleAttackTraces_;
}

HeadquartersCommand QLearningGrid::GetHeadquartersCommand(UnitFaction faction) const {
    return currentHeadquartersCommands_[static_cast<std::size_t>(FactionIndex(faction))];
}

DivisionCommand QLearningGrid::GetDivisionCommand(UnitFaction faction, BattleLane lane) const {
    return currentDivisionCommands_[static_cast<std::size_t>(FactionIndex(faction))]
                                   [static_cast<std::size_t>(LaneIndex(lane))];
}

int QLearningGrid::GetPlayerUnitCount() const {
    return CountActiveBattleUnits(UnitFaction::Player);
}

int QLearningGrid::GetEnemyUnitCount() const {
    return CountActiveBattleUnits(UnitFaction::Enemy);
}

int QLearningGrid::GetPlayerSoldierCount() const {
    return CountBattleSoldiers(UnitFaction::Player);
}

int QLearningGrid::GetEnemySoldierCount() const {
    return CountBattleSoldiers(UnitFaction::Enemy);
}

GridPoint QLearningGrid::GetStart() const {
    return start_;
}

GridPoint QLearningGrid::GetGoal() const {
    return goal_;
}

bool QLearningGrid::IsTerminal(int x, int y) const {
    const Tile tile = GetTile(x, y);
    return tile == Tile::Goal || tile == Tile::Pit;
}

std::wstring QLearningGrid::GetMapDisplayName() const {
    /* 組み込みマップは固定名 */
    if (!usingLdtkMap_) {
        return L"BuiltIn";
    }

    /* 外部マップはファイル名とレベル名をつなぐ */
    std::wstring result = configuredLdtkPath_.filename().wstring();
    if (!loadedLevelName_.empty()) {
        result += L" / ";
        result += ToWide(loadedLevelName_);
    }
    return result;
}

int QLearningGrid::GetGridWidth() const {
    return gridWidth_;
}

int QLearningGrid::GetGridHeight() const {
    return gridHeight_;
}

float QLearningGrid::GetQValue(int x, int y, Action action) const {
    const GridPoint primaryEnemy = SelectPrimaryEnemy({x, y}, enemies_);
    const int state = JointStateIndex({x, y}, primaryEnemy);
    return playerQ_[state * kActionCount + static_cast<int>(action)];
}

float QLearningGrid::GetBestValue(int x, int y) const {
    const GridPoint primaryEnemy = SelectPrimaryEnemy({x, y}, enemies_);
    return MaxQ(playerQ_, JointStateIndex({x, y}, primaryEnemy));
}

Action QLearningGrid::GetBestAction(int x, int y) const {
    const GridPoint primaryEnemy = SelectPrimaryEnemy({x, y}, enemies_);
    return GetBestActionForState(playerQ_, JointStateIndex({x, y}, primaryEnemy));
}

int QLearningGrid::GetEpisodeCount() const {
    return episodeCount_;
}

int QLearningGrid::GetSuccessfulEpisodeCount() const {
    return successfulEpisodeCount_;
}

int QLearningGrid::GetBlockedEpisodeCount() const {
    return episodeCount_ - successfulEpisodeCount_;
}

int QLearningGrid::GetCurrentEpisodeSteps() const {
    return episodeSteps_;
}

int QLearningGrid::GetTrainingStepCount() const {
    return totalStepCount_;
}

float QLearningGrid::GetEpsilon() const {
    return epsilon_;
}

float QLearningGrid::GetLastEpisodeReward() const {
    return lastEpisodeReward_;
}

float QLearningGrid::GetAverageReward() const {
    /* まだ履歴窓に値が入っていない初期状態では、平均の基になる値がありません。 */
    if (rewardWindowCount_ <= 0) {
        return 0.0f;
    }

    /* 直近に保存した報酬だけを合計し、件数で割って現在の平均報酬を作ります。 */
    float total = 0.0f;
    for (int index = 0; index < rewardWindowCount_; ++index) {
        total += rewardWindow_[index];
    }
    return total / static_cast<float>(rewardWindowCount_);
}

float QLearningGrid::GetRecentSuccessRate() const {
    /* 成功 / 失敗の履歴がまだ 1 件もない間は、成功率も 0 扱いにします。 */
    if (rewardWindowCount_ <= 0) {
        return 0.0f;
    }

    /* 成功を 1、失敗を 0 として持っているので、その平均がそのまま成功率になります。 */
    float total = 0.0f;
    for (int index = 0; index < rewardWindowCount_; ++index) {
        total += successWindow_[index];
    }
    return total / static_cast<float>(rewardWindowCount_);
}

float QLearningGrid::GetRecentBlockedRate() const {
    return 1.0f - GetRecentSuccessRate();
}

const std::vector<QLearningGrid::EpisodeRecord>& QLearningGrid::GetEpisodeHistory() const {
    return episodeHistory_;
}

//========================================
// greedy 経路確認
//========================================

int QLearningGrid::MeasureGreedyPathLength() const {
    const std::vector<GridPoint> path = BuildGreedyPath();

    /* greedy でたどった軌跡の最後がゴールなら、「移動回数 = 点数 - 1」として数えます。 */
    if (!path.empty() &&
        path.back().x == goal_.x &&
        path.back().y == goal_.y) {
        return static_cast<int>(path.size()) - 1;
    }
    return -1;
}

std::vector<GridPoint> QLearningGrid::BuildGreedyPath(int maxSteps) const {
    if (maxSteps <= 0) {
        maxSteps = std::max(1, gridWidth_ * gridHeight_ * 2);
    }

    std::vector<GridPoint> path;
    path.reserve(static_cast<std::size_t>(maxSteps) + 1);

    /* Start と敵開始位置列から始めて、プレイヤーと全敵の greedy 行動だけを仮想的に追いかけます。 */

    /* 同じ共同状態へ戻り続ける無限ループを止めるため、訪問済み状態を記録します。 */
    const int cellCount = gridWidth_ * gridHeight_;
    std::vector<unsigned char> visited(
        static_cast<std::size_t>(cellCount * cellCount * kEnemySupportMaskCount),
        0);
    GridPoint player = start_;
    std::vector<GridPoint> enemies = enemyStarts_;
    if (enemies.empty()) {
        enemies = ChooseDefaultEnemyStarts();
    }
    path.push_back(player);

    for (int step = 0; step < maxSteps; ++step) {
        /* ゴールへ着いた時点で、この方策は最後までたどれたので終了です。 */
        if (player.x == goal_.x && player.y == goal_.y) {
            break;
        }

        /* 以前と同じ共同状態へ戻ったら、その後も同じ循環に入るだけなので打ち切ります。 */
        const GridPoint primaryEnemy = SelectPrimaryEnemy(player, enemies);
        std::size_t primaryEnemyIndex = 0;
        for (; primaryEnemyIndex < enemies.size(); ++primaryEnemyIndex) {
            if (enemies[primaryEnemyIndex].x == primaryEnemy.x &&
                enemies[primaryEnemyIndex].y == primaryEnemy.y) {
                break;
            }
        }
        const int supportMask =
            enemies.empty()
                ? 0
                : BuildEnemySupportMask(player, enemies, primaryEnemyIndex);
        const int visitedState =
            JointStateIndex(player, primaryEnemy) * kEnemySupportMaskCount +
            supportMask;
        if (visited[visitedState]) {
            break;
        }
        visited[visitedState] = 1;

        /* プレイヤーの最善行動を 1 手だけ試し、最寄り敵込みで次にどこへ進むかを見ます。 */
        const Action playerAction =
            GetBestActionForState(playerQ_, JointStateIndex(player, primaryEnemy));
        const MoveResult playerMove = SimulateMove(player, playerAction, false);
        const GridPoint playerNext = playerMove.next;

        /* 壁などで座標が変わらないなら、方策が前へ進めていないので終了です。 */
        if (playerNext.x == player.x && playerNext.y == player.y) {
            break;
        }

        path.push_back(playerNext);

        /* プレイヤーが敵のいるマスへ入った時点で捕まるので、経路はここで終わります。 */
        if (ContainsEnemy(enemies, playerNext)) {
            break;
        }

        /* 落とし穴で終わる経路も「ここで途切れる方策」として記録して止めます。 */
        if (playerMove.landedTile == Tile::Goal || playerMove.landedTile == Tile::Pit) {
            break;
        }

        /* 全敵も現在の最善行動を 1 手ずつ進め、誰かが捕捉できたらその場で終了します。 */
        std::vector<GridPoint> nextEnemies = enemies;
        bool caught = false;
        for (std::size_t enemyIndex = 0; enemyIndex < enemies.size(); ++enemyIndex) {
            const GridPoint enemyState = enemies[enemyIndex];
            const int enemySupportMask =
                BuildEnemySupportMask(playerNext, enemies, enemyIndex);
            const Action enemyAction =
                GetBestActionForState(
                    enemyQ_,
                    EnemyStateIndex(playerNext, enemyState, enemySupportMask));

            std::vector<GridPoint> occupiedEnemies = nextEnemies;
            occupiedEnemies.erase(occupiedEnemies.begin() + static_cast<std::ptrdiff_t>(enemyIndex));
            const MoveResult enemyMove =
                SimulateEnemyMove(enemyState, enemyAction, occupiedEnemies);
            nextEnemies[enemyIndex] = enemyMove.next;

            if (enemyMove.next.x == playerNext.x && enemyMove.next.y == playerNext.y) {
                caught = true;
                break;
            }
        }
        if (caught) {
            break;
        }

        player = playerNext;
        enemies = std::move(nextEnemies);
    }

    return path;
}

//========================================
// マップ内部補助
//========================================

int QLearningGrid::CellIndex(const GridPoint& point) const {
    return point.y * gridWidth_ + point.x;
}

int QLearningGrid::JointStateIndex(
    const GridPoint& playerPoint,
    const GridPoint& enemyPoint) const {
    const int cellCount = gridWidth_ * gridHeight_;
    return CellIndex(playerPoint) * cellCount + CellIndex(enemyPoint);
}

bool QLearningGrid::IsInside(const GridPoint& point) const {
    return point.x >= 0 && point.x < gridWidth_ &&
           point.y >= 0 && point.y < gridHeight_;
}

bool QLearningGrid::IsEnemyWalkable(const GridPoint& point) const {
    if (!IsInside(point)) {
        return false;
    }

    const Tile tile = GetTile(point.x, point.y);
    return tile != Tile::Wall && tile != Tile::Pit;
}

void QLearningGrid::ResetMap() {
    //========================================
    // 初期状態の組み込みマップ
    //========================================

    gridWidth_ = kBuiltInGridWidth;
    gridHeight_ = kBuiltInGridHeight;
    tiles_.assign(static_cast<std::size_t>(gridWidth_ * gridHeight_), Tile::Empty);

    start_ = {0, 0};
    goal_ = {gridWidth_ - 1, gridHeight_ - 1};
    agent_ = start_;
    enemyStarts_.clear();
    enemies_.clear();
    initialBattleUnits_.clear();
    battleUnits_.clear();
    nextBattleUnitId_ = 1;

    /* 学習の基準になる開始点、到達点、危険マスをまず固定で置きます。 */
    tiles_[CellIndex(start_)] = Tile::Start;
    tiles_[CellIndex(goal_)] = Tile::Goal;
    tiles_[CellIndex({6, 4})] = Tile::Pit;
    tiles_[CellIndex({8, 3})] = Tile::Pit;

    /* 左側と右側を分断する縦壁を置き、1 箇所だけ通路を残します。 */
    for (int y = 0; y < gridHeight_; ++y) {
        if (y != 4) {
            tiles_[CellIndex({3, y})] = Tile::Wall;
        }
    }

    /* 下側にも横壁を足し、回り込みを学ばないと届かない形にします。 */
    for (int x = 5; x <= 8; ++x) {
        tiles_[CellIndex({x, 6})] = Tile::Wall;
    }

    /* Goal までの経路距離表も、このマップ配置に合わせて作り直します。 */
    RebuildGoalDistanceMap();
    enemyStarts_ = ChooseDefaultEnemyStarts();
    enemies_ = enemyStarts_;

    AddInitialBattleUnit(UnitFaction::Player, UnitClass::Infantry, start_, DefaultUnitCount(UnitClass::Infantry));
    AddInitialBattleUnit(UnitFaction::Player, UnitClass::Archer, {0, 1}, DefaultUnitCount(UnitClass::Archer));
    AddInitialBattleUnit(UnitFaction::Player, UnitClass::Cavalry, {1, 0}, DefaultUnitCount(UnitClass::Cavalry));

    if (!enemyStarts_.empty()) {
        AddInitialBattleUnit(UnitFaction::Enemy, UnitClass::Infantry, enemyStarts_[0], DefaultUnitCount(UnitClass::Infantry));
    }

    if (enemyStarts_.size() >= 2) {
        AddInitialBattleUnit(UnitFaction::Enemy, UnitClass::Archer, enemyStarts_[1], DefaultUnitCount(UnitClass::Archer));
    }

    if (enemyStarts_.size() >= 3) {
        AddInitialBattleUnit(UnitFaction::Enemy, UnitClass::Cavalry, enemyStarts_[2], DefaultUnitCount(UnitClass::Cavalry));
    }

    usingLdtkMap_ = false;
    loadedLevelName_ = "BuiltIn";
}

void QLearningGrid::ApplyLoadedMap(
    const std::string& levelIdentifier,
    int gridWidth,
    int gridHeight,
    const std::vector<int>& intGridCsv,
    const std::vector<LdtkEntityData>& entities) {
    //========================================
    // サイズ検証
    //========================================

    if (gridWidth <= 0 || gridHeight <= 0) {
        throw std::runtime_error("LDtk level size must be positive.");
    }

    const int cellCount = gridWidth * gridHeight;
    std::vector<Tile> loadedTiles(static_cast<std::size_t>(cellCount), Tile::Empty);
    GridPoint loadedStart = {0, 0};
    GridPoint loadedGoal = {gridWidth - 1, gridHeight - 1};
    std::vector<GridPoint> loadedEnemyStarts;
    std::vector<BattleUnit> loadedBattleUnits;

    bool hasStart = false;
    bool hasGoal = false;

    //========================================
    // IntGrid 反映
    //========================================

    /* まずセル単位の地形情報を敷き詰め、開始点とゴール候補もここで拾います。 */

    for (int index = 0; index < static_cast<int>(intGridCsv.size()); ++index) {
        const int x = index % gridWidth;
        const int y = index / gridWidth;
        const int stateIndex = y * gridWidth + x;

        const Tile converted =
            ConvertIntGridValue(intGridCsv[static_cast<std::size_t>(index)]);
        if (converted == Tile::Empty) {
            continue;
        }

        loadedTiles[static_cast<std::size_t>(stateIndex)] = converted;
        if (converted == Tile::Start) {
            loadedStart = {x, y};
            hasStart = true;
        } else if (converted == Tile::Goal) {
            loadedGoal = {x, y};
            hasGoal = true;
        }
    }

    //========================================
    // エンティティ上書き
    //========================================

    /* その上からエンティティ情報を重ね、LDtk 側で明示した配置を優先させます。 */

    for (const LdtkEntityData& entity : entities) {
        const std::string& identifier = entity.identifier;
        const GridPoint point = {entity.gridX, entity.gridY};

        if (point.x < 0 || point.x >= gridWidth ||
            point.y < 0 || point.y >= gridHeight) {
            continue;
        }

        const int stateIndex = point.y * gridWidth + point.x;
        UnitFaction unitFaction = UnitFaction::Player;
        UnitClass unitClass = UnitClass::Infantry;

        if (IsStartIdentifier(identifier)) {
            loadedTiles[static_cast<std::size_t>(stateIndex)] = Tile::Start;
            loadedStart = point;
            hasStart = true;
        } else if (IsGoalIdentifier(identifier)) {
            loadedTiles[static_cast<std::size_t>(stateIndex)] = Tile::Goal;
            loadedGoal = point;
            hasGoal = true;
        } else if (IsPitIdentifier(identifier)) {
            loadedTiles[static_cast<std::size_t>(stateIndex)] = Tile::Pit;
        } else if (IsWallIdentifier(identifier)) {
            loadedTiles[static_cast<std::size_t>(stateIndex)] = Tile::Wall;
        } else if (TryParseBattleUnitIdentifier(identifier, unitFaction, unitClass)) {
            const int unitCount =
                entity.count > 0 ? entity.count : DefaultUnitCount(unitClass);
            BattleUnit unit = {};
            unit.id = static_cast<int>(loadedBattleUnits.size()) + 1;
            unit.faction = unitFaction;
            unit.unitClass = unitClass;
            unit.position = point;
            unit.startPosition = point;
            unit.count = unitCount;
            unit.maxCount = unitCount;
            unit.active = unitCount > 0;
            loadedBattleUnits.push_back(unit);

            if (unitFaction == UnitFaction::Player && !hasStart) {
                loadedStart = point;
                hasStart = true;
            }

            if (unitFaction == UnitFaction::Enemy) {
                if (std::find_if(
                        loadedEnemyStarts.begin(),
                        loadedEnemyStarts.end(),
                        [&point](const GridPoint& current) {
                            return current.x == point.x && current.y == point.y;
                        }) == loadedEnemyStarts.end()) {
                    loadedEnemyStarts.push_back(point);
                }
            }
        } else if (IsEnemyIdentifier(identifier)) {
            if (std::find_if(
                    loadedEnemyStarts.begin(),
                    loadedEnemyStarts.end(),
                    [&point](const GridPoint& current) {
                        return current.x == point.x && current.y == point.y;
                    }) == loadedEnemyStarts.end()) {
                loadedEnemyStarts.push_back(point);
            }
        } else if (IsGeneralIdentifier(identifier)) {
            const UnitFaction genFaction = GeneralFaction(identifier);
            const int genCount = entity.count > 0 ? entity.count : DefaultUnitCount(UnitClass::Infantry);
            BattleUnit gen = {};
            gen.id = static_cast<int>(loadedBattleUnits.size()) + 1;
            gen.faction = genFaction;
            gen.unitClass = UnitClass::Infantry;
            gen.position = point;
            gen.startPosition = point;
            gen.count = genCount;
            gen.maxCount = genCount;
            gen.active = genCount > 0;
            gen.isGeneral = true;
            loadedBattleUnits.push_back(gen);

            if (genFaction == UnitFaction::Enemy) {
                loadedEnemyStarts.push_back(point);
            }
        }
    }

    //========================================
    // 最低条件確認
    //========================================

    const bool hasBattleUnits = !loadedBattleUnits.empty();
    if (!hasBattleUnits && (!hasStart || !hasGoal)) {
        throw std::runtime_error("LDtk map must contain both Start and Goal.");
    }

    /* 最後に Start と Goal を再代入し、他レイヤーの上書きで消えないよう確定させます。 */
    if (hasStart) {
        loadedTiles[static_cast<std::size_t>(loadedStart.y * gridWidth + loadedStart.x)] = Tile::Start;
    }

    if (hasGoal) {
        loadedTiles[static_cast<std::size_t>(loadedGoal.y * gridWidth + loadedGoal.x)] = Tile::Goal;
    }

    gridWidth_ = gridWidth;
    gridHeight_ = gridHeight;
    tiles_ = std::move(loadedTiles);
    start_ = loadedStart;
    goal_ = loadedGoal;
    agent_ = start_;

    /* 新しい地形へ切り替わったので、Goal までの距離地図も張り直します。 */
    RebuildGoalDistanceMap();

    initialBattleUnits_ = std::move(loadedBattleUnits);
    nextBattleUnitId_ = static_cast<int>(initialBattleUnits_.size()) + 1;
    if (initialBattleUnits_.empty()) {
        AddInitialBattleUnit(UnitFaction::Player, UnitClass::Infantry, start_, DefaultUnitCount(UnitClass::Infantry));
    }

    /* 敵開始位置は LDtk 側の配置を優先し、不正座標は除外したうえで 0 体なら自動配置します。 */
    enemyStarts_.clear();
    for (const GridPoint& enemyStart : loadedEnemyStarts) {
        if (!IsEnemyWalkable(enemyStart) ||
            (enemyStart.x == start_.x && enemyStart.y == start_.y) ||
            (enemyStart.x == goal_.x && enemyStart.y == goal_.y)) {
            continue;
        }

        enemyStarts_.push_back(enemyStart);
    }

    if (enemyStarts_.empty()) {
        enemyStarts_ = ChooseDefaultEnemyStarts();
    }

    if (std::none_of(
            initialBattleUnits_.begin(),
            initialBattleUnits_.end(),
            [](const BattleUnit& unit) {
                return unit.faction == UnitFaction::Enemy;
            })) {
        for (const GridPoint& enemyStart : enemyStarts_) {
            AddInitialBattleUnit(
                UnitFaction::Enemy,
                UnitClass::Infantry,
                enemyStart,
                DefaultUnitCount(UnitClass::Infantry));
        }
    }

    /* 将軍-配下の自動割り当て */
    for (BattleUnit& unit : loadedBattleUnits) {
        if (unit.isGeneral || unit.commanderId >= 0) {
            continue;
        }
        int nearestGeneralId = -1;
        int nearestDist = 999;
        for (const BattleUnit& gen : loadedBattleUnits) {
            if (!gen.isGeneral || gen.faction != unit.faction) {
                continue;
            }
            const int d = std::abs(gen.position.x - unit.position.x) +
                          std::abs(gen.position.y - unit.position.y);
            if (d < nearestDist) {
                nearestDist = d;
                nearestGeneralId = gen.id;
            }
        }
        if (nearestGeneralId >= 0) {
            unit.commanderId = nearestGeneralId;
        }
    }

    enemies_ = enemyStarts_;
    loadedLevelName_ = levelIdentifier;
}

void QLearningGrid::ResetLearningState() {
    /* 以前の学習結果が新しいマップへ混ざらないよう、Q 値と履歴窓を丸ごと消します。 */
    const std::size_t cellCount = static_cast<std::size_t>(gridWidth_ * gridHeight_);
    const std::size_t jointStateCount = cellCount * cellCount;
    playerQ_.assign(jointStateCount * static_cast<std::size_t>(kActionCount), 0.0f);
    enemyQ_.assign(
        jointStateCount * static_cast<std::size_t>(kEnemySupportMaskCount) *
            static_cast<std::size_t>(kActionCount),
        0.0f);
    for (std::vector<float>& qTable : headquartersQ_) {
        qTable.assign(
            static_cast<std::size_t>(kHeadquartersStateCount) *
                static_cast<std::size_t>(kHeadquartersCommandCount),
            0.0f);
    }

    for (std::vector<float>& qTable : divisionQ_) {
        qTable.assign(
            static_cast<std::size_t>(kDivisionStateCount) *
                static_cast<std::size_t>(kDivisionCommandCount),
            0.0f);
    }

    for (std::vector<float>& qTable : generalQ_) {
        qTable.assign(
            static_cast<std::size_t>(kGeneralStateCount) *
                static_cast<std::size_t>(kGeneralActionCount),
            0.0f);
    }

    rewardWindow_.fill(0.0f);
    successWindow_.fill(0.0f);

    /* 画面表示や CSV で使う集計値も、最初から学習し直す前提で初期化します。 */
    episodeCount_ = 0;
    successfulEpisodeCount_ = 0;
    episodeSteps_ = 0;
    totalStepCount_ = 0;
    rewardWindowCursor_ = 0;
    rewardWindowCount_ = 0;
    epsilon_ = 0.35f;
    episodeReward_ = 0.0f;
    lastEpisodeReward_ = 0.0f;
    episodeHistory_.clear();
    currentHeadquartersCommands_ = {
        HeadquartersCommand::Balanced,
        HeadquartersCommand::Balanced,
    };
    for (std::array<DivisionCommand, kBattleLaneCount>& divisionCommands : currentDivisionCommands_) {
        divisionCommands.fill(DivisionCommand::Hold);
    }

    ResetEpisode();
}

void QLearningGrid::RebuildGoalDistanceMap() {
    /* 壁と落とし穴を避けた通行可能マスだけで、Goal から逆向き BFS を張ります。 */
    goalDistance_.assign(static_cast<std::size_t>(gridWidth_ * gridHeight_), -1);

    if (!IsInside(goal_)) {
        return;
    }

    std::queue<GridPoint> frontier;
    goalDistance_[static_cast<std::size_t>(CellIndex(goal_))] = 0;
    frontier.push(goal_);

    while (!frontier.empty()) {
        const GridPoint current = frontier.front();
        frontier.pop();

        const int currentDistance = goalDistance_[static_cast<std::size_t>(CellIndex(current))];
        const std::array<GridPoint, 4> neighbors = {
            GridPoint{current.x + 1, current.y},
            GridPoint{current.x - 1, current.y},
            GridPoint{current.x, current.y + 1},
            GridPoint{current.x, current.y - 1},
        };

        for (const GridPoint& next : neighbors) {
            if (!IsInside(next)) {
                continue;
            }

            const Tile tile = GetTile(next.x, next.y);
            if (tile == Tile::Wall || tile == Tile::Pit) {
                continue;
            }

            const int nextIndex = CellIndex(next);
            if (goalDistance_[static_cast<std::size_t>(nextIndex)] >= 0) {
                continue;
            }

            goalDistance_[static_cast<std::size_t>(nextIndex)] = currentDistance + 1;
            frontier.push(next);
        }
    }
}

std::vector<GridPoint> QLearningGrid::ChooseDefaultEnemyStarts() const {
    /* まず Start から Goal への代表最短経路を 1 本復元し、その中腹付近へ複数候補を置きます。 */
    std::vector<GridPoint> shortestPath;
    if (GoalDistance(start_) >= 0) {
        GridPoint current = start_;
        shortestPath.push_back(current);

        for (int step = 0; step < gridWidth_ * gridHeight_; ++step) {
            if (current.x == goal_.x && current.y == goal_.y) {
                break;
            }

            const int currentDistance = GoalDistance(current);
            if (currentDistance <= 0) {
                break;
            }

            const std::array<GridPoint, 4> neighbors = {
                GridPoint{current.x + 1, current.y},
                GridPoint{current.x, current.y + 1},
                GridPoint{current.x - 1, current.y},
                GridPoint{current.x, current.y - 1},
            };

            bool foundNext = false;
            for (const GridPoint& next : neighbors) {
                if (!IsEnemyWalkable(next)) {
                    continue;
                }

                if (GoalDistance(next) == currentDistance - 1) {
                    shortestPath.push_back(next);
                    current = next;
                    foundNext = true;
                    break;
                }
            }

            if (!foundNext) {
                break;
            }
        }
    }

    std::vector<GridPoint> candidates;
    if (shortestPath.size() >= 3) {
        const std::array<float, 3> anchors = {0.35f, 0.55f, 0.75f};
        for (float anchor : anchors) {
            const int index = static_cast<int>(
                std::lround((static_cast<float>(shortestPath.size() - 1)) * anchor));
            if (index <= 0 || index >= static_cast<int>(shortestPath.size()) - 1) {
                continue;
            }

            const GridPoint candidate = shortestPath[static_cast<std::size_t>(index)];
            if (!(candidate.x == start_.x && candidate.y == start_.y) &&
                !(candidate.x == goal_.x && candidate.y == goal_.y) &&
                IsEnemyWalkable(candidate) &&
                !ContainsEnemy(candidates, candidate)) {
                candidates.push_back(candidate);
            }
        }
    }

    /* 代表経路が短い場合は、中央寄りの通常マスから追加候補を埋めます。 */
    for (int y = 0; y < gridHeight_; ++y) {
        for (int x = 0; x < gridWidth_; ++x) {
            if (static_cast<int>(candidates.size()) >= 3) {
                break;
            }

            const GridPoint candidate{x, y};
            if (!IsEnemyWalkable(candidate) ||
                (candidate.x == start_.x && candidate.y == start_.y) ||
                (candidate.x == goal_.x && candidate.y == goal_.y) ||
                ContainsEnemy(candidates, candidate)) {
                continue;
            }

            const int centerDistance =
                std::abs(x * 2 - (gridWidth_ - 1)) +
                std::abs(y * 2 - (gridHeight_ - 1));
            if (centerDistance <= std::max(4, (gridWidth_ + gridHeight_) / 6)) {
                candidates.push_back(candidate);
            }
        }
        if (static_cast<int>(candidates.size()) >= 3) {
            break;
        }
    }

    if (candidates.empty()) {
        for (int y = 0; y < gridHeight_; ++y) {
            for (int x = 0; x < gridWidth_; ++x) {
                const GridPoint candidate{x, y};
                if (IsEnemyWalkable(candidate) &&
                    !(candidate.x == start_.x && candidate.y == start_.y) &&
                    !(candidate.x == goal_.x && candidate.y == goal_.y)) {
                    candidates.push_back(candidate);
                    return candidates;
                }
            }
        }
    }

    return candidates;
}

GridPoint QLearningGrid::SelectPrimaryEnemy(
    const GridPoint& playerPoint,
    const std::vector<GridPoint>& enemies) const {
    if (enemies.empty()) {
        return start_;
    }

    GridPoint bestEnemy = enemies.front();
    int bestDistance =
        std::abs(bestEnemy.x - playerPoint.x) +
        std::abs(bestEnemy.y - playerPoint.y);
    for (const GridPoint& enemy : enemies) {
        const int distance =
            std::abs(enemy.x - playerPoint.x) +
            std::abs(enemy.y - playerPoint.y);
        if (distance < bestDistance ||
            (distance == bestDistance &&
             (enemy.y < bestEnemy.y ||
              (enemy.y == bestEnemy.y && enemy.x < bestEnemy.x)))) {
            bestEnemy = enemy;
            bestDistance = distance;
        }
    }
    return bestEnemy;
}

bool QLearningGrid::ContainsEnemy(
    const std::vector<GridPoint>& enemies,
    const GridPoint& point) const {
    return std::any_of(
        enemies.begin(),
        enemies.end(),
        [&point](const GridPoint& enemy) {
            return enemy.x == point.x && enemy.y == point.y;
        });
}

int QLearningGrid::EnemyStateIndex(
    const GridPoint& playerPoint,
    const GridPoint& enemyPoint,
    int supportMask) const {
    return JointStateIndex(playerPoint, enemyPoint) * kEnemySupportMaskCount +
           supportMask;
}

int QLearningGrid::BuildEnemySupportMask(
    const GridPoint& playerPoint,
    const std::vector<GridPoint>& enemies,
    std::size_t ignoredEnemyIndex) const {
    /* 他の敵がプレイヤーの隣接 4 マスを押さえていれば、その方向ビットを立てます。 */
    int mask = 0;
    for (std::size_t enemyIndex = 0; enemyIndex < enemies.size(); ++enemyIndex) {
        if (enemyIndex == ignoredEnemyIndex) {
            continue;
        }

        const GridPoint& enemy = enemies[enemyIndex];
        if (enemy.x == playerPoint.x && enemy.y == playerPoint.y - 1) {
            mask |= (1 << static_cast<int>(Action::Up));
        } else if (enemy.x == playerPoint.x + 1 && enemy.y == playerPoint.y) {
            mask |= (1 << static_cast<int>(Action::Right));
        } else if (enemy.x == playerPoint.x && enemy.y == playerPoint.y + 1) {
            mask |= (1 << static_cast<int>(Action::Down));
        } else if (enemy.x == playerPoint.x - 1 && enemy.y == playerPoint.y) {
            mask |= (1 << static_cast<int>(Action::Left));
        }
    }

    return mask;
}

int QLearningGrid::CountPlayerEscapeRoutes(
    const GridPoint& playerPoint,
    const std::vector<GridPoint>& enemies) const {
    /* 落とし穴は実質的な逃げ道ではないので除外し、安全に抜けられる方向だけ数えます。 */
    int routeCount = 0;
    for (int actionIndex = 0; actionIndex < kActionCount; ++actionIndex) {
        const MoveResult move =
            SimulateMove(playerPoint, static_cast<Action>(actionIndex), true);
        if (!move.blocked && !ContainsEnemy(enemies, move.next)) {
            ++routeCount;
        }
    }
    return routeCount;
}

int QLearningGrid::CountAdjacentEnemyCoverage(
    const GridPoint& playerPoint,
    const std::vector<GridPoint>& enemies) const {
    int mask = BuildEnemySupportMask(playerPoint, enemies, enemies.size());
    int coveredCount = 0;

    /* 4 ビットを数え上げて、プレイヤー周囲が何方向ふさがっているかへ変換します。 */
    for (int bitIndex = 0; bitIndex < kActionCount; ++bitIndex) {
        if ((mask & (1 << bitIndex)) != 0) {
            ++coveredCount;
        }
    }
    return coveredCount;
}

int QLearningGrid::EnemyApproachDirection(
    const GridPoint& playerPoint,
    const GridPoint& enemyPoint) const {
    const int dx = enemyPoint.x - playerPoint.x;
    const int dy = enemyPoint.y - playerPoint.y;

    /* 差が大きい軸を主方向とみなし、その側面から接近していると解釈します。 */
    if (std::abs(dx) >= std::abs(dy)) {
        return dx >= 0 ? static_cast<int>(Action::Right)
                       : static_cast<int>(Action::Left);
    }
    return dy >= 0 ? static_cast<int>(Action::Down)
                   : static_cast<int>(Action::Up);
}

int QLearningGrid::CountEnemyApproachDirections(
    const GridPoint& playerPoint,
    const std::vector<GridPoint>& enemies) const {
    int directionMask = 0;
    for (const GridPoint& enemy : enemies) {
        directionMask |= (1 << EnemyApproachDirection(playerPoint, enemy));
    }

    int directionCount = 0;
    for (int bitIndex = 0; bitIndex < kActionCount; ++bitIndex) {
        if ((directionMask & (1 << bitIndex)) != 0) {
            ++directionCount;
        }
    }
    return directionCount;
}

int QLearningGrid::GoalDistance(const GridPoint& point) const {
    if (!IsInside(point)) {
        return -1;
    }
    return goalDistance_[static_cast<std::size_t>(CellIndex(point))];
}

int QLearningGrid::EpisodeStepLimit() const {
    const int scaledLimit = gridWidth_ * gridHeight_ + gridWidth_ + gridHeight_;
    return std::max(kBaseMaxEpisodeSteps, scaledLimit);
}

float QLearningGrid::EpsilonDecayFactor() const {
    /* 面積が大きいほど探索空間も広がるので、減衰を少し緩めて探索期間を延ばします。 */
    const float areaScale = std::clamp(
        static_cast<float>(gridWidth_ * gridHeight_) / 100.0f,
        1.0f,
        4.0f);
    return 1.0f - ((1.0f - kBaseEpsilonDecay) / areaScale);
}

float QLearningGrid::MinimumExplorationRate() const {
    /* まだ一度もゴールへ届いていない間は、局所解へ固まらないよう探索を厚めに残します。 */
    if (successfulEpisodeCount_ <= 0) {
        const float areaScale = std::clamp(
            static_cast<float>(gridWidth_ * gridHeight_) / 100.0f,
            1.0f,
            4.0f);
        return std::min(0.20f, 0.08f + (areaScale - 1.0f) * 0.04f);
    }

    /* 一度経路が見つかったあとは、微調整用の少量探索だけ残します。 */
    return kMinEpsilon;
}

//========================================
// 行動選択と遷移
//========================================

float QLearningGrid::MaxQ(
    const std::vector<float>& qTable,
    int stateIndex) const {
    float bestValue = std::numeric_limits<float>::lowest();

    /* その状態添字にぶら下がる 4 行動の Q 値を総当たりし、最大値だけを抜き出します。 */
    for (int actionIndex = 0; actionIndex < kActionCount; ++actionIndex) {
        bestValue = std::max(bestValue, qTable[stateIndex * kActionCount + actionIndex]);
    }

    return bestValue;
}

Action QLearningGrid::RandomAction() {
    /* 探索時は価値を見ず、4 方向を均等確率で 1 つ選びます。 */
    std::uniform_int_distribution<int> distribution(0, kActionCount - 1);
    return static_cast<Action>(distribution(rng_));
}

Action QLearningGrid::GetBestActionForState(
    const std::vector<float>& qTable,
    int stateIndex) const {
    float bestValue = std::numeric_limits<float>::lowest();
    Action bestAction = Action::Up;

    /* 描画や経路確認では乱数を使わず、最初に見つかった最大値を採用します。 */
    for (int actionIndex = 0; actionIndex < kActionCount; ++actionIndex) {
        const float qValue = qTable[stateIndex * kActionCount + actionIndex];
        if (qValue > bestValue) {
            bestValue = qValue;
            bestAction = static_cast<Action>(actionIndex);
        }
    }

    return bestAction;
}

Action QLearningGrid::SelectGreedyAction(
    const std::vector<float>& qTable,
    int stateIndex) {
    float bestValue = std::numeric_limits<float>::lowest();
    std::array<Action, kActionCount> candidates = {};
    int candidateCount = 0;

    /* 4 方向を順番に見て、最大値を更新したら候補を入れ替え、同点なら候補へ追加します。 */
    for (int actionIndex = 0; actionIndex < kActionCount; ++actionIndex) {
        const float qValue = qTable[stateIndex * kActionCount + actionIndex];
        if (qValue > bestValue + 0.0001f) {
            bestValue = qValue;
            candidates[0] = static_cast<Action>(actionIndex);
            candidateCount = 1;
        } else if (std::fabs(qValue - bestValue) <= 0.0001f) {
            candidates[candidateCount++] = static_cast<Action>(actionIndex);
        }
    }

    /* 同率首位が複数あるときは、その中からランダムに 1 つ選んで偏りを減らします。 */
    std::uniform_int_distribution<int> distribution(0, candidateCount - 1);
    return candidates[distribution(rng_)];
}

Action QLearningGrid::SelectAction(
    const std::vector<float>& qTable,
    int stateIndex) {
    /* epsilon 未満なら探索、それ以外なら現在の最良手を採用する epsilon-greedy です。 */
    if (randomUnit_(rng_) < epsilon_) {
        return RandomAction();
    }
    return SelectGreedyAction(qTable, stateIndex);
}

float QLearningGrid::MaxTacticalQ(
    const std::vector<float>& qTable,
    int stateIndex,
    int actionCount) const {
    float bestValue = std::numeric_limits<float>::lowest();
    for (int actionIndex = 0; actionIndex < actionCount; ++actionIndex) {
        bestValue = std::max(bestValue, qTable[stateIndex * actionCount + actionIndex]);
    }

    return bestValue;
}

int QLearningGrid::SelectTacticalAction(
    const std::vector<float>& qTable,
    int stateIndex,
    int actionCount) {
    if (randomUnit_(rng_) < epsilon_) {
        std::uniform_int_distribution<int> distribution(0, actionCount - 1);
        return distribution(rng_);
    }

    float bestValue = std::numeric_limits<float>::lowest();
    std::vector<int> candidates;
    candidates.reserve(static_cast<std::size_t>(actionCount));

    for (int actionIndex = 0; actionIndex < actionCount; ++actionIndex) {
        const float value = qTable[stateIndex * actionCount + actionIndex];
        if (value > bestValue + 0.0001f) {
            bestValue = value;
            candidates.clear();
            candidates.push_back(actionIndex);
        } else if (std::fabs(value - bestValue) <= 0.0001f) {
            candidates.push_back(actionIndex);
        }
    }

    std::uniform_int_distribution<int> distribution(
        0,
        static_cast<int>(candidates.size()) - 1);
    return candidates[static_cast<std::size_t>(distribution(rng_))];
}

void QLearningGrid::UpdateTacticalQ(
    std::vector<float>& qTable,
    int stateIndex,
    int actionIndex,
    float reward,
    int nextStateIndex,
    bool done,
    int actionCount) {
    float& current = qTable[stateIndex * actionCount + actionIndex];
    const float future = done ? 0.0f : MaxTacticalQ(qTable, nextStateIndex, actionCount);
    current += kAlpha * (reward + kGamma * future - current);
}

QLearningGrid::MoveResult QLearningGrid::SimulateMove(
    const GridPoint& point,
    Action action,
    bool avoidPit) const {
    GridPoint next = point;

    //========================================
    // 次座標計算
    //========================================

    switch (action) {
    case Action::Up:
        next.y -= 1;
        break;
    case Action::Right:
        next.x += 1;
        break;
    case Action::Down:
        next.y += 1;
        break;
    case Action::Left:
        next.x -= 1;
        break;
    }

    //========================================
    // 壁判定
    //========================================

    /* 盤外か壁なら移動は成立しないので、座標は据え置きのまま blocked 扱いで返します。 */
    if (!IsInside(next)) {
        return {point, true, GetTile(point.x, point.y)};
    }

    const Tile tile = GetTile(next.x, next.y);
    if (tile == Tile::Wall || (avoidPit && tile == Tile::Pit)) {
        return {point, true, GetTile(point.x, point.y)};
    }

    return {next, false, tile};
}

QLearningGrid::MoveResult QLearningGrid::SimulateEnemyMove(
    const GridPoint& point,
    Action action,
    const std::vector<GridPoint>& occupiedEnemies) const {
    /* 敵は落とし穴を避けつつ、他の敵が居るマスへは重ならないように移動させます。 */
    const MoveResult move = SimulateMove(point, action, true);
    if (move.blocked) {
        return move;
    }

    if (ContainsEnemy(occupiedEnemies, move.next)) {
        return {point, true, GetTile(point.x, point.y)};
    }

    return move;
}

float QLearningGrid::PlayerProgressReward(
    const GridPoint& from,
    const GridPoint& to) const {
    const int previousDistance = GoalDistance(from);
    const int nextDistance = GoalDistance(to);
    float progressReward = 0.0f;

    /* 壁回りの遠回りも正しく評価できるよう、実際の最短経路距離の増減で進捗を測ります。 */
    if (previousDistance >= 0 && nextDistance >= 0) {
        progressReward =
            static_cast<float>(previousDistance - nextDistance) * 0.08f;
    } else if (previousDistance >= 0 && nextDistance < 0) {
        /* Goal へ届かない閉路側へ入り込む手は、学習が吸い寄せられないよう少し減点します。 */
        progressReward = -0.20f;
    } else if (previousDistance < 0 && nextDistance >= 0) {
        /* 行き止まり圏から Goal 側の到達可能領域へ戻る手は、抜け出しやすいよう少し加点します。 */
        progressReward = 0.20f;
    }

    return progressReward;
}

void QLearningGrid::AddInitialBattleUnit(
    UnitFaction faction,
    UnitClass unitClass,
    const GridPoint& position,
    int count) {
    if (!IsInside(position)) {
        return;
    }

    if (!IsBattleWalkable(position)) {
        return;
    }

    const int unitCount = std::max(1, count);
    BattleUnit unit = {};
    unit.id = nextBattleUnitId_;
    unit.faction = faction;
    unit.unitClass = unitClass;
    unit.position = position;
    unit.startPosition = position;
    unit.count = unitCount;
    unit.maxCount = unitCount;
    unit.active = true;
    initialBattleUnits_.push_back(unit);

    ++nextBattleUnitId_;
}

void QLearningGrid::SyncLegacyPositionsFromBattleUnits() {
    bool foundPlayer = false;
    enemies_.clear();

    for (const BattleUnit& unit : battleUnits_) {
        if (!unit.active || unit.count <= 0) {
            continue;
        }

        if (unit.faction == UnitFaction::Player && !foundPlayer) {
            agent_ = unit.position;
            foundPlayer = true;
        } else if (unit.faction == UnitFaction::Enemy) {
            enemies_.push_back(unit.position);
        }
    }

    if (!foundPlayer) {
        agent_ = start_;
    }
}

int QLearningGrid::CountActiveBattleUnits(UnitFaction faction) const {
    int unitCount = 0;
    for (const BattleUnit& unit : battleUnits_) {
        if (unit.faction == faction && unit.active && unit.count > 0) {
            ++unitCount;
        }
    }
    return unitCount;
}

int QLearningGrid::CountBattleSoldiers(UnitFaction faction) const {
    int soldierCount = 0;
    for (const BattleUnit& unit : battleUnits_) {
        if (unit.faction == faction && unit.active && unit.count > 0) {
            soldierCount += unit.count;
        }
    }
    return soldierCount;
}

int QLearningGrid::FactionIndex(UnitFaction faction) const {
    return faction == UnitFaction::Player ? 0 : 1;
}

BattleLane QLearningGrid::LaneForPoint(const GridPoint& point) const {
    const int laneWidth = std::max(1, gridWidth_ / kBattleLaneCount);
    if (point.x < laneWidth) {
        return BattleLane::Left;
    }

    if (point.x >= laneWidth * 2) {
        return BattleLane::Right;
    }

    return BattleLane::Center;
}

int QLearningGrid::LaneIndex(BattleLane lane) const {
    return static_cast<int>(lane);
}

int QLearningGrid::SoldierRatioBucket(int current, int maximum) const {
    if (maximum <= 0) {
        return 0;
    }

    const float ratio =
        static_cast<float>(std::max(0, current)) / static_cast<float>(maximum);
    if (ratio < 0.34f) {
        return 0;
    }

    if (ratio < 0.67f) {
        return 1;
    }

    return 2;
}

int QLearningGrid::AdvantageBucket(int ownSoldiers, int enemySoldiers) const {
    const int total = std::max(1, ownSoldiers + enemySoldiers);
    const float advantage =
        static_cast<float>(ownSoldiers - enemySoldiers) / static_cast<float>(total);
    if (advantage < -0.15f) {
        return 0;
    }

    if (advantage > 0.15f) {
        return 2;
    }

    return 1;
}

int QLearningGrid::BuildGeneralState(
    int ownSoldiers, int ownMax,
    int enemySoldiers, int enemyMax,
    int distance,
    int generalIndex) const {
    int stateIndex = SoldierRatioBucket(ownSoldiers, ownMax);
    stateIndex = stateIndex * kGeneralBucketCount +
                 SoldierRatioBucket(enemySoldiers, enemyMax);
    stateIndex = stateIndex * kGeneralBucketCount +
                 AdvantageBucket(ownSoldiers, enemySoldiers);
    stateIndex = stateIndex * kGeneralBucketCount +
                 std::min(2, distance / 5);
    stateIndex = stateIndex * 2 +
                 std::min(1, generalIndex);
    return stateIndex;
}

int QLearningGrid::SelectGeneralAction(int stateIndex, int factionIndex) {
    if (randomUnit_(rng_) < epsilon_) {
        std::uniform_int_distribution<int> dist(0, kGeneralActionCount - 1);
        return dist(rng_);
    }

    const std::vector<float>& q = generalQ_[static_cast<std::size_t>(factionIndex)];
    float bestValue = std::numeric_limits<float>::lowest();
    std::vector<int> candidates;
    candidates.reserve(static_cast<std::size_t>(kGeneralActionCount));

    for (int ai = 0; ai < kGeneralActionCount; ++ai) {
        const float v = q[stateIndex * kGeneralActionCount + ai];
        if (v > bestValue + 0.0001f) {
            bestValue = v;
            candidates.clear();
            candidates.push_back(ai);
        } else if (std::fabs(v - bestValue) <= 0.0001f) {
            candidates.push_back(ai);
        }
    }

    std::uniform_int_distribution<int> dist(0, static_cast<int>(candidates.size()) - 1);
    return candidates[static_cast<std::size_t>(dist(rng_))];
}

void QLearningGrid::UpdateGeneralQ(
    int factionIndex,
    int stateIndex,
    int actionIndex,
    float reward,
    int nextStateIndex,
    bool done) {
    std::vector<float>& q = generalQ_[static_cast<std::size_t>(factionIndex)];
    float& current = q[stateIndex * kGeneralActionCount + actionIndex];
    const float future = done ? 0.0f : MaxTacticalQ(q, nextStateIndex, kGeneralActionCount);
    constexpr float kAlpha = 0.20f;
    constexpr float kGamma = 0.90f;
    current += kAlpha * (reward + kGamma * future - current);
}

int QLearningGrid::CountGeneralGroupSoldiers(int generalUnitIndex) const {
    const BattleUnit& gen = battleUnits_[static_cast<std::size_t>(generalUnitIndex)];
    if (!gen.isGeneral) return gen.count;
    return CountGeneralGroupSoldiersById(gen.id);
}

int QLearningGrid::CountGeneralGroupSoldiersById(int generalId) const {
    int total = 0;
    for (const BattleUnit& u : battleUnits_) {
        if (!u.active || u.count <= 0) { continue; }
        if (u.id == generalId || u.commanderId == generalId) {
            total += u.count;
        }
    }
    return total;
}

int QLearningGrid::CountBattleSoldiersInLane(UnitFaction faction, BattleLane lane) const {
    int soldierCount = 0;
    for (const BattleUnit& unit : battleUnits_) {
        if (!unit.active || unit.count <= 0 || unit.faction != faction) {
            continue;
        }

        if (LaneForPoint(unit.position) == lane) {
            soldierCount += unit.count;
        }
    }

    return soldierCount;
}

int QLearningGrid::CountInitialBattleSoldiers(UnitFaction faction) const {
    int soldierCount = 0;
    for (const BattleUnit& unit : initialBattleUnits_) {
        if (unit.faction == faction && unit.maxCount > 0) {
            soldierCount += unit.maxCount;
        }
    }

    return soldierCount;
}

int QLearningGrid::CountInitialBattleSoldiersInLane(UnitFaction faction, BattleLane lane) const {
    int soldierCount = 0;
    for (const BattleUnit& unit : initialBattleUnits_) {
        if (unit.faction != faction || unit.maxCount <= 0) {
            continue;
        }

        if (LaneForPoint(unit.startPosition) == lane) {
            soldierCount += unit.maxCount;
        }
    }

    return soldierCount;
}

int QLearningGrid::EnemyArcherPressureBucket(UnitFaction faction) const {
    const UnitFaction enemyFaction = OppositeFaction(faction);
    int pressureCount = 0;

    for (const BattleUnit& enemy : battleUnits_) {
        if (!enemy.active || enemy.count <= 0 ||
            enemy.faction != enemyFaction ||
            enemy.unitClass != UnitClass::Archer) {
            continue;
        }

        for (const BattleUnit& own : battleUnits_) {
            if (!own.active || own.count <= 0 || own.faction != faction) {
                continue;
            }

            if (BattleDistance(enemy.position, own.position) <= UnitAttackRange(UnitClass::Archer)) {
                ++pressureCount;
                break;
            }
        }
    }

    if (pressureCount <= 0) {
        return 0;
    }

    if (pressureCount == 1) {
        return 1;
    }

    return 2;
}

int QLearningGrid::CenterControlBucket(UnitFaction faction) const {
    const int ownCenter = CountBattleSoldiersInLane(faction, BattleLane::Center);
    const int enemyCenter =
        CountBattleSoldiersInLane(OppositeFaction(faction), BattleLane::Center);
    return AdvantageBucket(ownCenter, enemyCenter);
}

int QLearningGrid::BuildHeadquartersState(UnitFaction faction) const {
    const UnitFaction enemyFaction = OppositeFaction(faction);
    const int ownSoldiers = CountBattleSoldiers(faction);
    const int enemySoldiers = CountBattleSoldiers(enemyFaction);
    const int ownInitial = CountInitialBattleSoldiers(faction);
    const int enemyInitial = CountInitialBattleSoldiers(enemyFaction);

    int stateIndex = SoldierRatioBucket(ownSoldiers, ownInitial);
    stateIndex = stateIndex * kTacticalBucketCount +
                 SoldierRatioBucket(enemySoldiers, enemyInitial);
    stateIndex = stateIndex * kTacticalBucketCount +
                 AdvantageBucket(ownSoldiers, enemySoldiers);
    stateIndex = stateIndex * kTacticalBucketCount +
                 EnemyArcherPressureBucket(faction);
    stateIndex = stateIndex * kTacticalBucketCount +
                 CenterControlBucket(faction);
    return stateIndex;
}

int QLearningGrid::BuildDivisionState(
    UnitFaction faction,
    BattleLane lane,
    HeadquartersCommand headquartersCommand) const {
    const UnitFaction enemyFaction = OppositeFaction(faction);
    const int ownLaneSoldiers = CountBattleSoldiersInLane(faction, lane);
    const int enemyLaneSoldiers = CountBattleSoldiersInLane(enemyFaction, lane);
    const int ownInitial = CountInitialBattleSoldiersInLane(faction, lane);
    const int enemyInitial = CountInitialBattleSoldiersInLane(enemyFaction, lane);

    int stateIndex = LaneIndex(lane);
    stateIndex = stateIndex * kHeadquartersCommandCount +
                 static_cast<int>(headquartersCommand);
    stateIndex = stateIndex * kTacticalBucketCount +
                 SoldierRatioBucket(ownLaneSoldiers, ownInitial);
    stateIndex = stateIndex * kTacticalBucketCount +
                 SoldierRatioBucket(enemyLaneSoldiers, enemyInitial);
    stateIndex = stateIndex * kTacticalBucketCount +
                 AdvantageBucket(ownLaneSoldiers, enemyLaneSoldiers);
    return stateIndex;
}

int QLearningGrid::UnitMoveRange(UnitClass unitClass) const {
    switch (unitClass) {
    case UnitClass::Cavalry:
        return 2;
    case UnitClass::Infantry:
    case UnitClass::Archer:
    default:
        break;
    }

    return 1;
}

int QLearningGrid::UnitAttackRange(UnitClass unitClass) const {
    if (unitClass == UnitClass::Archer) {
        return 3;
    }

    return 1;
}

int QLearningGrid::UnitBaseDamage(UnitClass unitClass) const {
    switch (unitClass) {
    case UnitClass::Cavalry:
        return 18;
    case UnitClass::Infantry:
        return 13;
    case UnitClass::Archer:
        return 14;
    default:
        break;
    }

    return 12;
}

float QLearningGrid::UnitDefense(UnitClass unitClass) const {
    switch (unitClass) {
    case UnitClass::Cavalry:
        return 1.00f;
    case UnitClass::Infantry:
        return 1.25f;
    case UnitClass::Archer:
        return 0.85f;
    default:
        break;
    }

    return 1.00f;
}

float QLearningGrid::UnitMatchupMultiplier(
    UnitClass attackerClass,
    UnitClass defenderClass) const {
    if ((attackerClass == UnitClass::Cavalry && defenderClass == UnitClass::Archer) ||
        (attackerClass == UnitClass::Archer && defenderClass == UnitClass::Infantry) ||
        (attackerClass == UnitClass::Infantry && defenderClass == UnitClass::Cavalry)) {
        return 1.45f;
    }

    if ((attackerClass == UnitClass::Archer && defenderClass == UnitClass::Cavalry) ||
        (attackerClass == UnitClass::Infantry && defenderClass == UnitClass::Archer) ||
        (attackerClass == UnitClass::Cavalry && defenderClass == UnitClass::Infantry)) {
        return 0.72f;
    }

    return 1.00f;
}

int QLearningGrid::BattleDistance(const GridPoint& from, const GridPoint& to) const {
    return std::abs(from.x - to.x) + std::abs(from.y - to.y);
}

bool QLearningGrid::IsBattleWalkable(const GridPoint& point) const {
    if (!IsInside(point)) {
        return false;
    }

    const Tile tile = GetTile(point.x, point.y);
    return tile != Tile::Wall && tile != Tile::Pit;
}

bool QLearningGrid::IsBattleOccupied(
    const GridPoint& point,
    int ignoredUnitIndex) const {
    for (int unitIndex = 0; unitIndex < static_cast<int>(battleUnits_.size()); ++unitIndex) {
        if (unitIndex == ignoredUnitIndex) {
            continue;
        }

        const BattleUnit& unit = battleUnits_[static_cast<std::size_t>(unitIndex)];
        if (!unit.active || unit.count <= 0) {
            continue;
        }

        if (unit.position.x == point.x && unit.position.y == point.y) {
            return true;
        }
    }

    return false;
}

int QLearningGrid::SelectBattleTargetIndex(
    int unitIndex,
    HeadquartersCommand headquartersCommand,
    DivisionCommand divisionCommand) const {
    if (unitIndex < 0 || unitIndex >= static_cast<int>(battleUnits_.size())) {
        return -1;
    }

    const BattleUnit& attacker = battleUnits_[static_cast<std::size_t>(unitIndex)];
    const UnitFaction defenderFaction = OppositeFaction(attacker.faction);
    const BattleLane attackerLane = LaneForPoint(attacker.position);
    const float ownHpRatio =
        static_cast<float>(attacker.count) / static_cast<float>(std::max(1, attacker.maxCount));
    int bestTargetIndex = -1;
    float bestScore = std::numeric_limits<float>::max();

    /* 孤立度を事前に計算: 各敵の周囲に味方が何人いるか */
    std::vector<int> allyCountNear(static_cast<std::size_t>(battleUnits_.size()), 0);
    for (int ti = 0; ti < static_cast<int>(battleUnits_.size()); ++ti) {
        const BattleUnit& t = battleUnits_[static_cast<std::size_t>(ti)];
        if (t.faction != defenderFaction || !t.active || t.count <= 0) {
            continue;
        }
        for (int ai = 0; ai < static_cast<int>(battleUnits_.size()); ++ai) {
            if (ai == ti) { continue; }
            const BattleUnit& a = battleUnits_[static_cast<std::size_t>(ai)];
            if (a.faction != defenderFaction || !a.active || a.count <= 0) {
                continue;
            }
            if (BattleDistance(t.position, a.position) <= 2) {
                allyCountNear[static_cast<std::size_t>(ti)]++;
            }
        }
    }

    for (int targetIndex = 0; targetIndex < static_cast<int>(battleUnits_.size()); ++targetIndex) {
        const BattleUnit& target = battleUnits_[static_cast<std::size_t>(targetIndex)];
        if (target.faction != defenderFaction || !target.active || target.count <= 0) {
            continue;
        }

        const int distance = BattleDistance(attacker.position, target.position);
        const float matchup = UnitMatchupMultiplier(attacker.unitClass, target.unitClass);
        const BattleLane targetLane = LaneForPoint(target.position);
        const float targetHpRatio =
            static_cast<float>(target.count) / static_cast<float>(std::max(1, target.maxCount));
        const int alliesNear = allyCountNear[static_cast<std::size_t>(targetIndex)];

        float score = static_cast<float>(distance) * 5.0f;
        score += targetHpRatio * 12.0f;           // 弱った敵を優先

        score -= static_cast<float>(std::min(3, alliesNear)) * 6.0f; // 孤立した敵を狙う

        /* 兵科相性を強く反映 */
        if (matchup > 1.00f) {
            score -= 28.0f;  // 有利相手は強く選好
        } else if (matchup < 1.00f) {
            score += 22.0f;  // 不利相手は避ける
        }

        /* 自部隊のHPが低いときは不利対面をさらに避ける */
        if (ownHpRatio < 0.40f && matchup < 1.00f) {
            score += 35.0f;
        }

        /* 兵科固有の選好 */
        if (attacker.unitClass == UnitClass::Cavalry) {
            if (target.unitClass == UnitClass::Archer) {
                score -= 35.0f;   // 騎馬は弓兵を追う
            } else if (target.unitClass == UnitClass::Infantry) {
                score += 18.0f;   // 騎馬は歩兵を避ける
            }
        } else if (attacker.unitClass == UnitClass::Archer) {
            if (target.unitClass == UnitClass::Infantry) {
                score -= 24.0f;   // 弓兵は歩兵を狙う
            } else if (target.unitClass == UnitClass::Cavalry) {
                score += 28.0f;   // 弓兵は騎馬を避ける
            }
            if (distance <= 1) {
                score += 30.0f;   // 弓兵は密着を避ける
            }
        } else if (attacker.unitClass == UnitClass::Infantry) {
            if (target.unitClass == UnitClass::Cavalry) {
                score -= 22.0f;   // 歩兵は騎馬を狙う
            } else if (target.unitClass == UnitClass::Archer) {
                score += 10.0f;   // 歩兵は弓兵に近づきすぎない
            }
        }

        /* 指揮命令の反映 */
        if ((headquartersCommand == HeadquartersCommand::LeftAttack &&
             targetLane == BattleLane::Left) ||
            (headquartersCommand == HeadquartersCommand::CenterAttack &&
             targetLane == BattleLane::Center) ||
            (headquartersCommand == HeadquartersCommand::RightAttack &&
             targetLane == BattleLane::Right)) {
            score -= 18.0f;
        }

        if (headquartersCommand == HeadquartersCommand::Flank &&
            target.unitClass == UnitClass::Archer) {
            score -= attacker.unitClass == UnitClass::Cavalry ? 28.0f : 10.0f;
        }

        if (divisionCommand == DivisionCommand::Support &&
            targetLane == attackerLane) {
            score -= 10.0f;
        } else if (divisionCommand == DivisionCommand::Flank &&
                   target.unitClass == UnitClass::Archer) {
            score -= 20.0f;
        } else if (divisionCommand == DivisionCommand::Retreat) {
            score -= static_cast<float>(std::max(0, 8 - distance));
        }

        if (distance <= UnitAttackRange(attacker.unitClass)) {
            score -= 6.0f;  // 射程内は少し優先
        }

        if (score < bestScore) {
            bestScore = score;
            bestTargetIndex = targetIndex;
        }
    }

    return bestTargetIndex;
}

GridPoint QLearningGrid::FindNextBattleStep(
    int unitIndex,
    const GridPoint& targetPoint,
    int attackRange) const {
    const BattleUnit& unit = battleUnits_[static_cast<std::size_t>(unitIndex)];
    const GridPoint start = unit.position;
    if (BattleDistance(start, targetPoint) <= attackRange) {
        return start;
    }

    const int cellCount = gridWidth_ * gridHeight_;
    const int startIndex = CellIndex(start);
    std::vector<int> parent(static_cast<std::size_t>(cellCount), -1);
    std::queue<GridPoint> frontier;
    GridPoint bestPoint = start;
    int bestDistance = BattleDistance(start, targetPoint);
    bool foundAttackPoint = false;

    parent[static_cast<std::size_t>(startIndex)] = startIndex;
    frontier.push(start);

    while (!frontier.empty() && !foundAttackPoint) {
        const GridPoint current = frontier.front();
        frontier.pop();

        const std::array<GridPoint, 4> neighbors = {
            GridPoint{current.x + 1, current.y},
            GridPoint{current.x, current.y + 1},
            GridPoint{current.x - 1, current.y},
            GridPoint{current.x, current.y - 1},
        };

        for (const GridPoint& next : neighbors) {
            if (!IsBattleWalkable(next) || IsBattleOccupied(next, unitIndex)) {
                continue;
            }

            const int nextIndex = CellIndex(next);
            if (parent[static_cast<std::size_t>(nextIndex)] >= 0) {
                continue;
            }

            parent[static_cast<std::size_t>(nextIndex)] = CellIndex(current);
            const int nextDistance = BattleDistance(next, targetPoint);
            if (nextDistance < bestDistance) {
                bestDistance = nextDistance;
                bestPoint = next;
            }

            if (nextDistance <= attackRange) {
                bestPoint = next;
                foundAttackPoint = true;
                break;
            }

            frontier.push(next);
        }
    }

    if (bestPoint.x == start.x && bestPoint.y == start.y) {
        return start;
    }

    int currentIndex = CellIndex(bestPoint);
    while (parent[static_cast<std::size_t>(currentIndex)] != startIndex &&
           parent[static_cast<std::size_t>(currentIndex)] >= 0) {
        currentIndex = parent[static_cast<std::size_t>(currentIndex)];
    }

    return {currentIndex % gridWidth_, currentIndex / gridWidth_};
}

void QLearningGrid::MoveBattleUnitToward(
    int unitIndex,
    const GridPoint& targetPoint) {
    BattleUnit& unit = battleUnits_[static_cast<std::size_t>(unitIndex)];
    const int moveRange = UnitMoveRange(unit.unitClass);
    const int attackRange = UnitAttackRange(unit.unitClass);

    for (int moveStep = 0; moveStep < moveRange; ++moveStep) {
        if (BattleDistance(unit.position, targetPoint) <= attackRange) {
            break;
        }

        const GridPoint next = FindNextBattleStep(unitIndex, targetPoint, attackRange);
        if (next.x == unit.position.x && next.y == unit.position.y) {
            break;
        }

        unit.position = next;
    }
}

void QLearningGrid::MoveArcherAwayFromTarget(
    int unitIndex,
    const GridPoint& targetPoint) {
    BattleUnit& unit = battleUnits_[static_cast<std::size_t>(unitIndex)];
    if (unit.unitClass != UnitClass::Archer) {
        return;
    }

    /* 弓兵は最適射程(3)を維持するように動く:
       2以下なら後退、3なら維持、4以上なら前進 */
    constexpr int kOptimalRange = 3;
    const int currentDistance = BattleDistance(unit.position, targetPoint);
    if (currentDistance == kOptimalRange) {
        return;
    }

    const int moveRange = UnitMoveRange(unit.unitClass);
    for (int step = 0; step < moveRange; ++step) {
        const int dist = BattleDistance(unit.position, targetPoint);
        if (dist == kOptimalRange) { break; }

        GridPoint bestPoint = unit.position;
        int bestDist = dist;
        const std::array<GridPoint, 4> candidates = {
            GridPoint{unit.position.x + 1, unit.position.y},
            GridPoint{unit.position.x, unit.position.y + 1},
            GridPoint{unit.position.x - 1, unit.position.y},
            GridPoint{unit.position.x, unit.position.y - 1},
        };

        for (const GridPoint& candidate : candidates) {
            if (!IsBattleWalkable(candidate) || IsBattleOccupied(candidate, unitIndex)) {
                continue;
            }
            const int candidateDistance = BattleDistance(candidate, targetPoint);
            if (dist < kOptimalRange) {
                if (candidateDistance > bestDist) {
                    bestDist = candidateDistance;
                    bestPoint = candidate;
                }
            } else {
                if (candidateDistance < bestDist && candidateDistance >= kOptimalRange) {
                    bestDist = candidateDistance;
                    bestPoint = candidate;
                }
            }
        }

        if (bestPoint.x == unit.position.x && bestPoint.y == unit.position.y) {
            break;
        }
        unit.position = bestPoint;
    }
}

void QLearningGrid::MoveBattleUnitAwayFromTarget(
    int unitIndex,
    const GridPoint& targetPoint,
    int safeDistance) {
    BattleUnit& unit = battleUnits_[static_cast<std::size_t>(unitIndex)];
    const int moveRange = UnitMoveRange(unit.unitClass);

    for (int moveStep = 0; moveStep < moveRange; ++moveStep) {
        if (BattleDistance(unit.position, targetPoint) >= safeDistance) {
            break;
        }

        GridPoint bestPoint = unit.position;
        int bestDistance = BattleDistance(unit.position, targetPoint);
        const std::array<GridPoint, 4> candidates = {
            GridPoint{unit.position.x + 1, unit.position.y},
            GridPoint{unit.position.x, unit.position.y + 1},
            GridPoint{unit.position.x - 1, unit.position.y},
            GridPoint{unit.position.x, unit.position.y - 1},
        };

        for (const GridPoint& candidate : candidates) {
            if (!IsBattleWalkable(candidate) || IsBattleOccupied(candidate, unitIndex)) {
                continue;
            }

            const int candidateDistance = BattleDistance(candidate, targetPoint);
            if (candidateDistance > bestDistance) {
                bestDistance = candidateDistance;
                bestPoint = candidate;
            }
        }

        if (bestPoint.x == unit.position.x && bestPoint.y == unit.position.y) {
            break;
        }

        unit.position = bestPoint;
    }
}

void QLearningGrid::MoveCavalryFlank(
    int unitIndex,
    const GridPoint& targetPoint) {
    BattleUnit& unit = battleUnits_[static_cast<std::size_t>(unitIndex)];
    if (unit.unitClass != UnitClass::Cavalry) {
        return;
    }

    const int moveRange = UnitMoveRange(unit.unitClass);
    for (int step = 0; step < moveRange; ++step) {
        if (BattleDistance(unit.position, targetPoint) <= UnitAttackRange(unit.unitClass)) {
            break;
        }

        /* 側面移動を優先: 上下方向にずれてから前進 */
        const int dx = targetPoint.x - unit.position.x;
        const int dy = targetPoint.y - unit.position.y;
        GridPoint bestPoint = unit.position;
        int bestDist = BattleDistance(unit.position, targetPoint);

        /* まず横方向(上下)を試す */
        const std::array<GridPoint, 2> lateralMoves = {
            GridPoint{unit.position.x, unit.position.y + (dy >= 0 ? 1 : -1)},
            GridPoint{unit.position.x, unit.position.y + (dy >= 0 ? -1 : 1)},
        };
        for (const GridPoint& lat : lateralMoves) {
            if (!IsBattleWalkable(lat) || IsBattleOccupied(lat, unitIndex)) { continue; }
            const int latDist = BattleDistance(lat, targetPoint);
            if (latDist < bestDist) {
                bestDist = latDist;
                bestPoint = lat;
            }
        }

        /* 横がダメなら縦方向(前進) */
        if (bestPoint.x == unit.position.x && bestPoint.y == unit.position.y) {
            GridPoint forward = unit.position;
            if (std::abs(dx) >= std::abs(dy)) {
                forward.x += (dx >= 0 ? 1 : -1);
            } else {
                forward.y += (dy >= 0 ? 1 : -1);
            }
            if (IsBattleWalkable(forward) && !IsBattleOccupied(forward, unitIndex)) {
                bestPoint = forward;
            }
        }

        if (bestPoint.x == unit.position.x && bestPoint.y == unit.position.y) {
            break;
        }
        unit.position = bestPoint;
    }
}

void QLearningGrid::MoveInfantryProtect(
    int unitIndex,
    const GridPoint& targetPoint,
    const std::vector<int>& archerIndices) {
    BattleUnit& unit = battleUnits_[static_cast<std::size_t>(unitIndex)];
    if (unit.unitClass != UnitClass::Infantry) {
        return;
    }

    /* 近くに味方弓兵がいる場合、弓兵と敵の間に割り込むように動く */
    GridPoint nearestArcherPos{-1, -1};
    int nearestArcherDist = 999;
    for (int ai : archerIndices) {
        const BattleUnit& archer = battleUnits_[static_cast<std::size_t>(ai)];
        if (!archer.active || archer.count <= 0) { continue; }
        const int d = BattleDistance(unit.position, archer.position);
        if (d < nearestArcherDist) {
            nearestArcherDist = d;
            nearestArcherPos = archer.position;
        }
    }

    if (nearestArcherPos.x < 0 || nearestArcherDist > 3) {
        return;
    }

    /* 弓兵と敵の間に割り込む位置を探す */
    const GridPoint mid = {
        (nearestArcherPos.x + targetPoint.x) / 2,
        (nearestArcherPos.y + targetPoint.y) / 2,
    };

    GridPoint bestPoint = unit.position;
    int bestDist = BattleDistance(unit.position, targetPoint);
    const std::array<GridPoint, 4> candidates = {
        GridPoint{unit.position.x + 1, unit.position.y},
        GridPoint{unit.position.x - 1, unit.position.y},
        GridPoint{unit.position.x, unit.position.y + 1},
        GridPoint{unit.position.x, unit.position.y - 1},
    };
    for (const GridPoint& c : candidates) {
        if (!IsBattleWalkable(c) || IsBattleOccupied(c, unitIndex)) { continue; }
        const int dMid = BattleDistance(c, mid);
        if (dMid < bestDist) {
            bestDist = dMid;
            bestPoint = c;
        }
    }

    if (bestPoint.x != unit.position.x || bestPoint.y != unit.position.y) {
        unit.position = bestPoint;
    }
}

bool QLearningGrid::ResolveBattleAttack(int attackerIndex, int targetIndex) {
    if (attackerIndex < 0 || targetIndex < 0 ||
        attackerIndex >= static_cast<int>(battleUnits_.size()) ||
        targetIndex >= static_cast<int>(battleUnits_.size())) {
        return false;
    }

    BattleUnit& attacker = battleUnits_[static_cast<std::size_t>(attackerIndex)];
    BattleUnit& target = battleUnits_[static_cast<std::size_t>(targetIndex)];
    if (!attacker.active || attacker.count <= 0 ||
        !target.active || target.count <= 0) {
        return false;
    }

    if (BattleDistance(attacker.position, target.position) >
        UnitAttackRange(attacker.unitClass)) {
        return false;
    }

    const int safeMaxCount = std::max(1, attacker.maxCount);
    const float countRate =
        Clamp01(static_cast<float>(attacker.count) / static_cast<float>(safeMaxCount));
    const float rawDamage =
        static_cast<float>(UnitBaseDamage(attacker.unitClass)) *
        (0.45f + countRate * 0.75f) *
        UnitMatchupMultiplier(attacker.unitClass, target.unitClass) /
        UnitDefense(target.unitClass);
    const int damage = std::min(target.count, std::max(1, static_cast<int>(std::lround(rawDamage))));

    battleAttackTraces_.push_back(
        {
            attacker.position,
            target.position,
            attacker.faction,
            10,
        });

    target.count -= damage;
    target.hitFlashTimer = 8;
    target.hitShakeTimer = 6;
    target.hitByPlayer = attacker.faction == UnitFaction::Player;
    if (target.count <= 0) {
        target.count = 0;
        target.active = false;
    }

    episodeReward_ += attacker.faction == UnitFaction::Player
                          ? static_cast<float>(damage)
                          : -static_cast<float>(damage);
    return true;
}

void QLearningGrid::AdvanceBattleEffects() {
    for (BattleUnit& unit : battleUnits_) {
        if (unit.hitFlashTimer > 0) {
            unit.hitFlashTimer -= 1;
        }

        if (unit.hitShakeTimer > 0) {
            unit.hitShakeTimer -= 1;
        }
    }

    for (BattleAttackTrace& trace : battleAttackTraces_) {
        if (trace.timer > 0) {
            trace.timer -= 1;
        }
    }

    battleAttackTraces_.erase(
        std::remove_if(
            battleAttackTraces_.begin(),
            battleAttackTraces_.end(),
            [](const BattleAttackTrace& trace) {
                return trace.timer <= 0;
            }),
        battleAttackTraces_.end());
}

QLearningGrid::TacticalDecision QLearningGrid::SelectTacticalDecision(UnitFaction faction) {
    const int factionIndex = FactionIndex(faction);
    TacticalDecision decision = {};

    decision.ownSoldiersBefore = CountBattleSoldiers(faction);
    decision.enemySoldiersBefore = CountBattleSoldiers(OppositeFaction(faction));
    decision.headquartersState = BuildHeadquartersState(faction);
    decision.headquartersCommand = static_cast<HeadquartersCommand>(
        SelectTacticalAction(
            headquartersQ_[static_cast<std::size_t>(factionIndex)],
            decision.headquartersState,
            kHeadquartersCommandCount));
    currentHeadquartersCommands_[static_cast<std::size_t>(factionIndex)] =
        decision.headquartersCommand;

    for (int laneIndex = 0; laneIndex < kBattleLaneCount; ++laneIndex) {
        const BattleLane lane = static_cast<BattleLane>(laneIndex);
        decision.ownLaneSoldiersBefore[static_cast<std::size_t>(laneIndex)] =
            CountBattleSoldiersInLane(faction, lane);
        decision.enemyLaneSoldiersBefore[static_cast<std::size_t>(laneIndex)] =
            CountBattleSoldiersInLane(OppositeFaction(faction), lane);
        decision.divisionStates[static_cast<std::size_t>(laneIndex)] =
            BuildDivisionState(faction, lane, decision.headquartersCommand);
        decision.divisionCommands[static_cast<std::size_t>(laneIndex)] =
            static_cast<DivisionCommand>(
                SelectTacticalAction(
                    divisionQ_[static_cast<std::size_t>(factionIndex)],
                    decision.divisionStates[static_cast<std::size_t>(laneIndex)],
                    kDivisionCommandCount));
        currentDivisionCommands_[static_cast<std::size_t>(factionIndex)]
                                [static_cast<std::size_t>(laneIndex)] =
            decision.divisionCommands[static_cast<std::size_t>(laneIndex)];
    }

    return decision;
}

void QLearningGrid::UpdateTacticalDecision(
    UnitFaction faction,
    const TacticalDecision& decision,
    bool done,
    bool ownWin) {
    const int factionIndex = FactionIndex(faction);
    const UnitFaction enemyFaction = OppositeFaction(faction);
    const int ownSoldiersAfter = CountBattleSoldiers(faction);
    const int enemySoldiersAfter = CountBattleSoldiers(enemyFaction);
    const int ownLoss = std::max(0, decision.ownSoldiersBefore - ownSoldiersAfter);
    const int enemyLoss = std::max(0, decision.enemySoldiersBefore - enemySoldiersAfter);

    float headquartersReward =
        static_cast<float>(enemyLoss) * 0.45f -
        static_cast<float>(ownLoss) * 0.65f;
    if (done) {
        headquartersReward += ownWin ? 60.0f : -60.0f;
    }

    UpdateTacticalQ(
        headquartersQ_[static_cast<std::size_t>(factionIndex)],
        decision.headquartersState,
        static_cast<int>(decision.headquartersCommand),
        headquartersReward,
        BuildHeadquartersState(faction),
        done,
        kHeadquartersCommandCount);

    for (int laneIndex = 0; laneIndex < kBattleLaneCount; ++laneIndex) {
        const BattleLane lane = static_cast<BattleLane>(laneIndex);
        const int ownLaneAfter = CountBattleSoldiersInLane(faction, lane);
        const int enemyLaneAfter = CountBattleSoldiersInLane(enemyFaction, lane);
        const int ownLaneLoss = std::max(
            0,
            decision.ownLaneSoldiersBefore[static_cast<std::size_t>(laneIndex)] -
                ownLaneAfter);
        const int enemyLaneLoss = std::max(
            0,
            decision.enemyLaneSoldiersBefore[static_cast<std::size_t>(laneIndex)] -
                enemyLaneAfter);
        float divisionReward =
            static_cast<float>(enemyLaneLoss) * 0.60f -
            static_cast<float>(ownLaneLoss) * 0.80f;

        if (done) {
            divisionReward += ownWin ? 18.0f : -18.0f;
        }

        UpdateTacticalQ(
            divisionQ_[static_cast<std::size_t>(factionIndex)],
            decision.divisionStates[static_cast<std::size_t>(laneIndex)],
            static_cast<int>(decision.divisionCommands[static_cast<std::size_t>(laneIndex)]),
            divisionReward,
            BuildDivisionState(faction, lane, decision.headquartersCommand),
            done,
            kDivisionCommandCount);
    }
}

void QLearningGrid::StepBattle() {
    if (battleUnits_.empty()) {
        ResetEpisode();
    }

    AdvanceBattleEffects();

    ++episodeSteps_;
    ++totalStepCount_;

    //========================================
    // 将軍AI: Q学習で行動選択
    //========================================

    struct GeneralPlan {
        int generalUnitIndex = -1;
        int targetIndex = -1;
        AIIntent tactic = AIIntent::Advance;
    };
    std::vector<GeneralPlan> plans;

    /* 前ターンの将軍Q学習を更新 (state → nextState) */
    for (GeneralQLearningState& past : generalQLearningStates_) {
        const BattleUnit& gen = battleUnits_[static_cast<std::size_t>(past.unitIndex)];
        if (!gen.active || gen.count <= 0 || !gen.isGeneral) {
            continue;
        }
        const int ownAfter = CountGeneralGroupSoldiers(past.unitIndex);
        int enemyAfter = past.enemySoldiersBefore; /* fallback */
        if (past.enemyGeneralUnitIndex >= 0) {
            const BattleUnit& enemyGen = battleUnits_[static_cast<std::size_t>(past.enemyGeneralUnitIndex)];
            if (enemyGen.active && enemyGen.count > 0) {
                enemyAfter = CountGeneralGroupSoldiers(past.enemyGeneralUnitIndex);
            }
        }
        /* 報酬 = 敵減少数 × 0.5 - 自減少数 × 0.7 */
        const int ownLoss = std::max(0, past.ownSoldiersBefore - ownAfter);
        const int enemyLoss = std::max(0, past.enemySoldiersBefore - enemyAfter);
        const float reward = static_cast<float>(enemyLoss) * 0.5f -
                             static_cast<float>(ownLoss) * 0.7f;

        const int ownMax = CountInitialBattleSoldiers(gen.faction);
        const int enemyMax = CountInitialBattleSoldiers(
            gen.faction == UnitFaction::Player ? UnitFaction::Enemy : UnitFaction::Player);
        /* 将軍Index */
        int generalIdx = 0;
        {
            int idx = 0;
            for (int gi = 0; gi < static_cast<int>(battleUnits_.size()); ++gi) {
                const BattleUnit& u = battleUnits_[static_cast<std::size_t>(gi)];
                if (u.active && u.count > 0 && u.isGeneral && u.faction == gen.faction) {
                    if (u.id == gen.id) { generalIdx = std::min(1, idx); break; }
                    ++idx;
                }
            }
        }
        /* 最寄りの敵将軍までの距離 */
        int minDist = 999;
        for (const BattleUnit& u : battleUnits_) {
            if (!u.active || u.count <= 0 || !u.isGeneral || u.faction == gen.faction) { continue; }
            minDist = std::min(minDist, BattleDistance(gen.position, u.position));
        }
        const int nextState = BuildGeneralState(
            ownAfter, ownMax, enemyAfter, enemyMax, minDist < 999 ? minDist : 99, generalIdx);

        const int fi = FactionIndex(gen.faction);
        UpdateGeneralQ(fi, past.stateIndex, past.actionIndex, reward, nextState, false);
    }
    generalQLearningStates_.clear();

    /* 各将軍の状態を構築し、Q学習で行動選択 */
    int generalCount[2] = {0, 0};
    for (int ui = 0; ui < static_cast<int>(battleUnits_.size()); ++ui) {
        const BattleUnit& gen = battleUnits_[static_cast<std::size_t>(ui)];
        if (!gen.active || gen.count <= 0 || !gen.isGeneral) {
            continue;
        }

        const int fi = FactionIndex(gen.faction);
        const int gi = generalCount[fi]++;  /* 将軍Index */

        const int ownSoldiers = CountGeneralGroupSoldiers(ui);
        const int ownMax = CountInitialBattleSoldiers(gen.faction);

        /* 最寄りの敵将軍を探す */
        int nearestEnemyGenIndex = -1;
        int minDist = 999;
        int enemyGroupSoldiers = 0;
        int enemyMax = CountInitialBattleSoldiers(
            gen.faction == UnitFaction::Player ? UnitFaction::Enemy : UnitFaction::Player);
        for (int ti = 0; ti < static_cast<int>(battleUnits_.size()); ++ti) {
            const BattleUnit& enemyGen = battleUnits_[static_cast<std::size_t>(ti)];
            if (!enemyGen.active || enemyGen.count <= 0 || !enemyGen.isGeneral ||
                enemyGen.faction == gen.faction) {
                continue;
            }
            const int d = BattleDistance(gen.position, enemyGen.position);
            if (d < minDist) {
                minDist = d;
                nearestEnemyGenIndex = ti;
                enemyGroupSoldiers = CountGeneralGroupSoldiers(ti);
            }
        }

        const int stateIndex = BuildGeneralState(
            ownSoldiers, ownMax,
            enemyGroupSoldiers, enemyMax,
            minDist < 999 ? minDist : 99,
            gi);
        const int actionIndex = SelectGeneralAction(stateIndex, fi);

        /* 行動 → ターゲットと戦術へマップ */
        GeneralPlan plan;
        plan.generalUnitIndex = ui;

        /* 将軍単位で前ターンQ更新用に退避 */
        GeneralQLearningState qState;
        qState.unitIndex = ui;
        qState.stateIndex = stateIndex;
        qState.actionIndex = actionIndex;
        qState.ownSoldiersBefore = ownSoldiers;
        qState.enemyGeneralUnitIndex = nearestEnemyGenIndex;
        qState.enemySoldiersBefore = enemyGroupSoldiers;
        generalQLearningStates_.push_back(qState);

        switch (actionIndex) {
        case 0: /* Advance: 最寄りの敵将軍を狙う */
            plan.targetIndex = nearestEnemyGenIndex;
            plan.tactic = AIIntent::Advance;
            break;
        case 1: /* Hold: 現在位置維持 */
            plan.targetIndex = nearestEnemyGenIndex;
            plan.tactic = AIIntent::Hold;
            break;
        case 2: /* Retreat: 後退 */
            plan.targetIndex = nearestEnemyGenIndex;
            plan.tactic = AIIntent::Retreat;
            break;
        case 3: /* Flank: 敵将軍の近くの弓兵を狙う */
            plan.targetIndex = nearestEnemyGenIndex;
            plan.tactic = AIIntent::Flank;
            break;
        case 4: /* Support: 味方将軍支援（近い味方のターゲットを継承） */
        {
            /* 一番近い味方将軍を探す */
            int nearestAllyGenIdx = -1;
            int nearestAllyDist = 999;
            for (int ai = 0; ai < static_cast<int>(battleUnits_.size()); ++ai) {
                if (ai == ui) { continue; }
                const BattleUnit& ally = battleUnits_[static_cast<std::size_t>(ai)];
                if (!ally.active || ally.count <= 0 || !ally.isGeneral ||
                    ally.faction != gen.faction) {
                    continue;
                }
                const int d = BattleDistance(gen.position, ally.position);
                if (d < nearestAllyDist) {
                    nearestAllyDist = d;
                    nearestAllyGenIdx = ai;
                }
            }
            /* 支援先のターゲットをそのまま使う（plan.targetIndexは後で上書き） */
            plan.targetIndex = (nearestAllyGenIdx >= 0) ? nearestAllyGenIdx : nearestEnemyGenIndex;
            plan.tactic = AIIntent::Support;
            break;
        }
        case 5: /* HuntCavalry: 敵騎兵を優先 */
            plan.targetIndex = nearestEnemyGenIndex;
            plan.tactic = AIIntent::HuntArcher;
            break;
        default:
            plan.targetIndex = nearestEnemyGenIndex;
            plan.tactic = AIIntent::Advance;
            break;
        }

        plans.push_back(plan);
    }

    //========================================
    // 行動順ソート
    //========================================

    std::vector<int> turnOrder;
    turnOrder.reserve(battleUnits_.size());
    for (int unitIndex = 0; unitIndex < static_cast<int>(battleUnits_.size()); ++unitIndex) {
        const BattleUnit& unit = battleUnits_[static_cast<std::size_t>(unitIndex)];
        if (unit.active && unit.count > 0) {
            turnOrder.push_back(unitIndex);
        }
    }

    auto initiative = [](UnitClass unitClass) {
        switch (unitClass) {
        case UnitClass::Cavalry:
            return 3;
        case UnitClass::Archer:
            return 2;
        case UnitClass::Infantry:
        default:
            break;
        }
        return 1;
    };

    std::sort(
        turnOrder.begin(),
        turnOrder.end(),
        [&](int leftIndex, int rightIndex) {
            const BattleUnit& left = battleUnits_[static_cast<std::size_t>(leftIndex)];
            const BattleUnit& right = battleUnits_[static_cast<std::size_t>(rightIndex)];
            const int leftInitiative = initiative(left.unitClass);
            const int rightInitiative = initiative(right.unitClass);
            if (leftInitiative != rightInitiative) {
                return leftInitiative > rightInitiative;
            }
            return left.id < right.id;
        });

    //========================================
    // 本体ループ
    //========================================

    bool done = false;
    bool playerWin = false;
    const int playerUnitsBeforeTurn = CountActiveBattleUnits(UnitFaction::Player);
    const int enemyUnitsBeforeTurn = CountActiveBattleUnits(UnitFaction::Enemy);
    if (playerUnitsBeforeTurn <= 0 || enemyUnitsBeforeTurn <= 0) {
        done = true;
        playerWin = playerUnitsBeforeTurn > 0;
    }

    /* 弓兵インデックス事前収集 */
    std::vector<int> archerIndices;
    for (int ui = 0; ui < static_cast<int>(battleUnits_.size()); ++ui) {
        const BattleUnit& u = battleUnits_[static_cast<std::size_t>(ui)];
        if (u.active && u.count > 0 && u.unitClass == UnitClass::Archer) {
            archerIndices.push_back(ui);
        }
    }

    /* 将軍計画を unitIndex → plan へマップ */
    std::vector<int> unitToPlanIndex(static_cast<std::size_t>(battleUnits_.size()), -1);
    for (std::size_t pi = 0; pi < plans.size(); ++pi) {
        unitToPlanIndex[static_cast<std::size_t>(plans[pi].generalUnitIndex)] = static_cast<int>(pi);
    }

    /* 将軍 → targetIndex の逆引き */
    std::vector<int> generalTargetMap(static_cast<std::size_t>(battleUnits_.size()), -1);
    std::vector<AIIntent> generalTacticMap(static_cast<std::size_t>(battleUnits_.size()), AIIntent::Advance);
    for (const GeneralPlan& plan : plans) {
        if (plan.targetIndex >= 0) {
            generalTargetMap[static_cast<std::size_t>(plan.generalUnitIndex)] = plan.targetIndex;
            generalTacticMap[static_cast<std::size_t>(plan.generalUnitIndex)] = plan.tactic;
        }
    }

    for (int unitIndex : turnOrder) {
        if (done) { break; }

        BattleUnit& unit = battleUnits_[static_cast<std::size_t>(unitIndex)];
        if (!unit.active || unit.count <= 0) { continue; }

        //========================================
        // ターゲット決定
        //========================================

        int targetIndex = -1;
        if (unit.isGeneral) {
            /* 将軍自身 → 将軍計画のターゲット */
            targetIndex = generalTargetMap[static_cast<std::size_t>(unitIndex)];
        } else if (unit.commanderId >= 0) {
            /* 配下 → 将軍のターゲットを継承 */
            for (int gi = 0; gi < static_cast<int>(battleUnits_.size()); ++gi) {
                const BattleUnit& gen = battleUnits_[static_cast<std::size_t>(gi)];
                if (gen.id == unit.commanderId && gen.active && gen.count > 0) {
                    targetIndex = generalTargetMap[static_cast<std::size_t>(gi)];
                    break;
                }
            }
        }

        /* どの将軍配下でもない独立部隊 → 自力でターゲット選択 */
        if (targetIndex < 0) {
            targetIndex = SelectBattleTargetIndex(
                unitIndex,
                HeadquartersCommand::Balanced,
                DivisionCommand::Advance);
        }

        /* 将軍のターゲットが将軍の場合、そのグループの中で最も近い実ユニットを狙う */
        if (targetIndex >= 0) {
            const BattleUnit& targetGen = battleUnits_[static_cast<std::size_t>(targetIndex)];
            if (targetGen.isGeneral && targetGen.active && targetGen.count > 0) {
                int closestSub = targetIndex;
                int closestDist = BattleDistance(unit.position, targetGen.position);
                for (int ti = 0; ti < static_cast<int>(battleUnits_.size()); ++ti) {
                    const BattleUnit& tu = battleUnits_[static_cast<std::size_t>(ti)];
                    if (!tu.active || tu.count <= 0 || tu.faction == unit.faction) { continue; }
                    if (tu.commanderId == targetGen.id) {
                        const int d = BattleDistance(unit.position, tu.position);
                        if (d < closestDist) {
                            closestDist = d;
                            closestSub = ti;
                        }
                    }
                }
                targetIndex = closestSub;
            }
        }

        if (targetIndex < 0) {
            done = true;
            playerWin = unit.faction == UnitFaction::Player;
            break;
        }

        const BattleUnit& targetUnit = battleUnits_[static_cast<std::size_t>(targetIndex)];
        const GridPoint targetPoint = targetUnit.position;
        const UnitClass targetUnitClass = targetUnit.unitClass;
        const int attackRange = UnitAttackRange(unit.unitClass);
        const int targetDistance = BattleDistance(unit.position, targetPoint);
        const float matchup = UnitMatchupMultiplier(unit.unitClass, targetUnitClass);
        const bool disadvantage = matchup < 1.00f && targetDistance <= 2;

        /* この部隊の将軍の戦術を取得 */
        AIIntent tactic = AIIntent::Advance;
        if (unit.isGeneral) {
            tactic = generalTacticMap[static_cast<std::size_t>(unitIndex)];
        } else if (unit.commanderId >= 0) {
            for (int gi = 0; gi < static_cast<int>(battleUnits_.size()); ++gi) {
                const BattleUnit& gen = battleUnits_[static_cast<std::size_t>(gi)];
                if (gen.id == unit.commanderId && gen.active && gen.count > 0) {
                    tactic = generalTacticMap[static_cast<std::size_t>(gi)];
                    break;
                }
            }
        }

        /* 意図・ターゲットを保存（可視化用） */
        unit.targetId = targetUnit.id;
        unit.intent = tactic;

        //========================================
        // 兵科別行動
        //========================================

        /* 将軍命令: 後退 */
        if (tactic == AIIntent::Retreat) {
            unit.intent = AIIntent::Retreat;
            MoveBattleUnitAwayFromTarget(unitIndex, targetPoint, 4);
            ResolveBattleAttack(unitIndex, targetIndex);
            goto checkDone;
        }

        /* 不利相性かつ近い → 自発退避 */
        if (disadvantage) {
            if (unit.unitClass == UnitClass::Archer) {
                unit.intent = AIIntent::Kite;
                MoveArcherAwayFromTarget(unitIndex, targetPoint);
                ResolveBattleAttack(unitIndex, targetIndex);
                goto checkDone;
            }
            if (unit.unitClass == UnitClass::Cavalry && targetUnitClass == UnitClass::Infantry) {
                unit.intent = AIIntent::Retreat;
                MoveBattleUnitAwayFromTarget(unitIndex, targetPoint, 3);
                goto checkDone;
            }
        }

        /* 将軍命令: 維持 → 攻撃可能なら射撃、不可なら待機 */
        if (tactic == AIIntent::Hold) {
            if (targetDistance <= attackRange) {
                ResolveBattleAttack(unitIndex, targetIndex);
                goto checkDone;
            }
            unit.intent = AIIntent::Hold;
            goto checkDone;
        }

        /* 弓兵: 距離維持射撃（将軍命令に関わらず基本行動） */
        if (unit.unitClass == UnitClass::Archer) {
            unit.intent = AIIntent::Kite;
            MoveArcherAwayFromTarget(unitIndex, targetPoint);
            ResolveBattleAttack(unitIndex, targetIndex);
            goto checkDone;
        }

        /* 騎馬: 弓兵狙いなら側面、歩兵不利なら退避 */
        if (unit.unitClass == UnitClass::Cavalry) {
            if (targetUnitClass == UnitClass::Archer) {
                unit.intent = unit.isGeneral ? AIIntent::Advance : AIIntent::HuntArcher;
                MoveCavalryFlank(unitIndex, targetPoint);
            } else if (targetUnitClass == UnitClass::Infantry && matchup < 1.0f) {
                unit.intent = AIIntent::Flank;
                MoveBattleUnitToward(unitIndex, targetPoint);
            } else {
                unit.intent = AIIntent::Advance;
                MoveBattleUnitToward(unitIndex, targetPoint);
            }
            ResolveBattleAttack(unitIndex, targetIndex);
            goto checkDone;
        }

        /* 歩兵: 騎馬を狙う / 弓兵保護 / 前進 */
        if (unit.unitClass == UnitClass::Infantry) {
            bool nearArcher = false;
            for (int ai : archerIndices) {
                const BattleUnit& archer = battleUnits_[static_cast<std::size_t>(ai)];
                if (!archer.active || archer.count <= 0) { continue; }
                if (BattleDistance(unit.position, archer.position) <= 2) {
                    nearArcher = true;
                    break;
                }
            }

            if (nearArcher && targetUnitClass == UnitClass::Cavalry) {
                unit.intent = AIIntent::Protect;
                MoveInfantryProtect(unitIndex, targetPoint, archerIndices);
            } else {
                unit.intent = AIIntent::Advance;
                MoveBattleUnitToward(unitIndex, targetPoint);
            }
            ResolveBattleAttack(unitIndex, targetIndex);
            goto checkDone;
        }

        /* 将軍(兵種不問): 前進 */
        unit.intent = AIIntent::Advance;
        MoveBattleUnitToward(unitIndex, targetPoint);
        ResolveBattleAttack(unitIndex, targetIndex);

    checkDone:
        const int playerUnitCount = CountActiveBattleUnits(UnitFaction::Player);
        const int enemyUnitCount = CountActiveBattleUnits(UnitFaction::Enemy);
        if (playerUnitCount <= 0 || enemyUnitCount <= 0) {
            done = true;
            playerWin = playerUnitCount > 0;
            break;
        }
    }

    //========================================
    // タイムアウト
    //========================================

    if (!done && episodeSteps_ >= EpisodeStepLimit()) {
        const int playerSoldiers = CountBattleSoldiers(UnitFaction::Player);
        const int enemySoldiers = CountBattleSoldiers(UnitFaction::Enemy);
        done = true;
        playerWin = playerSoldiers >= enemySoldiers;
    }

    //========================================
    // エピソード終了: 将軍Q最終更新
    //========================================

    for (GeneralQLearningState& past : generalQLearningStates_) {
        const BattleUnit& gen = battleUnits_[static_cast<std::size_t>(past.unitIndex)];
        if (!gen.active || gen.count <= 0 || !gen.isGeneral) { continue; }
        const int ownAfter = CountGeneralGroupSoldiers(past.unitIndex);
        int enemyAfter = past.enemySoldiersBefore;
        if (past.enemyGeneralUnitIndex >= 0) {
            const BattleUnit& enemyGen = battleUnits_[static_cast<std::size_t>(past.enemyGeneralUnitIndex)];
            if (enemyGen.active && enemyGen.count > 0) {
                enemyAfter = CountGeneralGroupSoldiers(past.enemyGeneralUnitIndex);
            }
        }
        const int ownLoss = std::max(0, past.ownSoldiersBefore - ownAfter);
        const int enemyLoss = std::max(0, past.enemySoldiersBefore - enemyAfter);
        float reward = static_cast<float>(enemyLoss) * 0.5f - static_cast<float>(ownLoss) * 0.7f;
        if (done) {
            const bool genWon = (gen.faction == UnitFaction::Player && playerWin) ||
                                (gen.faction == UnitFaction::Enemy && !playerWin);
            reward += genWon ? 30.0f : -30.0f;
        }
        UpdateGeneralQ(FactionIndex(gen.faction), past.stateIndex, past.actionIndex, reward, 0, done);
    }
    generalQLearningStates_.clear();

    SyncLegacyPositionsFromBattleUnits();

    if (done) {
        episodeReward_ += playerWin ? 80.0f : -80.0f;
        FinishEpisode(playerWin);
    }
}

void QLearningGrid::Step() {
    StepBattle();
}

#if 0
void QLearningGrid::StepOldQLearning() {
    struct EnemyTransition {
        /* どの敵の遷移かを示し、未来状態の組み立て時に同じ個体を追跡します。 */
        std::size_t enemyIndex = 0;

        /* 行動選択時点での敵位置です。距離差報酬の元になる旧座標として残します。 */
        GridPoint state = {};

        /* 味方の被覆マスク込みで圧縮した、敵用 Q テーブルの状態添字です。 */
        int stateIndex = 0;

        /* その状態で選んだ行動です。 */
        Action action = Action::Up;

        /* 実際に盤面へ適用した結果を保持し、報酬計算へ使います。 */
        MoveResult move = {};
    };

    //========================================
    // プレイヤー行動選択
    //========================================

    /* プレイヤーは「自分の位置 + いま最も近い敵」を代表脅威として 1 手選びます。 */
    const GridPoint playerState = agent_;
    const GridPoint primaryEnemyState = SelectPrimaryEnemy(playerState, enemies_);
    const int playerStateIndex = JointStateIndex(playerState, primaryEnemyState);
    const Action playerAction = SelectAction(playerQ_, playerStateIndex);
    const MoveResult playerMove = SimulateMove(playerState, playerAction, false);
    GridPoint playerNext = playerMove.next;
    std::vector<GridPoint> nextEnemies = enemies_;
    std::vector<EnemyTransition> enemyTransitions;
    enemyTransitions.reserve(enemies_.size());

    //========================================
    // ステップ数更新
    //========================================

    ++episodeSteps_;
    ++totalStepCount_;

    //========================================
    // プレイヤー報酬と終端判定
    //========================================

    float playerReward = 0.0f;
    bool done = false;
    bool success = false;
    bool enemyCaughtPlayer = false;

    int escapeRoutesBefore = 0;
    int adjacentCoverageBefore = 0;
    int approachDirectionsBefore = 0;
    int escapeRoutesAfter = 0;
    int adjacentCoverageAfter = 0;
    int approachDirectionsAfter = 0;

    /* まずプレイヤー自身の移動結果だけを採点し、Goal / Pit / 壁を処理します。 */
    if (playerMove.blocked) {
        playerReward = kWallPenalty;
    } else {
        playerReward = kStepReward + PlayerProgressReward(playerState, playerNext);
    }

    /* プレイヤーが敵のいるマスへ突っ込んだ場合は、その場で捕捉扱いです。 */
    if (ContainsEnemy(enemies_, playerNext)) {
        playerReward += kCaughtPenalty;
        done = true;
    } else if (!playerMove.blocked && playerMove.landedTile == Tile::Goal) {
        playerReward = kGoalReward + PlayerProgressReward(playerState, playerNext);
        done = true;
        success = true;
    } else if (!playerMove.blocked && playerMove.landedTile == Tile::Pit) {
        playerReward = kPitPenalty + PlayerProgressReward(playerState, playerNext);
        done = true;
    }

    //========================================
    // 敵行動選択
    //========================================

    if (!done) {
        /* 移動前後でプレイヤーの逃げ道がどれだけ減ったかを、連携報酬の基準として残します。 */
        escapeRoutesBefore = CountPlayerEscapeRoutes(playerNext, enemies_);
        adjacentCoverageBefore = CountAdjacentEnemyCoverage(playerNext, enemies_);
        approachDirectionsBefore = CountEnemyApproachDirections(playerNext, enemies_);

        /* 敵はプレイヤー移動後の位置を見て、自分ごとに味方の被覆状況込みで 1 手を選びます。 */
        for (std::size_t enemyIndex = 0; enemyIndex < enemies_.size(); ++enemyIndex) {
            const GridPoint enemyState = nextEnemies[enemyIndex];
            const int supportMask =
                BuildEnemySupportMask(playerNext, nextEnemies, enemyIndex);
            const int enemyStateIndex =
                EnemyStateIndex(playerNext, enemyState, supportMask);
            const Action enemyAction = SelectAction(enemyQ_, enemyStateIndex);

            std::vector<GridPoint> occupiedEnemies = nextEnemies;
            occupiedEnemies.erase(occupiedEnemies.begin() + static_cast<std::ptrdiff_t>(enemyIndex));

            const MoveResult enemyMove =
                SimulateEnemyMove(enemyState, enemyAction, occupiedEnemies);
            nextEnemies[enemyIndex] = enemyMove.next;
            enemyTransitions.push_back(
                {enemyIndex, enemyState, enemyStateIndex, enemyAction, enemyMove});

            /* その敵がプレイヤーを捕まえたら、この時点で敵報酬を上積みして終了です。 */
            if (enemyMove.next.x == playerNext.x && enemyMove.next.y == playerNext.y) {
                playerReward += kCaughtPenalty;
                done = true;
                enemyCaughtPlayer = true;
            }

            if (done) {
                break;
            }
        }

        /* 全敵が動いたあとで、逃げ道封鎖と包囲の進み具合をまとめて測ります。 */
        escapeRoutesAfter = CountPlayerEscapeRoutes(playerNext, nextEnemies);
        adjacentCoverageAfter = CountAdjacentEnemyCoverage(playerNext, nextEnemies);
        approachDirectionsAfter = CountEnemyApproachDirections(playerNext, nextEnemies);
    }

    //========================================
    // タイムアウト補正
    //========================================

    /* 手数上限を超えたら、そのターン終了時点でプレイヤー失敗 / 敵阻止成功として締めます。 */
    if (!done && episodeSteps_ >= EpisodeStepLimit()) {
        playerReward += kTimeoutPenalty;
        done = true;
    }

    //========================================
    // 敵側 Q 学習更新式
    //========================================

    if (!enemyTransitions.empty()) {
        /* チーム全体で逃げ道を減らした量、隣接包囲の増加量、方向分散の増加量を共有報酬にします。 */
        const float sharedSealReward =
            static_cast<float>(escapeRoutesBefore - escapeRoutesAfter) *
            kEnemySealEscapeReward;
        const float sharedAdjacencyReward =
            static_cast<float>(adjacentCoverageAfter - adjacentCoverageBefore) *
            kEnemyAdjacencyReward;
        const float sharedSpreadReward =
            static_cast<float>(approachDirectionsAfter - approachDirectionsBefore) *
            kEnemySpreadReward;
        const bool playerBlockedWithoutCatch = done && !success && !enemyCaughtPlayer;

        for (const EnemyTransition& transition : enemyTransitions) {
            const int previousDistance =
                std::abs(transition.state.x - playerNext.x) +
                std::abs(transition.state.y - playerNext.y);
            const int nextDistance =
                std::abs(transition.move.next.x - playerNext.x) +
                std::abs(transition.move.next.y - playerNext.y);

            /* 個別の接近報酬に加え、チーム全体の包囲成果も各敵へ同じように返します。 */
            float enemyReward = 0.0f;
            if (transition.move.blocked) {
                enemyReward = kEnemyWallPenalty;
            } else {
                enemyReward =
                    kEnemyStepReward +
                    static_cast<float>(previousDistance - nextDistance) * 0.12f;
            }
            enemyReward +=
                sharedSealReward + sharedAdjacencyReward + sharedSpreadReward;

            if (transition.move.next.x == playerNext.x &&
                transition.move.next.y == playerNext.y) {
                enemyReward += kEnemyCatchReward;
            } else if (success) {
                enemyReward += kEnemyGoalPenalty;
            } else if (playerBlockedWithoutCatch) {
                enemyReward += kEnemyBlockReward;
            }

            const int futureSupportMask =
                BuildEnemySupportMask(playerNext, nextEnemies, transition.enemyIndex);
            const int futureStateIndex = EnemyStateIndex(
                playerNext,
                nextEnemies[transition.enemyIndex],
                futureSupportMask);
            const float enemyFuture = done ? 0.0f : MaxQ(enemyQ_, futureStateIndex);
            const int enemyActionIndex = static_cast<int>(transition.action);
            float& enemyCurrent =
                enemyQ_[transition.stateIndex * kActionCount + enemyActionIndex];
            enemyCurrent +=
                kAlpha * (enemyReward + kGamma * enemyFuture - enemyCurrent);
        }
    }

    //========================================
    // Q 学習更新式
    //========================================

    /* プレイヤー側 Q は、自分の行動後に敵群が応答したあとの代表脅威状態へ寄せて更新します。 */
    const int playerActionIndex = static_cast<int>(playerAction);
    const GridPoint primaryEnemyNext = SelectPrimaryEnemy(playerNext, nextEnemies);
    const float playerFuture =
        done ? 0.0f : MaxQ(playerQ_, JointStateIndex(playerNext, primaryEnemyNext));
    float& playerCurrent = playerQ_[playerStateIndex * kActionCount + playerActionIndex];
    playerCurrent += kAlpha * (playerReward + kGamma * playerFuture - playerCurrent);

    //========================================
    // 現在状態反映
    //========================================

    /* 仮想遷移の結果を実際の現在位置へ反映し、プレイヤー累積報酬も積み増します。 */
    agent_ = playerNext;
    enemies_ = std::move(nextEnemies);
    episodeReward_ += playerReward;

    //========================================
    // エピソード終了処理
    //========================================

    if (done) {
        FinishEpisode(success);
    }
}
#endif

//========================================
// エピソード終了処理
//========================================

void QLearningGrid::FinishEpisode(bool success) {
    const int stepsThisEpisode = episodeSteps_;
    const float epsilonUsedThisEpisode = epsilon_;

    //========================================
    // 集計更新
    //========================================

    /* 完了件数、勝利件数、直近窓の内容を更新します。 */
    ++episodeCount_;
    if (success) {
        ++successfulEpisodeCount_;
    }

    lastEpisodeReward_ = episodeReward_;
    rewardWindow_[rewardWindowCursor_] = lastEpisodeReward_;
    successWindow_[rewardWindowCursor_] = success ? 1.0f : 0.0f;
    rewardWindowCursor_ = (rewardWindowCursor_ + 1) % kRewardWindowSize;
    rewardWindowCount_ = std::min(rewardWindowCount_ + 1, kRewardWindowSize);

    /* 戦術AIの探索率を少しずつ下げ、序盤は試行、後半は良い命令を優先します。 */
    epsilon_ = std::max(0.04f, epsilon_ * 0.996f);

    //========================================
    // 履歴レコード追加
    //========================================

    /* その時点の平均報酬や成功率をスナップショットとして履歴へ残します。 */
    episodeHistory_.push_back({
        episodeCount_,
        epsilonUsedThisEpisode,
        GetAverageReward(),
        GetRecentSuccessRate(),
        GetPlayerSoldierCount() - GetEnemySoldierCount(),
        stepsThisEpisode,
    });

    //========================================
    // コンソール進捗表示
    //========================================

    /* 一定間隔または成功時だけ進捗を出し、学習の改善を追いやすくします。 */
    if ((episodeCount_ % 10) == 0 || success) {
        std::cout << "Episode " << episodeCount_
                  << " | epsilon=" << std::fixed << std::setprecision(3)
                  << epsilon_
                  << " | avg=" << std::setprecision(2) << GetAverageReward()
                  << " | success=" << std::setprecision(2)
                  << GetRecentSuccessRate()
                  << " | enemy-win=" << std::setprecision(2)
                  << GetRecentBlockedRate()
                  << " | player-soldiers=" << GetPlayerSoldierCount()
                  << " | enemy-soldiers=" << GetEnemySoldierCount();

        std::cout << '\n';
    }

    ResetEpisode();
}

void QLearningGrid::ResetEpisode() {
    /* 次の試行は必ず LDtk の部隊初期配置から始めるため、位置・手数・累積報酬を初期化します。 */
    agent_ = start_;
    battleUnits_ = initialBattleUnits_;
    battleAttackTraces_.clear();
    currentHeadquartersCommands_ = {
        HeadquartersCommand::Balanced,
        HeadquartersCommand::Balanced,
    };
    for (std::array<DivisionCommand, kBattleLaneCount>& divisionCommands : currentDivisionCommands_) {
        divisionCommands.fill(DivisionCommand::Hold);
    }

    if (enemyStarts_.empty()) {
        enemyStarts_ = ChooseDefaultEnemyStarts();
    }
    enemies_ = enemyStarts_;
    SyncLegacyPositionsFromBattleUnits();
    episodeSteps_ = 0;
    episodeReward_ = 0.0f;
}
