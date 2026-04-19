#pragma once

#include "SharedTypes.h"

#include <array>
#include <filesystem>
#include <random>
#include <string>
#include <utility>
#include <vector>

class QLearningGrid {
public:
    struct EpisodeRecord {
        int episode = 0;
        float epsilon = 0.0f;
        float averageReward = 0.0f;
        float recentSuccessRate = 0.0f;
        int bestPathLength = -1;
        int steps = 0;
    };

    QLearningGrid();

    void ReloadMapFromLdtk();
    void LoadMapFromLdtk(const std::filesystem::path& filePath, int levelIndex = 0);

    void Train(int steps);

    Tile GetTile(int x, int y) const;
    GridPoint GetAgent() const;
    GridPoint GetStart() const;
    GridPoint GetGoal() const;
    bool IsTerminal(int x, int y) const;
    std::wstring GetMapDisplayName() const;

    float GetQValue(int x, int y, Action action) const;
    float GetBestValue(int x, int y) const;
    Action GetBestAction(int x, int y) const;

    int GetEpisodeCount() const;
    int GetSuccessfulEpisodeCount() const;
    int GetCurrentEpisodeSteps() const;
    int GetTrainingStepCount() const;
    float GetEpsilon() const;
    float GetLastEpisodeReward() const;
    float GetAverageReward() const;
    float GetRecentSuccessRate() const;
    const std::vector<EpisodeRecord>& GetEpisodeHistory() const;

    int MeasureGreedyPathLength() const;
    std::vector<GridPoint> BuildGreedyPath(int maxSteps = 128) const;

private:
    struct StepResult {
        GridPoint next;
        float reward;
        bool done;
    };

    static constexpr float kAlpha = 0.16f;
    static constexpr float kGamma = 0.95f;
    static constexpr float kMinEpsilon = 0.00f;
    static constexpr float kEpsilonDecay = 0.998f;

    static constexpr float kStepReward = -0.04f;
    static constexpr float kWallPenalty = -0.80f;
    static constexpr float kGoalReward = 10.0f;
    static constexpr float kPitPenalty = -8.0f;
    static constexpr float kTimeoutPenalty = -1.5f;

    static constexpr int kMaxEpisodeSteps = 120;
    static constexpr int kRewardWindowSize = 30;

    int StateIndex(const GridPoint& point) const;
    bool IsInside(const GridPoint& point) const;
    void ResetMap();
    void ApplyLoadedMap(
        const std::string& levelIdentifier,
        int gridWidth,
        int gridHeight,
        const std::vector<int>& intGridCsv,
        const std::vector<std::pair<std::string, GridPoint>>& entities);
    void ResetLearningState();

    float MaxQ(const GridPoint& point) const;
    Action RandomAction();
    Action SelectGreedyAction(const GridPoint& point);
    Action SelectAction(const GridPoint& point);
    StepResult Simulate(const GridPoint& point, Action action) const;
    void Step();
    void FinishEpisode(bool success);
    void ResetEpisode();

    std::array<Tile, kGridWidth * kGridHeight> tiles_ = {};
    std::array<float, kGridWidth * kGridHeight * kActionCount> q_ = {};
    std::array<float, kRewardWindowSize> rewardWindow_ = {};
    std::array<float, kRewardWindowSize> successWindow_ = {};

    GridPoint start_ = {};
    GridPoint goal_ = {};
    GridPoint agent_ = {};

    std::mt19937 rng_;
    std::uniform_real_distribution<float> randomUnit_{0.0f, 1.0f};

    int episodeCount_ = 0;
    int successfulEpisodeCount_ = 0;
    int episodeSteps_ = 0;
    int totalStepCount_ = 0;
    int rewardWindowCursor_ = 0;
    int rewardWindowCount_ = 0;
    float epsilon_ = 1.0f;
    float episodeReward_ = 0.0f;
    float lastEpisodeReward_ = 0.0f;
    std::vector<EpisodeRecord> episodeHistory_;

    std::filesystem::path configuredLdtkPath_;
    int configuredLevelIndex_ = 0;
    std::string loadedLevelName_ = "BuiltIn";
    bool usingLdtkMap_ = false;
};
