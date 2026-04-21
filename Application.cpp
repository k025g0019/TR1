#include "Application.h"

#include "CsvExporter.h"
#include "SceneBuilder.h"
#include "SharedTypes.h"

#include <stdexcept>
#include <string>
#include <vector>
#include <windowsx.h>

//========================================
// Application 実装
//========================================
// メインループ、入力処理、学習進行の制御をここでまとめて扱います。
// Win32 のイベントと QLearningGrid の状態更新を橋渡しする中心ファイルです。

namespace {

//========================================
// ローカル補助関数
//========================================

/* 例外送出 */
// Win32 初期化失敗などを統一した形で投げ直します。
[[noreturn]] void ThrowWithMessage(const std::string& message) {
    throw std::runtime_error(message);
}

/* 速度プリセット一覧 */
// キーボード 1〜4 に対応する学習速度を固定表で持ちます。
constexpr Application::SpeedPreset kSpeedPresets[] = {
    {L"Slow", 260, 1},
    {L"Normal", 120, 1},
    {L"Fast", 45, 1},
    {L"Max", 0, 100},
};

/* HWND から Application を取り出す */
// WindowProc で this を参照するための小さな取り出し関数です。
Application* GetApplication(HWND hwnd) {
    return reinterpret_cast<Application*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
}

}  // namespace

//========================================
// ウィンドウ手続き
//========================================

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    //========================================
    // this ポインタ紐付け
    //========================================

    /* 生成時の関連付け */
    // WM_NCCREATE の段階で Application* を GWLP_USERDATA へ保存します。
    if (message == WM_NCCREATE) {
        const auto* createStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);
        auto* app = static_cast<Application*>(createStruct->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
        return TRUE;
    }

    /* 保存済みインスタンス取得 */
    Application* app = GetApplication(hwnd);

    //========================================
    // メッセージ分岐
    //========================================

    switch (message) {
    case WM_KEYDOWN:
        /* 仮想キー入力 */
        if (app != nullptr) {
            app->HandleKeyDown(wParam);
            return 0;
        }
        break;

    case WM_CHAR:
        /* 文字入力 */
        if (app != nullptr) {
            app->HandleCharInput(static_cast<wchar_t>(wParam));
            return 0;
        }
        break;

    case WM_LBUTTONDOWN:
        /* マウス左クリック */
        if (app != nullptr) {
            app->HandleLeftButtonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;
        }
        break;

    case WM_CLOSE:
        /* 閉じる要求 */
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        /* 終了通知 */
        PostQuitMessage(0);
        return 0;

    default:
        break;
    }

    /* ここで扱っていないメッセージだけは、Win32 標準の処理へそのまま流します。 */
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

//========================================
// メインループ
//========================================

void Application::Run() {
    //========================================
    // 起動初期化
    //========================================

    /* ウィンドウ作成 */
    CreateMainWindow();

    /* レンダラー初期化 */
    renderer_.Initialize(hwnd_, kWindowWidth, kWindowHeight);

    /* ウィンドウ表示 */
    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);

    /* 学習タイマー初期化 */
    ResetStepTimer();

    //========================================
    // メイン反復
    //========================================

    MSG msg = {};
    while (msg.message != WM_QUIT) {
        //========================================
        // OS メッセージ処理
        //========================================

        /* OS から来たイベントがあれば先に処理し、入力や終了要求を即反映します。 */
        if (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            continue;
        }

        //========================================
        // 学習進行
        //========================================

        /* 1 フレーム分だけ学習を進める */
        AdvanceTraining();

        //========================================
        // 表示用状態構築
        //========================================

        /* 散らばった UI 状態を 1 つへまとめ、描画関数へそのまま渡せる形にします。 */
        const EpisodeRunUiState episodeRunUiState{
            episodeTargetInput_,
            editingEpisodeTarget_,
            autoRunningToTargetEpisode_,
            targetEpisode_,
        };

        /* ワールド状態と UI 状態から、そのフレームに必要な図形頂点を全部組み立てます。 */
        const std::vector<Vertex> vertices = BuildSceneVertices(
            world_,
            kWindowWidth,
            kWindowHeight,
            kSpeedPresets[speedPresetIndex_].label,
            paused_,
            episodeRunUiState);

        //========================================
        // 描画
        //========================================

        /* GPU へ頂点を転送 */
        renderer_.UploadVertices(vertices);

        /* DirectX 描画 */
        renderer_.Render(static_cast<UINT>(vertices.size()));

        /* GDI テキスト重ね描き */
        DrawSceneOverlayText(
            hwnd_,
            world_,
            kWindowWidth,
            kWindowHeight,
            kSpeedPresets[speedPresetIndex_].label,
            paused_,
            episodeRunUiState);

        //========================================
        // タイトル更新
        //========================================

        /* タイトル更新は OS 呼び出しが多いので、少し間引いて負荷とちらつきを抑えます。 */
        if ((frameCounter_ % 8) == 0) {
            SetWindowTextW(
                hwnd_,
                BuildWindowTitle(
                    world_,
                    kSpeedPresets[speedPresetIndex_].label,
                    paused_,
                    episodeRunUiState)
                    .c_str());
        }

        /* フレーム番号更新 */
        ++frameCounter_;
    }
}

//========================================
// 目標エピソード入力
//========================================

void Application::BeginEpisodeTargetEdit() {
    /* クリック直後の 1 打目で既存値を書き換えられるよう、編集状態へ入ります。 */
    editingEpisodeTarget_ = true;

    /* 既定値の "50" を毎回手で消さなくて済むよう、次の数字で一度空にします。 */
    clearEpisodeTargetOnNextDigit_ = true;
}

void Application::ConfirmEpisodeTargetEdit() {
    //========================================
    // 空入力チェック
    //========================================

    /* 未入力は警告で弾く */
    if (episodeTargetInput_.empty()) {
        MessageBoxW(
            hwnd_,
            L"Please enter a target episode.",
            L"Target Episode",
            MB_OK | MB_ICONWARNING);
        return;
    }

    //========================================
    // 数値変換
    //========================================

    try {
        /* 文字列全体が正の整数になっているかを確認しながら数値化します。 */
        std::size_t processed = 0;
        const int parsed = std::stoi(episodeTargetInput_, &processed);
        if (processed != episodeTargetInput_.size() || parsed <= 0) {
            throw std::runtime_error("invalid target episode");
        }

        //========================================
        // 確定反映
        //========================================

        /* 数値として有効なら、以後の自動実行が参照する目標値へ確定します。 */
        targetEpisode_ = parsed;

        /* 編集状態を閉じる */
        editingEpisodeTarget_ = false;
        clearEpisodeTargetOnNextDigit_ = false;

        //========================================
        // 自動実行制御
        //========================================

        /* 目標が現在より先なら自動実行を開始し、既に到達済みならその場で一時停止します。 */
        if (targetEpisode_ > world_.GetEpisodeCount()) {
            autoRunningToTargetEpisode_ = true;
            paused_ = false;
            ResetStepTimer();
        } else {
            autoRunningToTargetEpisode_ = false;
            paused_ = true;
        }
    } catch (const std::exception&) {
        //========================================
        // 変換失敗通知
        //========================================

        /* 正の整数以外は受け付けない */
        MessageBoxW(
            hwnd_,
            L"Please enter a positive integer.",
            L"Target Episode",
            MB_OK | MB_ICONWARNING);
    }
}

void Application::CancelEpisodeTargetEdit() {
    /* 編集フラグ解除 */
    editingEpisodeTarget_ = false;
    clearEpisodeTargetOnNextDigit_ = false;

    /* 空欄のまま閉じると見た目が壊れるので、最後に確定していた目標値を戻します。 */
    if (episodeTargetInput_.empty()) {
        episodeTargetInput_ = std::to_wstring(std::max(1, targetEpisode_));
    }
}

void Application::StopEpisodeTargetRun() {
    /* 自動到達運転だけ止める */
    autoRunningToTargetEpisode_ = false;
}

//========================================
// キーボード入力
//========================================

void Application::HandleKeyDown(WPARAM key) {
    //========================================
    // 編集中専用操作
    //========================================

    if (editingEpisodeTarget_) {
        switch (key) {
        case VK_RETURN:
            /* Enter で確定 */
            ConfirmEpisodeTargetEdit();
            return;

        case VK_BACK:
            /* Backspace で 1 文字削除 */
            if (clearEpisodeTargetOnNextDigit_) {
                episodeTargetInput_.clear();
                clearEpisodeTargetOnNextDigit_ = false;
            }
            if (!episodeTargetInput_.empty()) {
                episodeTargetInput_.pop_back();
            }
            return;

        case VK_ESCAPE:
            /* Esc で編集中止 */
            CancelEpisodeTargetEdit();
            return;

        default:
            /* 文字入力は WM_CHAR 側で受けるので、ここでは特殊キーだけを処理します。 */
            return;
        }
    }

    //========================================
    // 通常操作
    //========================================

    switch (key) {
    case VK_ESCAPE:
        /* 編集中でなければ Esc はアプリ全体の終了ショートカットとして扱います。 */
        DestroyWindow(hwnd_);
        return;

    case '1':
        /* 低速再生へ切り替え、停止していたタイマー基準も現在時刻へ引き直します。 */
        StopEpisodeTargetRun();
        speedPresetIndex_ = 0;
        paused_ = false;
        ResetStepTimer();
        return;

    case '2':
        /* 標準速度へ戻し、ここから通常の刻み幅で学習を再開します。 */
        StopEpisodeTargetRun();
        speedPresetIndex_ = 1;
        paused_ = false;
        ResetStepTimer();
        return;

    case '3':
        /* 高速再生へ切り替え、短い間隔で Step を進めるようにします。 */
        StopEpisodeTargetRun();
        speedPresetIndex_ = 2;
        paused_ = false;
        ResetStepTimer();
        return;

    case '4':
        /* Max は待ち時間なしで複数 Step を回す専用モードです。 */
        StopEpisodeTargetRun();
        speedPresetIndex_ = 3;
        paused_ = false;
        ResetStepTimer();
        return;

    case VK_RETURN:
        /* Enter で直接入力欄へ入り、目標エピソード実行の準備を始めます。 */
        BeginEpisodeTargetEdit();
        return;

    case VK_SPACE:
        /* 自動再生を止め、Space で通常再生と一時停止をトグルします。 */
        StopEpisodeTargetRun();
        paused_ = !paused_;
        ResetStepTimer();
        return;

    case 'N':
        /* 一時停止中だけ 1 Step 進め、学習の様子を手動で追えるようにします。 */
        StopEpisodeTargetRun();
        if (paused_) {
            world_.Train(1);
        }
        return;

    case 'R':
        //========================================
        // LDtk 再読み込み
        //========================================

        StopEpisodeTargetRun();
        CancelEpisodeTargetEdit();
        try {
            world_.ReloadMapFromLdtk();
            ResetStepTimer();
        } catch (const std::exception& exception) {
            MessageBoxA(hwnd_, exception.what(), "LDtk Reload Error", MB_OK | MB_ICONERROR);
        }
        return;

    case 'E':
        //========================================
        // CSV 出力
        //========================================

        StopEpisodeTargetRun();
        try {
            const auto exportPath = ExportEpisodeHistoryCsv(world_, L"exports");
            const std::wstring message =
                L"CSV exported to:\n" + exportPath.wstring();
            MessageBoxW(
                hwnd_,
                message.c_str(),
                L"CSV Export",
                MB_OK | MB_ICONINFORMATION);
        } catch (const std::exception& exception) {
            MessageBoxA(hwnd_, exception.what(), "CSV Export Error", MB_OK | MB_ICONERROR);
        }
        return;

    default:
        return;
    }
}

void Application::HandleCharInput(wchar_t character) {
    //========================================
    // 入力対象チェック
    //========================================

    /* 他の UI 操作中に数字が混ざらないよう、入力欄編集中だけ文字入力を受けます。 */
    if (!editingEpisodeTarget_) {
        return;
    }

    /* ここでは整数だけを受けたいので、数字以外の WM_CHAR は捨てます。 */
    if (character < L'0' || character > L'9') {
        return;
    }

    //========================================
    // 入力反映
    //========================================

    /* 編集開始直後の 1 打目だけは既定文字列を置き換え、連続入力を自然にします。 */
    if (clearEpisodeTargetOnNextDigit_) {
        episodeTargetInput_.clear();
        clearEpisodeTargetOnNextDigit_ = false;
    }

    /* 異常に長い入力でレイアウトが崩れないよう、6 桁で打ち止めにします。 */
    if (episodeTargetInput_.size() >= 6) {
        return;
    }

    /* 文字追加 */
    episodeTargetInput_.push_back(character);
}

void Application::HandleLeftButtonDown(int x, int y) {
    //========================================
    // 入力欄ヒット判定
    //========================================

    /* まず UI 上の入力欄座標を計算し、クリック位置がそこに入っているか調べます。 */
    const RECT inputRect = GetEpisodeTargetInputRect(world_, kWindowWidth, kWindowHeight);
    const POINT point{x, y};

    /* 入力欄を押したら編集開始 */
    if (PtInRect(&inputRect, point)) {
        BeginEpisodeTargetEdit();
        return;
    }

    //========================================
    // 編集解除
    //========================================

    /* それ以外を押したら編集を閉じる */
    if (editingEpisodeTarget_) {
        CancelEpisodeTargetEdit();
    }
}

//========================================
// 学習進行制御
//========================================

void Application::ResetStepTimer() {
    /* ここを現在時刻に合わせることで、再開直後に連続更新が走るのを防ぎます。 */
    nextSimulationTime_ = std::chrono::steady_clock::now();
}

void Application::AdvanceTraining() {
    //========================================
    // 目標エピソード自動実行
    //========================================

    if (autoRunningToTargetEpisode_) {
        /* 目標到達まで回したい一方で、1 フレームで無制限に回すと描画が固まるので予算を切ります。 */
        int remainingBudget = kAutoRunStepsPerFrame;
        while (world_.GetEpisodeCount() < targetEpisode_ && remainingBudget > 0) {
            world_.Train(1);
            --remainingBudget;
        }

        /* 所定のエピソード数へ届いたら、自動実行を終了してその場で一時停止します。 */
        if (world_.GetEpisodeCount() >= targetEpisode_) {
            autoRunningToTargetEpisode_ = false;
            paused_ = true;
            ResetStepTimer();
        }
        return;
    }

    //========================================
    // 一時停止中
    //========================================

    if (paused_) {
        return;
    }

    //========================================
    // 通常速度更新
    //========================================

    const SpeedPreset& preset = kSpeedPresets[speedPresetIndex_];

    /* Max モードは待ち時間を使わず、その場でまとめて複数 Step 進めます。 */
    if (preset.intervalMs <= 0) {
        world_.Train(preset.stepsPerTick);
        return;
    }

    /* 通常速度では予定時刻に追従しながら進め、多少遅れても数回だけ追いつかせます。 */
    const auto now = std::chrono::steady_clock::now();
    int catchUpCount = 0;
    while (now >= nextSimulationTime_ && catchUpCount < 4) {
        world_.Train(preset.stepsPerTick);
        nextSimulationTime_ += std::chrono::milliseconds(preset.intervalMs);
        ++catchUpCount;
    }
}

//========================================
// ウィンドウ生成
//========================================

void Application::CreateMainWindow() {
    //========================================
    // クラス登録
    //========================================

    const HINSTANCE instance = GetModuleHandleW(nullptr);
    const wchar_t* className = L"DirectX12QLearningWindow";

    WNDCLASSEXW windowClass = {};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.lpszClassName = className;

    if (RegisterClassExW(&windowClass) == 0) {
        ThrowWithMessage("RegisterClassExW failed.");
    }

    //========================================
    // 表示サイズ補正
    //========================================

    RECT rect = {
        0,
        0,
        static_cast<LONG>(kWindowWidth),
        static_cast<LONG>(kWindowHeight),
    };
    AdjustWindowRect(&rect, kWindowStyle, FALSE);

    //========================================
    // 実ウィンドウ生成
    //========================================

    hwnd_ = CreateWindowExW(
        0,
        className,
        L"DirectX12 Q-Learning",
        kWindowStyle,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        nullptr,
        nullptr,
        instance,
        this);

    if (hwnd_ == nullptr) {
        ThrowWithMessage("CreateWindowExW failed.");
    }
}
