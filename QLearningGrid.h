#pragma once

#include "SharedTypes.h"

#include <array>
#include <filesystem>
#include <queue>
#include <random>
#include <string>
#include <utility>
#include <vector>

//========================================
// QLearningGrid クラス宣言
//========================================
// Q 学習そのもの、グリッドマップ状態、履歴記録をまとめて扱う中心クラスです。

class QLearningGrid {
public:
    //========================================
    // 学習履歴レコード
    //========================================

    struct EpisodeRecord {
        /* CSV や画面表示で行を識別するための通し番号です。 */
        int episode = 0;

        /* このエピソード開始時点で使っていた探索率を残します。 */
        float epsilon = 0.0f;

        /* 直近ウィンドウ内の報酬平均で、学習の安定度を見る指標です。 */
        float averageReward = 0.0f;

        /* 直近ウィンドウ内でゴール到達できた割合です。 */
        float recentSuccessRate = 0.0f;

        /* 現在の最良行動だけで進んだときの経路長です。未到達なら -1 です。 */
        int bestPathLength = -1;

        /* そのエピソードを終了させるまでに消費した手数です。 */
        int steps = 0;
    };

    //========================================
    // 生成とマップ読み込み
    //========================================

    /* 組み込みマップを初期化し、必要なら外部 LDtk を読み込みます。 */
    QLearningGrid();

    /* 前回保存したパスとレベル番号で、同じマップを読み直します。 */
    void ReloadMapFromLdtk();

    /* LDtk を内部グリッドへ変換し、学習状態もそのサイズに合わせて初期化し直します。 */
    void LoadMapFromLdtk(const std::filesystem::path& filePath, int levelIndex = 0);

    //========================================
    // 学習更新
    //========================================

    /* Step を指定回数だけ回し、Q テーブルと統計を前へ進めます。 */
    void Train(int steps);

    //========================================
    // グリッド参照
    //========================================

    /* 指定座標のマス種別を返し、壁・穴・ゴール判定の基礎情報に使います。 */
    Tile GetTile(int x, int y) const;

    /* 現在エピソードでエージェントが立っている座標を返します。 */
    GridPoint GetAgent() const;

    /* 毎エピソードの開始位置として使う Start 座標を返します。 */
    GridPoint GetStart() const;

    /* 到達目標として扱う Goal 座標を返します。 */
    GridPoint GetGoal() const;

    /* そのマスが Goal か Pit かを調べ、エピソード終了マスかどうかを返します。 */
    bool IsTerminal(int x, int y) const;

    /* 現在使用中のマップ名を、タイトル表示用の文字列として組み立てます。 */
    std::wstring GetMapDisplayName() const;

    /* 現在読み込まれているマップの横マス数を返します。 */
    int GetGridWidth() const;

    /* 現在読み込まれているマップの縦マス数を返します。 */
    int GetGridHeight() const;

    //========================================
    // Q 値参照
    //========================================

    /* 指定マス・指定行動に対応する生の Q 値をそのまま読み出します。 */
    float GetQValue(int x, int y, Action action) const;

    /* そのマスで取りうる 4 行動のうち、最大の Q 値だけを返します。 */
    float GetBestValue(int x, int y) const;

    /* そのマスで現在もっとも評価の高い行動を 1 つ選んで返します。 */
    Action GetBestAction(int x, int y) const;

    //========================================
    // 統計参照
    //========================================

    /* これまで完了した総エピソード数を返します。 */
    int GetEpisodeCount() const;

    /* ゴール到達で終わった成功エピソード数を返します。 */
    int GetSuccessfulEpisodeCount() const;

    /* 進行中エピソードで現在何手進んでいるかを返します。 */
    int GetCurrentEpisodeSteps() const;

    /* アプリ起動後から累計で何 Step 学習したかを返します。 */
    int GetTrainingStepCount() const;

    /* 現在の探索率 epsilon を返し、探索の残り具合を確認できるようにします。 */
    float GetEpsilon() const;

    /* 直前に終了したエピソードの累積報酬を返します。 */
    float GetLastEpisodeReward() const;

    /* 直近ウィンドウに入っている報酬の平均値を返します。 */
    float GetAverageReward() const;

    /* 直近ウィンドウに入っている成功フラグから成功率を計算して返します。 */
    float GetRecentSuccessRate() const;

    /* CSV 出力や画面表示で使う履歴レコード列を参照用に返します。 */
    const std::vector<EpisodeRecord>& GetEpisodeHistory() const;

    //========================================
    // 経路確認
    //========================================

    /* 現在の greedy 方策だけで進んだとき、ゴールまで届く経路長を測ります。 */
    int MeasureGreedyPathLength() const;

    /* Start から greedy 行動だけをたどった仮想経路を、座標列として組み立てます。 */
    std::vector<GridPoint> BuildGreedyPath(int maxSteps = -1) const;

private:
    //========================================
    // 1 ステップ結果
    //========================================

    struct StepResult {
        GridPoint next;
        float reward;
        bool done;
    };

    //========================================
    // 学習係数
    //========================================

    /* 新しい観測結果を、既存の Q 値へどれだけ強く反映するかを決めます。 */
    static constexpr float kAlpha = 0.16f;

    /* 1 手先ではなく、その先に続く将来価値をどこまで重視するかを決めます。 */
    static constexpr float kGamma = 0.95f;

    /* 一度経路を見つけた後でも、完全固定化を避けるために残す探索率の下限です。 */
    static constexpr float kMinEpsilon = 0.00f;

    /* 10x10 前後の小さめマップで使う基準の探索率減衰係数です。 */
    static constexpr float kBaseEpsilonDecay = 0.998f;

    //========================================
    // 報酬設定
    //========================================

    /* 何も起きない通常移動にも小さな負コストを付け、遠回りを抑えます。 */
    static constexpr float kStepReward = -0.04f;

    /* 盤外や壁へぶつかった手は、その場足踏みとして強めに減点します。 */
    static constexpr float kWallPenalty = -0.80f;

    /* ゴール到達時に与える大きな正報酬で、目標を明確にします。 */
    static constexpr float kGoalReward = 10.0f;

    /* 落とし穴へ入ったときに与える負報酬で、危険マスを避けさせます。 */
    static constexpr float kPitPenalty = -8.0f;

    /* 手数上限で打ち切られたエピソードへ、停滞分の追加減点を入れます。 */
    static constexpr float kTimeoutPenalty = -1.5f;

    //========================================
    // 進行制限
    //========================================

    /* 小さいマップでも学習が打ち切られすぎないようにする最低手数上限です。 */
    static constexpr int kBaseMaxEpisodeSteps = 200;

    /* 平均報酬や成功率を、直近何件の履歴で平滑化するかを表します。 */
    static constexpr int kRewardWindowSize = 30;

    //========================================
    // 内部処理
    //========================================

    /* 2 次元座標を 1 次元配列の添字へ変換します。 */
    int StateIndex(const GridPoint& point) const;

    /* マスが盤面の外へはみ出していないかを判定します。 */
    bool IsInside(const GridPoint& point) const;

    /* 組み込みマップを再構築し、固定の開始位置と障害物を置き直します。 */
    void ResetMap();

    /* LDtk の IntGrid とエンティティを統合し、内部マップを作り直します。 */
    void ApplyLoadedMap(
        const std::string& levelIdentifier,
        int gridWidth,
        int gridHeight,
        const std::vector<int>& intGridCsv,
        const std::vector<std::pair<std::string, GridPoint>>& entities);

    /* Q テーブル、統計窓、現在エピソードをすべて初期状態へ戻します。 */
    void ResetLearningState();

    /* Goal から逆向きに最短距離を張り、進捗報酬の基準になる距離表を作ります。 */
    void RebuildGoalDistanceMap();

    /* そのマスから Goal までの最短距離を返します。到達不能なら -1 です。 */
    int GoalDistance(const GridPoint& point) const;

    /* 現在のマップサイズに応じて、1 エピソードの手数上限を動的に決めます。 */
    int EpisodeStepLimit() const;

    /* マップ面積に応じて、探索率の減衰速度を少し遅くした係数を返します。 */
    float EpsilonDecayFactor() const;

    /* 経路未発見の間は探索を止めすぎないよう、状況に応じた探索率下限を返します。 */
    float MinimumExplorationRate() const;

    /* そのマスで取りうる 4 行動のうち、最大 Q 値だけを抜き出します。 */
    float MaxQ(const GridPoint& point) const;

    /* 探索用に 4 方向から完全ランダムで 1 手を選びます。 */
    Action RandomAction();

    /* 最大 Q 値の行動だけを候補に残し、同点ならランダムで 1 つ選びます。 */
    Action SelectGreedyAction(const GridPoint& point);

    /* epsilon-greedy に従い、探索手か活用手かを切り替えて 1 手を選びます。 */
    Action SelectAction(const GridPoint& point);

    /* 指定行動を仮想的に 1 手だけ進め、次座標・報酬・終了判定を組み立てます。 */
    StepResult Simulate(const GridPoint& point, Action action) const;

    /* 1 手ぶんの行動選択、Q 更新、終了判定をまとめて進めます。 */
    void Step();

    /* エピソード終了時に履歴と統計を更新し、次エピソードの準備をします。 */
    void FinishEpisode(bool success);

    /* エージェントを開始位置へ戻し、手数と累積報酬をリセットします。 */
    void ResetEpisode();

    //========================================
    // マップと Q テーブル
    //========================================

    /* 現在マップの横マス数です。LDtk 読み込み時に可変で変わります。 */
    int gridWidth_ = kBuiltInGridWidth;

    /* 現在マップの縦マス数です。LDtk 読み込み時に可変で変わります。 */
    int gridHeight_ = kBuiltInGridHeight;

    /* 現在マップの各マス種別を 1 次元配列で保持します。 */
    std::vector<Tile> tiles_;

    /* 各マス × 各行動の Q 値本体です。学習はここを書き換えて進みます。 */
    std::vector<float> q_;

    /* 各マスから Goal までの最短距離で、進捗報酬の地図として使います。 */
    std::vector<int> goalDistance_;

    /* 直近エピソードの報酬を循環バッファとして保持します。 */
    std::array<float, kRewardWindowSize> rewardWindow_ = {};

    /* 直近エピソードの成功 / 失敗を 0/1 で保持します。 */
    std::array<float, kRewardWindowSize> successWindow_ = {};

    //========================================
    // 主要座標
    //========================================

    /* 毎エピソードの開始位置です。ResetEpisode でここへ戻します。 */
    GridPoint start_ = {};

    /* 目標マスです。距離報酬や終了判定の基準として使います。 */
    GridPoint goal_ = {};

    /* 現在エピソードでエージェントが立っている座標です。 */
    GridPoint agent_ = {};

    //========================================
    // 乱数と探索制御
    //========================================

    /* 行動選択の探索や同値候補のタイブレークに使う乱数エンジンです。 */
    std::mt19937 rng_;

    /* epsilon 判定用に 0..1 の乱数を引く分布です。 */
    std::uniform_real_distribution<float> randomUnit_{0.0f, 1.0f};

    //========================================
    // 学習集計値
    //========================================

    /* これまで完了したエピソード総数です。 */
    int episodeCount_ = 0;

    /* そのうちゴール到達で終わったエピソード数です。 */
    int successfulEpisodeCount_ = 0;

    /* 現在エピソードの手数です。タイムアウト判定に使います。 */
    int episodeSteps_ = 0;

    /* アプリ起動後からの総 Step 回数です。 */
    int totalStepCount_ = 0;

    /* 次に履歴窓へ書き込む位置を指す循環バッファのカーソルです。 */
    int rewardWindowCursor_ = 0;

    /* 履歴窓に実際何件入っているかを表します。初期は満杯ではありません。 */
    int rewardWindowCount_ = 0;

    /* 現在の探索率で、SelectAction が探索 / 活用を決める基準です。 */
    float epsilon_ = 1.0f;

    /* 進行中エピソードでここまでに積み上がった累積報酬です。 */
    float episodeReward_ = 0.0f;

    /* 直前に終了したエピソードの最終報酬を保持します。 */
    float lastEpisodeReward_ = 0.0f;

    /* CSV 出力や画面表示で使う履歴ログです。 */
    std::vector<EpisodeRecord> episodeHistory_;

    //========================================
    // 外部マップ設定
    //========================================

    /* 再読み込み時に使う LDtk ファイルパスです。 */
    std::filesystem::path configuredLdtkPath_;

    /* 再読み込み時に使うレベル番号です。 */
    int configuredLevelIndex_ = 0;

    /* タイトル表示用に、現在読み込んでいるレベル名を残します。 */
    std::string loadedLevelName_ = "BuiltIn";

    /* 組み込みマップではなく外部 LDtk を使っているかどうかです。 */
    bool usingLdtkMap_ = false;
};
