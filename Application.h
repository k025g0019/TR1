#pragma once

#define NOMINMAX
#include <windows.h>

#include <chrono>
#include <cstdint>
#include <string>

#include "Dx12Renderer.h"
#include "QLearningGrid.h"

//========================================
// Application クラス宣言
//========================================
// アプリ全体の実行ループ、入力処理、学習更新の進行管理をまとめるクラスです。
// Win32 のウィンドウ手続きから受けた操作を内部状態へ反映し、
// 描画用レンダラーと学習用ワールドを結び付ける役割を持ちます。

class Application {
public:
    //========================================
    // 速度プリセット定義
    //========================================
    // 画面上で選べる学習速度を、表示名と更新量の組で管理します。

    struct SpeedPreset {
        /* タイトルバーや UI に出す速度名です。 */
        const wchar_t* label;

        /* 通常再生で次の学習更新まで待つ時間です。0 以下なら即時連続実行です。 */
        int intervalMs;

        /* 1 回タイマーが進んだときにまとめて回す学習 Step 数です。 */
        int stepsPerTick;
    };

    //========================================
    // 公開インターフェース
    //========================================

    /* 実行開始 */
    // ウィンドウ生成からメインループ終了までを担当します。
    void Run();

private:
    //========================================
    // Win32 連携
    //========================================

    /* ウィンドウ手続きから private メンバーへ触るための許可 */
    friend LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    //========================================
    // 入力処理
    //========================================

    /* 仮想キー入力 */
    void HandleKeyDown(WPARAM key);

    /* 文字入力 */
    void HandleCharInput(wchar_t character);

    /* マウス左クリック */
    void HandleLeftButtonDown(int x, int y);

    /* 目標エピソード編集の開始 */
    void BeginEpisodeTargetEdit();

    /* 目標エピソード編集の確定 */
    void ConfirmEpisodeTargetEdit();

    /* 目標エピソード編集の中止 */
    void CancelEpisodeTargetEdit();

    /* 自動実行の停止 */
    void StopEpisodeTargetRun();

    //========================================
    // 実行制御
    //========================================

    /* 通常再生用タイマーの基準時刻を更新 */
    void ResetStepTimer();

    /* 1 フレーム分の学習更新を進める */
    void AdvanceTraining();

    /* メインウィンドウ生成 */
    void CreateMainWindow();

    //========================================
    // 固定設定
    //========================================

    /* リサイズを外し、学習 HUD が崩れない固定サイズウィンドウにします。 */
    static constexpr DWORD kWindowStyle =
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;

    /* 自動実行で 1 フレームに学習しすぎて描画が止まらないようにする上限です。 */
    static constexpr int kAutoRunStepsPerFrame = 2048;

    //========================================
    // 外部システム保持
    //========================================

    /* Win32 ウィンドウ本体 */
    HWND hwnd_ = nullptr;

    /* DirectX12 描画担当 */
    Dx12Renderer renderer_;

    /* 学習ワールド本体 */
    QLearningGrid world_;

    //========================================
    // 実行状態
    //========================================

    /* タイトル更新の間引きなどに使う単純なフレームカウンタです。 */
    std::uint64_t frameCounter_ = 0;

    /* kSpeedPresets のどれを現在採用しているかを表す添字です。 */
    int speedPresetIndex_ = 1;

    /* 通常再生を止めて、手動 1 ステップだけ受け付ける状態かどうかです。 */
    bool paused_ = false;

    /* 通常速度モードで、次に Step を進める予定時刻です。 */
    std::chrono::steady_clock::time_point nextSimulationTime_{};

    //========================================
    // 目標エピソード入力状態
    //========================================

    /* Enter やクリックで目標エピソード入力欄を編集中かどうかです。 */
    bool editingEpisodeTarget_ = false;

    /* 編集開始直後の最初の数字で、既存文字列を置き換えるためのフラグです。 */
    bool clearEpisodeTargetOnNextDigit_ = false;

    /* まだ整数へ確定していない途中入力も含め、そのまま UI に出す文字列です。 */
    std::wstring episodeTargetInput_ = L"50";

    /* 目標エピソード到達まで一時停止せず回す自動実行モードかどうかです。 */
    bool autoRunningToTargetEpisode_ = false;

    /* 自動実行が止まる基準になるエピソード番号です。 */
    int targetEpisode_ = 50;
};
