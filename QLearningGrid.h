#pragma once

#include "SharedTypes.h"

#include <array>
#include <filesystem>
#include <queue>
#include <random>
#include <string>
#include <utility>
#include <vector>

struct LdtkEntityData;

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

    /* 現在エピソードで敵が立っている座標を返します。 */
    GridPoint GetEnemy() const;

    /* 現在エピソードで動いている全敵の座標列を返します。 */
    const std::vector<GridPoint>& GetEnemies() const;

    /* 現在の合戦に出ている全ユニットを返します。 */
    const std::vector<BattleUnit>& GetBattleUnits() const;

    /* 直近の攻撃演出線を返します。 */
    const std::vector<BattleAttackTrace>& GetBattleAttackTraces() const;

    /* 指定陣営の本部AIが現在出している命令を返します。 */
    HeadquartersCommand GetHeadquartersCommand(UnitFaction faction) const;

    /* 指定陣営・師団の師団長AIが現在出している命令を返します。 */
    DivisionCommand GetDivisionCommand(UnitFaction faction, BattleLane lane) const;

    /* 現在残っているプレイヤー側の部隊数を返します。 */
    int GetPlayerUnitCount() const;

    /* 現在残っている敵側の部隊数を返します。 */
    int GetEnemyUnitCount() const;

    /* 現在残っているプレイヤー側の総人数を返します。 */
    int GetPlayerSoldierCount() const;

    /* 現在残っている敵側の総人数を返します。 */
    int GetEnemySoldierCount() const;

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

    /* ゴールを阻止した失敗エピソード数を返します。 */
    int GetBlockedEpisodeCount() const;

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

    /* 直近ウィンドウでゴール阻止になった割合を返します。 */
    float GetRecentBlockedRate() const;

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

    struct MoveResult {
        GridPoint next;
        bool blocked = false;
        Tile landedTile = Tile::Empty;
    };

    struct TacticalDecision {
        int headquartersState = 0;
        HeadquartersCommand headquartersCommand = HeadquartersCommand::Balanced;
        std::array<int, kBattleLaneCount> divisionStates = {};
        std::array<DivisionCommand, kBattleLaneCount> divisionCommands = {};
        int ownSoldiersBefore = 0;
        int enemySoldiersBefore = 0;
        std::array<int, kBattleLaneCount> ownLaneSoldiersBefore = {};
        std::array<int, kBattleLaneCount> enemyLaneSoldiersBefore = {};
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

    /* 敵へ捕まったときに与える大きな負報酬で、接触回避を強く学ばせます。 */
    static constexpr float kCaughtPenalty = -10.0f;

    /* 手数上限で打ち切られたエピソードへ、停滞分の追加減点を入れます。 */
    static constexpr float kTimeoutPenalty = -1.5f;

    /* 敵が通常移動したときの小さな基本報酬で、長引く追跡を少しだけ抑えます。 */
    static constexpr float kEnemyStepReward = -0.02f;

    /* 敵が壁や盤外で足踏みしたときの減点です。 */
    static constexpr float kEnemyWallPenalty = -0.30f;

    /* 敵がプレイヤーを捕まえたときの大きな正報酬です。 */
    static constexpr float kEnemyCatchReward = 10.0f;

    /* プレイヤーにゴールへ逃げ切られたときの敵側ペナルティです。 */
    static constexpr float kEnemyGoalPenalty = -10.0f;

    /* プレイヤーが穴や時間切れで脱落したとき、敵にも阻止成功として与える報酬です。 */
    static constexpr float kEnemyBlockReward = 3.0f;

    /* 他の敵とは別の逃げ道を塞いだときに加点し、包囲を学ばせます。 */
    static constexpr float kEnemySealEscapeReward = 0.60f;

    /* プレイヤー隣接 4 方向を複数敵で押さえるほど加点し、囲い込みを促します。 */
    static constexpr float kEnemyAdjacencyReward = 0.35f;

    /* 敵が同じ側へ団子にならず、別方向へ散るほど少し加点して挟み込みを促します。 */
    static constexpr float kEnemySpreadReward = 0.18f;

    /* プレイヤー周囲 4 方向の被覆状態を表すビットマスクの総数です。 */
    static constexpr int kEnemySupportMaskCount = 16;

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

    /* 2 次元座標を 1 次元セル添字へ変換します。 */
    int CellIndex(const GridPoint& point) const;

    /* プレイヤー位置と敵位置の組を、共同状態の添字へ変換します。 */
    int JointStateIndex(const GridPoint& playerPoint, const GridPoint& enemyPoint) const;

    /* マスが盤面の外へはみ出していないかを判定します。 */
    bool IsInside(const GridPoint& point) const;

    /* 敵が踏み込める通常マスかどうかを判定します。 */
    bool IsEnemyWalkable(const GridPoint& point) const;

    /* 組み込みマップを再構築し、固定の開始位置と障害物を置き直します。 */
    void ResetMap();

    /* LDtk の IntGrid とエンティティを統合し、内部マップを作り直します。 */
    void ApplyLoadedMap(
        const std::string& levelIdentifier,
        int gridWidth,
        int gridHeight,
        const std::vector<int>& intGridCsv,
        const std::vector<LdtkEntityData>& entities);

    /* Q テーブル、統計窓、現在エピソードをすべて初期状態へ戻します。 */
    void ResetLearningState();

    /* Goal から逆向きに最短距離を張り、進捗報酬の基準になる距離表を作ります。 */
    void RebuildGoalDistanceMap();

    /* Start から Goal への代表経路上から、敵の初期配置候補を組み立てます。 */
    std::vector<GridPoint> ChooseDefaultEnemyStarts() const;

    /* そのマスから Goal までの最短距離を返します。到達不能なら -1 です。 */
    int GoalDistance(const GridPoint& point) const;

    /* ある時点の敵一覧から、プレイヤーに最も近い代表脅威を 1 体選びます。 */
    GridPoint SelectPrimaryEnemy(
        const GridPoint& playerPoint,
        const std::vector<GridPoint>& enemies) const;

    /* 指定座標に他の敵が居るかを調べ、敵同士の重なりを防ぎます。 */
    bool ContainsEnemy(
        const std::vector<GridPoint>& enemies,
        const GridPoint& point) const;

    /* 敵専用の状態添字として、味方の逃げ道封鎖情報も含めた添字を返します。 */
    int EnemyStateIndex(
        const GridPoint& playerPoint,
        const GridPoint& enemyPoint,
        int supportMask) const;

    /* 他の敵がプレイヤー周囲 4 方向のどこを既に押さえているかをビットで返します。 */
    int BuildEnemySupportMask(
        const GridPoint& playerPoint,
        const std::vector<GridPoint>& enemies,
        std::size_t ignoredEnemyIndex) const;

    /* プレイヤーが今このターンに安全に逃げられる方向数を数えます。 */
    int CountPlayerEscapeRoutes(
        const GridPoint& playerPoint,
        const std::vector<GridPoint>& enemies) const;

    /* プレイヤー隣接 4 マスのうち、敵が実際に押さえている方向数を数えます。 */
    int CountAdjacentEnemyCoverage(
        const GridPoint& playerPoint,
        const std::vector<GridPoint>& enemies) const;

    /* 敵 1 体がプレイヤーのどちら側から圧力をかけているかを 0..3 で返します。 */
    int EnemyApproachDirection(
        const GridPoint& playerPoint,
        const GridPoint& enemyPoint) const;

    /* 敵群が何方向からプレイヤーへ圧力をかけているかを数えます。 */
    int CountEnemyApproachDirections(
        const GridPoint& playerPoint,
        const std::vector<GridPoint>& enemies) const;

    /* 現在のマップサイズに応じて、1 エピソードの手数上限を動的に決めます。 */
    int EpisodeStepLimit() const;

    /* マップ面積に応じて、探索率の減衰速度を少し遅くした係数を返します。 */
    float EpsilonDecayFactor() const;

    /* 経路未発見の間は探索を止めすぎないよう、状況に応じた探索率下限を返します。 */
    float MinimumExplorationRate() const;

    /* 状態添字 1 つぶんの 4 行動から、指定テーブルの最大 Q 値だけを返します。 */
    float MaxQ(const std::vector<float>& qTable, int stateIndex) const;

    /* 探索用に 4 方向から完全ランダムで 1 手を選びます。 */
    Action RandomAction();

    /* 指定テーブルを deterministic に読み、状態添字で最も高い行動を 1 つ返します。 */
    Action GetBestActionForState(
        const std::vector<float>& qTable,
        int stateIndex) const;

    /* 最大 Q 値の行動だけを候補に残し、同点ならランダムで 1 つ選びます。 */
    Action SelectGreedyAction(
        const std::vector<float>& qTable,
        int stateIndex);

    /* epsilon-greedy に従い、探索手か活用手かを切り替えて 1 手を選びます。 */
    Action SelectAction(
        const std::vector<float>& qTable,
        int stateIndex);

    /* 指定行動を 1 手ぶんだけ盤面上へ適用し、壁判定込みの次座標を返します。 */
    MoveResult SimulateMove(const GridPoint& point, Action action, bool avoidPit) const;

    /* 敵専用の移動判定として、壁や落とし穴に加えて他の敵との重なりも避けます。 */
    MoveResult SimulateEnemyMove(
        const GridPoint& point,
        Action action,
        const std::vector<GridPoint>& occupiedEnemies) const;

    /* プレイヤーが Goal へ近づいた度合いを、最短距離差から報酬へ変換します。 */
    float PlayerProgressReward(const GridPoint& from, const GridPoint& to) const;

    /* 1 手ぶんの行動選択、Q 更新、終了判定をまとめて進めます。 */
    void Step();

    /* エピソード終了時に履歴と統計を更新し、次エピソードの準備をします。 */
    void FinishEpisode(bool success);

    /* エージェントを開始位置へ戻し、手数と累積報酬をリセットします。 */
    void ResetEpisode();

    /* LDtk 識別子から部隊を作り、初期部隊一覧へ追加します。 */
    void AddInitialBattleUnit(
        UnitFaction faction,
        UnitClass unitClass,
        const GridPoint& position,
        int count);

    /* 現在の部隊一覧から、旧描画互換の agent_ / enemies_ を更新します。 */
    void SyncLegacyPositionsFromBattleUnits();

    /* 指定陣営で生存している部隊数を返します。 */
    int CountActiveBattleUnits(UnitFaction faction) const;

    /* 指定陣営で生存している総人数を返します。 */
    int CountBattleSoldiers(UnitFaction faction) const;

    /* 1 ターンぶんの部隊移動、攻撃、勝敗判定を進めます。 */
    void StepBattle();

    /* 指定兵科の移動可能マス数を返します。 */
    int UnitMoveRange(UnitClass unitClass) const;

    /* 指定兵科の攻撃射程を返します。 */
    int UnitAttackRange(UnitClass unitClass) const;

    /* 指定兵科の基礎攻撃力を返します。 */
    int UnitBaseDamage(UnitClass unitClass) const;

    /* 指定兵科の守備補正を返します。 */
    float UnitDefense(UnitClass unitClass) const;

    /* 兵科相性による攻撃倍率を返します。 */
    float UnitMatchupMultiplier(UnitClass attackerClass, UnitClass defenderClass) const;

    /* 盤面距離をマンハッタン距離で返します。 */
    int BattleDistance(const GridPoint& from, const GridPoint& to) const;

    /* 指定マスが部隊移動先として使えるかを返します。 */
    bool IsBattleWalkable(const GridPoint& point) const;

    /* 指定マスに生存部隊がいるかを返します。 */
    bool IsBattleOccupied(const GridPoint& point, int ignoredUnitIndex) const;

    /* 目標射程へ近づくための次の 1 マスを BFS で探します。 */
    GridPoint FindNextBattleStep(
        int unitIndex,
        const GridPoint& targetPoint,
        int attackRange) const;

    /* 部隊を目標へ向けて兵科の移動力ぶん進めます。 */
    void MoveBattleUnitToward(int unitIndex, const GridPoint& targetPoint);

    /* 弓兵が近接されたとき、可能なら距離を取ります。 */
    void MoveArcherAwayFromTarget(int unitIndex, const GridPoint& targetPoint);

    /* 攻撃可能なら人数ダメージを適用します。 */
    bool ResolveBattleAttack(int attackerIndex, int targetIndex);

    /* 攻撃線と被弾演出の残り時間を 1 手ぶん進めます。 */
    void AdvanceBattleEffects();

    /* 陣営を Q テーブル配列の添字へ変換します。 */
    int FactionIndex(UnitFaction faction) const;

    /* 座標から左翼・中央・右翼のどこにいるかを返します。 */
    BattleLane LaneForPoint(const GridPoint& point) const;

    /* 師団列挙を配列添字へ変換します。 */
    int LaneIndex(BattleLane lane) const;

    /* 初期兵数を基準に、現在兵数を 0..2 の段階へ圧縮します。 */
    int SoldierRatioBucket(int current, int maximum) const;

    /* 兵力差を 0..2 の優劣段階へ圧縮します。 */
    int AdvantageBucket(int ownSoldiers, int enemySoldiers) const;

    /* 指定師団にいる指定陣営の総兵数を返します。 */
    int CountBattleSoldiersInLane(UnitFaction faction, BattleLane lane) const;

    /* 指定陣営の初期総兵数を返します。 */
    int CountInitialBattleSoldiers(UnitFaction faction) const;

    /* 指定師団にいる指定陣営の初期総兵数を返します。 */
    int CountInitialBattleSoldiersInLane(UnitFaction faction, BattleLane lane) const;

    /* 敵弓兵が自軍へどれだけ圧をかけているかを 0..2 へ圧縮します。 */
    int EnemyArcherPressureBucket(UnitFaction faction) const;

    /* 中央師団の支配状況を 0..2 へ圧縮します。 */
    int CenterControlBucket(UnitFaction faction) const;

    /* 本部AI用の状態添字を作ります。 */
    int BuildHeadquartersState(UnitFaction faction) const;

    /* 師団長AI用の状態添字を作ります。 */
    int BuildDivisionState(
        UnitFaction faction,
        BattleLane lane,
        HeadquartersCommand headquartersCommand) const;

    /* 任意行動数の Q テーブルから最大 Q 値を返します。 */
    float MaxTacticalQ(
        const std::vector<float>& qTable,
        int stateIndex,
        int actionCount) const;

    /* 任意行動数の Q テーブルから epsilon-greedy で行動添字を選びます。 */
    int SelectTacticalAction(
        const std::vector<float>& qTable,
        int stateIndex,
        int actionCount);

    /* Q 学習の更新式を本部・師団長の両方で使える形にまとめます。 */
    void UpdateTacticalQ(
        std::vector<float>& qTable,
        int stateIndex,
        int actionIndex,
        float reward,
        int nextStateIndex,
        bool done,
        int actionCount);

    /* 本部AIと師団長AIの命令を選び、後で学習更新できる形へまとめます。 */
    TacticalDecision SelectTacticalDecision(UnitFaction faction);

    /* 1 ターンの結果を、本部AIと師団長AIの Q 値へ戻します。 */
    void UpdateTacticalDecision(
        UnitFaction faction,
        const TacticalDecision& decision,
        bool done,
        bool ownWin);

    /* 本部命令と師団長命令を踏まえ、部隊が狙う相手を選びます。 */
    int SelectBattleTargetIndex(
        int unitIndex,
        HeadquartersCommand headquartersCommand,
        DivisionCommand divisionCommand) const;

    /* 指定部隊を安全距離まで目標から遠ざけます。 */
    void MoveBattleUnitAwayFromTarget(
        int unitIndex,
        const GridPoint& targetPoint,
        int safeDistance);

    /* 騎馬が弓兵を狙うとき側面から接近します。 */
    void MoveCavalryFlank(
        int unitIndex,
        const GridPoint& targetPoint);

    /* 歩兵が近くの弓兵を保護するように動きます。 */
    void MoveInfantryProtect(
        int unitIndex,
        const GridPoint& targetPoint,
        const std::vector<int>& archerIndices);

    //========================================
    // マップと Q テーブル
    //========================================

    /* 現在マップの横マス数です。LDtk 読み込み時に可変で変わります。 */
    int gridWidth_ = kBuiltInGridWidth;

    /* 現在マップの縦マス数です。LDtk 読み込み時に可変で変わります。 */
    int gridHeight_ = kBuiltInGridHeight;

    /* 現在マップの各マス種別を 1 次元配列で保持します。 */
    std::vector<Tile> tiles_;

    /* プレイヤー位置 × 敵位置 × 行動の Q 値本体です。 */
    std::vector<float> playerQ_;

    /* 敵位置 × プレイヤー位置 × 味方被覆マスク × 行動の Q 値本体です。 */
    std::vector<float> enemyQ_;

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

    /* 毎エピソードの敵開始位置列です。マップから読むか、自動配置で決めます。 */
    std::vector<GridPoint> enemyStarts_;

    /* 目標マスです。距離報酬や終了判定の基準として使います。 */
    GridPoint goal_ = {};

    /* 現在エピソードでエージェントが立っている座標です。 */
    GridPoint agent_ = {};

    /* 現在エピソードで敵が立っている座標列です。 */
    std::vector<GridPoint> enemies_;

    /* エピソード開始時に復元する部隊配置です。 */
    std::vector<BattleUnit> initialBattleUnits_;

    /* 現在の合戦で動いている部隊配置です。 */
    std::vector<BattleUnit> battleUnits_;

    /* 直近の攻撃線です。 */
    std::vector<BattleAttackTrace> battleAttackTraces_;

    /* 部隊 ID を重複させないための採番値です。 */
    int nextBattleUnitId_ = 1;

    /* 本部AIの Q テーブルです。陣営ごとに同じ状態設計で持ちます。 */
    std::array<std::vector<float>, 2> headquartersQ_;

    /* 師団長AIの Q テーブルです。左翼・中央・右翼の局所命令を学習します。 */
    std::array<std::vector<float>, 2> divisionQ_;

    /* 現在ターンで本部AIが出している命令です。 */
    std::array<HeadquartersCommand, 2> currentHeadquartersCommands_ = {
        HeadquartersCommand::Balanced,
        HeadquartersCommand::Balanced,
    };

    /* 現在ターンで師団長AIが出している命令です。 */
    std::array<std::array<DivisionCommand, kBattleLaneCount>, 2> currentDivisionCommands_ = {};

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
