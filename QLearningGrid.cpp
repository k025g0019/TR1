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
    return tiles_[StateIndex({x, y})];
}

GridPoint QLearningGrid::GetAgent() const {
    return agent_;
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
    const int state = StateIndex({x, y});
    return q_[state * kActionCount + static_cast<int>(action)];
}

float QLearningGrid::GetBestValue(int x, int y) const {
    return MaxQ({x, y});
}

Action QLearningGrid::GetBestAction(int x, int y) const {
    const int state = StateIndex({x, y});
    float bestValue = std::numeric_limits<float>::lowest();
    Action bestAction = Action::Up;

    /* 4 方向を順番に見ていき、その時点で一番良い方向を保持し続けます。 */
    for (int actionIndex = 0; actionIndex < kActionCount; ++actionIndex) {
        const float qValue = q_[state * kActionCount + actionIndex];

        /* 今見ている方向の評価が暫定 1 位を上回ったら、その方向へ更新します。 */
        if (qValue > bestValue) {
            bestValue = qValue;
            bestAction = static_cast<Action>(actionIndex);
        }
    }

    return bestAction;
}

int QLearningGrid::GetEpisodeCount() const {
    return episodeCount_;
}

int QLearningGrid::GetSuccessfulEpisodeCount() const {
    return successfulEpisodeCount_;
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
        maxSteps = std::max(1, gridWidth_ * gridHeight_);
    }

    std::vector<GridPoint> path;
    path.reserve(static_cast<std::size_t>(maxSteps) + 1);

    /* Start から始めて、毎手「今いちばん良い行動」だけを仮想的に追いかけます。 */

    /* 同じマスへ戻り続ける無限ループを止めるため、訪問済みマスを記録します。 */
    std::vector<unsigned char> visited(static_cast<std::size_t>(gridWidth_ * gridHeight_), 0);
    GridPoint position = start_;
    path.push_back(position);

    for (int step = 0; step < maxSteps; ++step) {
        /* ゴールへ着いた時点で、この方策は最後までたどれたので終了です。 */
        if (position.x == goal_.x && position.y == goal_.y) {
            break;
        }

        /* 以前と同じマスへ戻ったら、その後も同じ循環に入るだけなので打ち切ります。 */
        const int state = StateIndex(position);
        if (visited[state]) {
            break;
        }
        visited[state] = 1;

        /* そのマスでの最善行動を 1 手だけ試し、次にどこへ進むかを見ます。 */
        const StepResult result =
            Simulate(position, GetBestAction(position.x, position.y));

        /* 壁などで座標が変わらないなら、方策が前へ進めていないので終了です。 */
        if (result.next.x == position.x && result.next.y == position.y) {
            break;
        }

        path.push_back(result.next);

        /* 落とし穴で終わる経路も「ここで途切れる方策」として記録して止めます。 */
        if (GetTile(result.next.x, result.next.y) == Tile::Pit) {
            break;
        }

        position = result.next;
    }

    return path;
}

//========================================
// マップ内部補助
//========================================

int QLearningGrid::StateIndex(const GridPoint& point) const {
    return point.y * gridWidth_ + point.x;
}

bool QLearningGrid::IsInside(const GridPoint& point) const {
    return point.x >= 0 && point.x < gridWidth_ &&
           point.y >= 0 && point.y < gridHeight_;
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

    /* 学習の基準になる開始点、到達点、危険マスをまず固定で置きます。 */
    tiles_[StateIndex(start_)] = Tile::Start;
    tiles_[StateIndex(goal_)] = Tile::Goal;
    tiles_[StateIndex({6, 4})] = Tile::Pit;
    tiles_[StateIndex({8, 3})] = Tile::Pit;

    /* 左側と右側を分断する縦壁を置き、1 箇所だけ通路を残します。 */
    for (int y = 0; y < gridHeight_; ++y) {
        if (y != 4) {
            tiles_[StateIndex({3, y})] = Tile::Wall;
        }
    }

    /* 下側にも横壁を足し、回り込みを学ばないと届かない形にします。 */
    for (int x = 5; x <= 8; ++x) {
        tiles_[StateIndex({x, 6})] = Tile::Wall;
    }

    /* Goal までの経路距離表も、このマップ配置に合わせて作り直します。 */
    RebuildGoalDistanceMap();

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
    loadedLevelName_ = levelIdentifier;
}

void QLearningGrid::ResetLearningState() {
    /* 以前の学習結果が新しいマップへ混ざらないよう、Q 値と履歴窓を丸ごと消します。 */
    q_.assign(static_cast<std::size_t>(gridWidth_ * gridHeight_ * kActionCount), 0.0f);
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
    goalDistance_[static_cast<std::size_t>(StateIndex(goal_))] = 0;
    frontier.push(goal_);

    while (!frontier.empty()) {
        const GridPoint current = frontier.front();
        frontier.pop();

        const int currentDistance = goalDistance_[static_cast<std::size_t>(StateIndex(current))];
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

            const int nextIndex = StateIndex(next);
            if (goalDistance_[static_cast<std::size_t>(nextIndex)] >= 0) {
                continue;
            }

            goalDistance_[static_cast<std::size_t>(nextIndex)] = currentDistance + 1;
            frontier.push(next);
        }
    }
}

int QLearningGrid::GoalDistance(const GridPoint& point) const {
    if (!IsInside(point)) {
        return -1;
    }
    return goalDistance_[static_cast<std::size_t>(StateIndex(point))];
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

float QLearningGrid::MaxQ(const GridPoint& point) const {
    const int state = StateIndex(point);
    float bestValue = std::numeric_limits<float>::lowest();

    /* そのマスにある 4 行動の Q 値を総当たりし、最大値だけを抜き出します。 */
    for (int actionIndex = 0; actionIndex < kActionCount; ++actionIndex) {
        bestValue = std::max(bestValue, q_[state * kActionCount + actionIndex]);
    }

    return bestValue;
}

Action QLearningGrid::RandomAction() {
    /* 探索時は価値を見ず、4 方向を均等確率で 1 つ選びます。 */
    std::uniform_int_distribution<int> distribution(0, kActionCount - 1);
    return static_cast<Action>(distribution(rng_));
}

Action QLearningGrid::SelectGreedyAction(const GridPoint& point) {
    const int state = StateIndex(point);
    float bestValue = std::numeric_limits<float>::lowest();
    std::array<Action, kActionCount> candidates = {};
    int candidateCount = 0;

    /* 4 方向を順番に見て、最大値を更新したら候補を入れ替え、同点なら候補へ追加します。 */
    for (int actionIndex = 0; actionIndex < kActionCount; ++actionIndex) {
        const float qValue = q_[state * kActionCount + actionIndex];
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

Action QLearningGrid::SelectAction(const GridPoint& point) {
    /* epsilon 未満なら探索、それ以外なら現在の最良手を採用する epsilon-greedy です。 */
    if (randomUnit_(rng_) < epsilon_) {
        return RandomAction();
    }
    return SelectGreedyAction(point);
}

QLearningGrid::StepResult QLearningGrid::Simulate(
    const GridPoint& point,
    Action action) const {
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

    /* 盤外か壁なら移動は成立しないので、座標は据え置きのまま壁ペナルティを適用します。 */
    if (!IsInside(next) || GetTile(next.x, next.y) == Tile::Wall) {
        return {point, kWallPenalty, false};
    }

    //========================================
    // 距離報酬と終端判定
    //========================================

    const Tile tile = GetTile(next.x, next.y);
    const int previousDistance = GoalDistance(point);
    const int nextDistance = GoalDistance(next);
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

    /* 終端マスならその専用報酬を返し、そうでなければ通常移動報酬で継続します。 */
    if (tile == Tile::Goal) {
        return {next, kGoalReward + progressReward, true};
    }
    if (tile == Tile::Pit) {
        return {next, kPitPenalty + progressReward, true};
    }

    return {next, kStepReward + progressReward, false};
}

void QLearningGrid::Step() {
    //========================================
    // 行動選択
    //========================================

    /* 現在位置を状態とし、そこから 1 手選んで仮想遷移を計算します。 */
    const GridPoint statePoint = agent_;
    const int state = StateIndex(statePoint);
    const Action action = SelectAction(statePoint);
    const StepResult simulated = Simulate(statePoint, action);

    //========================================
    // ステップ数更新
    //========================================

    ++episodeSteps_;
    ++totalStepCount_;

    //========================================
    // タイムアウト補正
    //========================================

    /* 手数上限を超えたら、通常遷移でも強制終了させて追加ペナルティを載せます。 */
    float reward = simulated.reward;
    bool done = simulated.done;
    if (!done && episodeSteps_ >= EpisodeStepLimit()) {
        reward += kTimeoutPenalty;
        done = true;
    }

    //========================================
    // Q 学習更新式
    //========================================

    /* 現在の Q 値を、観測した報酬 + 次状態の最大価値へ少しずつ寄せて更新します。 */
    const int actionIndex = static_cast<int>(action);
    const float future = done ? 0.0f : MaxQ(simulated.next);
    float& current = q_[state * kActionCount + actionIndex];
    current += kAlpha * (reward + kGamma * future - current);

    //========================================
    // 現在状態反映
    //========================================

    /* 仮想遷移の結果を実際の現在位置へ反映し、累積報酬も積み増します。 */
    agent_ = simulated.next;
    episodeReward_ += reward;

    //========================================
    // エピソード終了処理
    //========================================

    if (done) {
        /* 終端の理由がゴールかどうかを見て、成功 / 失敗として締めます。 */
        const bool success =
            GetTile(simulated.next.x, simulated.next.y) == Tile::Goal;
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
                  << GetRecentSuccessRate();

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
    /* 次の試行は必ず Start から始めるため、位置・手数・累積報酬を初期化します。 */
    agent_ = start_;
    episodeSteps_ = 0;
    episodeReward_ = 0.0f;
}
