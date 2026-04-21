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

    /* エンティティを内部形式へ詰め直す */
    std::vector<std::pair<std::string, GridPoint>> entities;
    entities.reserve(level.entities.size());
    for (const LdtkEntityData& entity : level.entities) {
        entities.push_back({entity.identifier, {entity.gridX, entity.gridY}});
    }

    //========================================
    // 内部マップへ反映
    //========================================

    ApplyLoadedMap(
        level.identifier,
        level.gridWidth,
        level.gridHeight,
        level.intGridCsv,
        entities);

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

    usingLdtkMap_ = false;
    loadedLevelName_ = "BuiltIn";
}

void QLearningGrid::ApplyLoadedMap(
    const std::string& levelIdentifier,
    int gridWidth,
    int gridHeight,
    const std::vector<int>& intGridCsv,
    const std::vector<std::pair<std::string, GridPoint>>& entities) {
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

    for (const auto& entity : entities) {
        const std::string& identifier = entity.first;
        const GridPoint point = entity.second;

        if (point.x < 0 || point.x >= gridWidth ||
            point.y < 0 || point.y >= gridHeight) {
            continue;
        }

        const int stateIndex = point.y * gridWidth + point.x;

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
        } else if (IsEnemyIdentifier(identifier)) {
            if (std::find_if(
                    loadedEnemyStarts.begin(),
                    loadedEnemyStarts.end(),
                    [&point](const GridPoint& current) {
                        return current.x == point.x && current.y == point.y;
                    }) == loadedEnemyStarts.end()) {
                loadedEnemyStarts.push_back(point);
            }
        }
    }

    //========================================
    // 最低条件確認
    //========================================

    if (!hasStart || !hasGoal) {
        throw std::runtime_error("LDtk map must contain both Start and Goal.");
    }

    /* 最後に Start と Goal を再代入し、他レイヤーの上書きで消えないよう確定させます。 */
    loadedTiles[static_cast<std::size_t>(loadedStart.y * gridWidth + loadedStart.x)] = Tile::Start;
    loadedTiles[static_cast<std::size_t>(loadedGoal.y * gridWidth + loadedGoal.x)] = Tile::Goal;

    gridWidth_ = gridWidth;
    gridHeight_ = gridHeight;
    tiles_ = std::move(loadedTiles);
    start_ = loadedStart;
    goal_ = loadedGoal;
    agent_ = start_;

    /* 新しい地形へ切り替わったので、Goal までの距離地図も張り直します。 */
    RebuildGoalDistanceMap();

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
    rewardWindow_.fill(0.0f);
    successWindow_.fill(0.0f);

    /* 画面表示や CSV で使う集計値も、最初から学習し直す前提で初期化します。 */
    episodeCount_ = 0;
    successfulEpisodeCount_ = 0;
    episodeSteps_ = 0;
    totalStepCount_ = 0;
    rewardWindowCursor_ = 0;
    rewardWindowCount_ = 0;
    epsilon_ = 1.0f;
    episodeReward_ = 0.0f;
    lastEpisodeReward_ = 0.0f;
    episodeHistory_.clear();

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

void QLearningGrid::Step() {
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

//========================================
// エピソード終了処理
//========================================

void QLearningGrid::FinishEpisode(bool success) {
    const int stepsThisEpisode = episodeSteps_;
    const float epsilonUsedThisEpisode = epsilon_;

    //========================================
    // 集計更新
    //========================================

    /* 完了件数、成功件数、直近窓の内容を更新し、次エピソード向けに epsilon も減衰させます。 */
    ++episodeCount_;
    if (success) {
        ++successfulEpisodeCount_;
    }

    lastEpisodeReward_ = episodeReward_;
    rewardWindow_[rewardWindowCursor_] = lastEpisodeReward_;
    successWindow_[rewardWindowCursor_] = success ? 1.0f : 0.0f;
    rewardWindowCursor_ = (rewardWindowCursor_ + 1) % kRewardWindowSize;
    rewardWindowCount_ = std::min(rewardWindowCount_ + 1, kRewardWindowSize);

    /* 大きいマップでは減衰を緩め、経路未発見の間は探索率が落ち切らないよう下限も上げます。 */
    epsilon_ = std::max(MinimumExplorationRate(), epsilon_ * EpsilonDecayFactor());

    //========================================
    // 履歴レコード追加
    //========================================

    /* その時点の平均報酬や成功率をスナップショットとして履歴へ残します。 */
    episodeHistory_.push_back({
        episodeCount_,
        epsilonUsedThisEpisode,
        GetAverageReward(),
        GetRecentSuccessRate(),
        MeasureGreedyPathLength(),
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
                  << " | blocked=" << std::setprecision(2)
                  << GetRecentBlockedRate();

        const int greedyPath = MeasureGreedyPathLength();
        if (greedyPath >= 0) {
            std::cout << " | greedy-path=" << greedyPath;
        } else {
            std::cout << " | greedy-path=not-found";
        }

        std::cout << '\n';
    }

    ResetEpisode();
}

void QLearningGrid::ResetEpisode() {
    /* 次の試行は必ず Start と敵開始位置列から始めるため、位置・手数・累積報酬を初期化します。 */
    agent_ = start_;
    if (enemyStarts_.empty()) {
        enemyStarts_ = ChooseDefaultEnemyStarts();
    }
    enemies_ = enemyStarts_;
    episodeSteps_ = 0;
    episodeReward_ = 0.0f;
}
