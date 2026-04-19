#include "QLearningGrid.h"

#include "LdtkLoader.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {

std::wstring ToWide(const std::string& text) {
    return std::wstring(text.begin(), text.end());
}

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

bool IsStartIdentifier(const std::string& identifier) {
    return identifier == "Start";
}

bool IsGoalIdentifier(const std::string& identifier) {
    return identifier == "Goal";
}

bool IsPitIdentifier(const std::string& identifier) {
    return identifier == "Pit";
}

bool IsWallIdentifier(const std::string& identifier) {
    return identifier == "Wall";
}

}  // namespace

QLearningGrid::QLearningGrid() : rng_(std::random_device{}()) {
    configuredLdtkPath_ = std::filesystem::path("maps") / "qlearning_demo.ldtk";
    configuredLevelIndex_ = 0;

    ResetMap();
    ResetLearningState();

    try {
        if (std::filesystem::exists(configuredLdtkPath_)) {
            LoadMapFromLdtk(configuredLdtkPath_, configuredLevelIndex_);
        }
    } catch (const std::exception& exception) {
        ResetMap();
        ResetLearningState();
        usingLdtkMap_ = false;
        loadedLevelName_ = "BuiltIn";
        std::cout << "LDtk map load skipped: " << exception.what() << '\n';
    }
}

void QLearningGrid::ReloadMapFromLdtk() {
    if (configuredLdtkPath_.empty()) {
        throw std::runtime_error("LDtk file path is not configured.");
    }

    LoadMapFromLdtk(configuredLdtkPath_, configuredLevelIndex_);
}

void QLearningGrid::LoadMapFromLdtk(
    const std::filesystem::path& filePath,
    int levelIndex) {
    const LdtkProjectData project = LoadLdtkProject(filePath);
    if (levelIndex < 0 || levelIndex >= static_cast<int>(project.levels.size())) {
        throw std::runtime_error("LDtk level index is out of range.");
    }

    const LdtkLevelData& level = project.levels[static_cast<std::size_t>(levelIndex)];

    std::vector<std::pair<std::string, GridPoint>> entities;
    entities.reserve(level.entities.size());
    for (const LdtkEntityData& entity : level.entities) {
        entities.push_back({entity.identifier, {entity.gridX, entity.gridY}});
    }

    ApplyLoadedMap(
        level.identifier,
        level.gridWidth,
        level.gridHeight,
        level.intGridCsv,
        entities);

    configuredLdtkPath_ = filePath;
    configuredLevelIndex_ = levelIndex;
    usingLdtkMap_ = true;
    ResetLearningState();
}

void QLearningGrid::Train(int steps) {
    for (int index = 0; index < steps; ++index) {
        Step();
    }
}

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
    if (!usingLdtkMap_) {
        return L"BuiltIn";
    }

    std::wstring result = configuredLdtkPath_.filename().wstring();
    if (!loadedLevelName_.empty()) {
        result += L" / ";
        result += ToWide(loadedLevelName_);
    }
    return result;
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

    for (int actionIndex = 0; actionIndex < kActionCount; ++actionIndex) {
        const float qValue = q_[state * kActionCount + actionIndex];
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
    if (rewardWindowCount_ <= 0) {
        return 0.0f;
    }

    float total = 0.0f;
    for (int index = 0; index < rewardWindowCount_; ++index) {
        total += rewardWindow_[index];
    }
    return total / static_cast<float>(rewardWindowCount_);
}

float QLearningGrid::GetRecentSuccessRate() const {
    if (rewardWindowCount_ <= 0) {
        return 0.0f;
    }

    float total = 0.0f;
    for (int index = 0; index < rewardWindowCount_; ++index) {
        total += successWindow_[index];
    }
    return total / static_cast<float>(rewardWindowCount_);
}

const std::vector<QLearningGrid::EpisodeRecord>& QLearningGrid::GetEpisodeHistory() const {
    return episodeHistory_;
}

int QLearningGrid::MeasureGreedyPathLength() const {
    const std::vector<GridPoint> path = BuildGreedyPath();
    if (!path.empty() &&
        path.back().x == goal_.x &&
        path.back().y == goal_.y) {
        return static_cast<int>(path.size()) - 1;
    }
    return -1;
}

std::vector<GridPoint> QLearningGrid::BuildGreedyPath(int maxSteps) const {
    std::vector<GridPoint> path;
    path.reserve(static_cast<std::size_t>(maxSteps) + 1);

    std::array<bool, kGridWidth * kGridHeight> visited = {};
    GridPoint position = start_;
    path.push_back(position);

    for (int step = 0; step < maxSteps; ++step) {
        if (position.x == goal_.x && position.y == goal_.y) {
            break;
        }

        const int state = StateIndex(position);
        if (visited[state]) {
            break;
        }
        visited[state] = true;

        const StepResult result =
            Simulate(position, GetBestAction(position.x, position.y));
        if (result.next.x == position.x && result.next.y == position.y) {
            break;
        }

        path.push_back(result.next);
        if (GetTile(result.next.x, result.next.y) == Tile::Pit) {
            break;
        }

        position = result.next;
    }

    return path;
}

int QLearningGrid::StateIndex(const GridPoint& point) const {
    return point.y * kGridWidth + point.x;
}

bool QLearningGrid::IsInside(const GridPoint& point) const {
    return point.x >= 0 && point.x < kGridWidth &&
           point.y >= 0 && point.y < kGridHeight;
}

void QLearningGrid::ResetMap() {
    tiles_.fill(Tile::Empty);

    start_ = {0, 0};
    goal_ = {9, 9};
    agent_ = start_;

    tiles_[StateIndex(start_)] = Tile::Start;
    tiles_[StateIndex(goal_)] = Tile::Goal;
    tiles_[StateIndex({6, 4})] = Tile::Pit;
    tiles_[StateIndex({8, 3})] = Tile::Pit;

    for (int y = 0; y < kGridHeight; ++y) {
        if (y != 4) {
            tiles_[StateIndex({3, y})] = Tile::Wall;
        }
    }

    for (int x = 5; x <= 8; ++x) {
        tiles_[StateIndex({x, 6})] = Tile::Wall;
    }

    usingLdtkMap_ = false;
    loadedLevelName_ = "BuiltIn";
}

void QLearningGrid::ApplyLoadedMap(
    const std::string& levelIdentifier,
    int gridWidth,
    int gridHeight,
    const std::vector<int>& intGridCsv,
    const std::vector<std::pair<std::string, GridPoint>>& entities) {
    if (gridWidth != kGridWidth || gridHeight != kGridHeight) {
        throw std::runtime_error("LDtk level size must be 10x10 for this demo.");
    }

    tiles_.fill(Tile::Empty);

    bool hasStart = false;
    bool hasGoal = false;

    for (int index = 0; index < static_cast<int>(intGridCsv.size()); ++index) {
        const int x = index % gridWidth;
        const int y = index / gridWidth;

        const Tile converted =
            ConvertIntGridValue(intGridCsv[static_cast<std::size_t>(index)]);
        if (converted == Tile::Empty) {
            continue;
        }

        tiles_[StateIndex({x, y})] = converted;
        if (converted == Tile::Start) {
            start_ = {x, y};
            hasStart = true;
        } else if (converted == Tile::Goal) {
            goal_ = {x, y};
            hasGoal = true;
        }
    }

    for (const auto& entity : entities) {
        const std::string& identifier = entity.first;
        const GridPoint point = entity.second;

        if (!IsInside(point)) {
            continue;
        }

        if (IsStartIdentifier(identifier)) {
            start_ = point;
            tiles_[StateIndex(point)] = Tile::Start;
            hasStart = true;
        } else if (IsGoalIdentifier(identifier)) {
            goal_ = point;
            tiles_[StateIndex(point)] = Tile::Goal;
            hasGoal = true;
        } else if (IsPitIdentifier(identifier)) {
            tiles_[StateIndex(point)] = Tile::Pit;
        } else if (IsWallIdentifier(identifier)) {
            tiles_[StateIndex(point)] = Tile::Wall;
        }
    }

    if (!hasStart || !hasGoal) {
        throw std::runtime_error("LDtk map must contain both Start and Goal.");
    }

    tiles_[StateIndex(start_)] = Tile::Start;
    tiles_[StateIndex(goal_)] = Tile::Goal;
    agent_ = start_;
    loadedLevelName_ = levelIdentifier;
}

void QLearningGrid::ResetLearningState() {
    q_.fill(0.0f);
    rewardWindow_.fill(0.0f);
    successWindow_.fill(0.0f);

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

float QLearningGrid::MaxQ(const GridPoint& point) const {
    const int state = StateIndex(point);
    float bestValue = std::numeric_limits<float>::lowest();

    for (int actionIndex = 0; actionIndex < kActionCount; ++actionIndex) {
        bestValue = std::max(bestValue, q_[state * kActionCount + actionIndex]);
    }

    return bestValue;
}

Action QLearningGrid::RandomAction() {
    std::uniform_int_distribution<int> distribution(0, kActionCount - 1);
    return static_cast<Action>(distribution(rng_));
}

Action QLearningGrid::SelectGreedyAction(const GridPoint& point) {
    const int state = StateIndex(point);
    float bestValue = std::numeric_limits<float>::lowest();
    std::array<Action, kActionCount> candidates = {};
    int candidateCount = 0;

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

    std::uniform_int_distribution<int> distribution(0, candidateCount - 1);
    return candidates[distribution(rng_)];
}

Action QLearningGrid::SelectAction(const GridPoint& point) {
    if (randomUnit_(rng_) < epsilon_) {
        return RandomAction();
    }
    return SelectGreedyAction(point);
}

QLearningGrid::StepResult QLearningGrid::Simulate(
    const GridPoint& point,
    Action action) const {
    GridPoint next = point;

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

    if (!IsInside(next) || GetTile(next.x, next.y) == Tile::Wall) {
        return {point, kWallPenalty, false};
    }

    const Tile tile = GetTile(next.x, next.y);
    const int previousDistance =
        std::abs(point.x - goal_.x) + std::abs(point.y - goal_.y);
    const int nextDistance =
        std::abs(next.x - goal_.x) + std::abs(next.y - goal_.y);
    const float progressReward =
        static_cast<float>(previousDistance - nextDistance) * 0.08f;

    if (tile == Tile::Goal) {
        return {next, kGoalReward + progressReward, true};
    }
    if (tile == Tile::Pit) {
        return {next, kPitPenalty + progressReward, true};
    }

    return {next, kStepReward + progressReward, false};
}

void QLearningGrid::Step() {
    const GridPoint statePoint = agent_;
    const int state = StateIndex(statePoint);
    const Action action = SelectAction(statePoint);
    const StepResult simulated = Simulate(statePoint, action);

    ++episodeSteps_;
    ++totalStepCount_;

    float reward = simulated.reward;
    bool done = simulated.done;
    if (!done && episodeSteps_ >= kMaxEpisodeSteps) {
        reward += kTimeoutPenalty;
        done = true;
    }

    const int actionIndex = static_cast<int>(action);
    const float future = done ? 0.0f : MaxQ(simulated.next);
    float& current = q_[state * kActionCount + actionIndex];
    current += kAlpha * (reward + kGamma * future - current);

    agent_ = simulated.next;
    episodeReward_ += reward;

    if (done) {
        const bool success =
            GetTile(simulated.next.x, simulated.next.y) == Tile::Goal;
        FinishEpisode(success);
    }
}

void QLearningGrid::FinishEpisode(bool success) {
    const int stepsThisEpisode = episodeSteps_;
    const float epsilonUsedThisEpisode = epsilon_;

    ++episodeCount_;
    if (success) {
        ++successfulEpisodeCount_;
    }

    lastEpisodeReward_ = episodeReward_;
    rewardWindow_[rewardWindowCursor_] = lastEpisodeReward_;
    successWindow_[rewardWindowCursor_] = success ? 1.0f : 0.0f;
    rewardWindowCursor_ = (rewardWindowCursor_ + 1) % kRewardWindowSize;
    rewardWindowCount_ = std::min(rewardWindowCount_ + 1, kRewardWindowSize);
    epsilon_ = std::max(kMinEpsilon, epsilon_ * kEpsilonDecay);

    episodeHistory_.push_back({
        episodeCount_,
        epsilonUsedThisEpisode,
        GetAverageReward(),
        GetRecentSuccessRate(),
        MeasureGreedyPathLength(),
        stepsThisEpisode,
    });

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
    agent_ = start_;
    episodeSteps_ = 0;
    episodeReward_ = 0.0f;
}
