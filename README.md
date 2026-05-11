<<<<<<< HEAD
﻿# Q-Learning Demo README
This README is a source-oriented guide for understanding where each major process lives in the current codebase.
Important sections include actual code snippets taken from the current source files.
---
## Overview
This program is a grid-based demo where a player AI learns to reach the goal using Q-learning.
- Learning core: `QLearningGrid`
- App loop and input: `Application`
- Scene construction: `SceneBuilder`
- DirectX12 rendering: `Dx12Renderer`
- LDtk loading: `LdtkLoader`
- CSV export: `CsvExporter`
??????????????
`	ext
main()
  ?
Application::Run()
  ?
AdvanceTraining() ???????
  ?
BuildSceneVertices() ?????????
  ?
Dx12Renderer::Render() ?????
`
---
## Startup and Main Loop
<details>
<summary><strong>main.cpp: ????</strong></summary>
### ???
`cpp

//========================================
// エントリーポイント
//========================================
// コンソール説明を出したあと、Application を起動します。

int main() {
    try {
        //========================================
        // 起動時ガイド表示
        //========================================

        /* 操作ヘルプ */
        // 起動直後に最低限のキー操作が分かるよう、コンソールへ短いガイドを先に出します。
=======
# 強化学習デモ README

この README は、**コードを読む前に「どこで何をしているか」をつかむための説明**です。  
全部を同じ重さで並べるのではなく、**実際に重要な処理の流れ**が追えるように書いています。

---

## 全体像

このプログラムは、**グリッド上のプレイヤー AI が Q 学習でゴールを目指すデモ**です。

- 学習本体は `QLearningGrid`
- 実行ループと入力は `Application`
- 画面構築は `SceneBuilder`
- DirectX12 描画は `Dx12Renderer`
- LDtk 読み込みは `LdtkLoader`
- CSV 出力は `CsvExporter`

大きな流れは次のとおりです。

```text
main()
  ↓
Application::Run()
  ↓
AdvanceTraining() で学習を進める
  ↓
BuildSceneVertices() で画面用頂点を作る
  ↓
Dx12Renderer::Render() で描画する
```

---

## 起動と実行ループ

<details>
<summary><strong>main.cpp: 起動入口</strong></summary>

### コード

```cpp
int main() {
    try {
        // 起動直後に最低限の操作方法をコンソールへ出します。
>>>>>>> origin/main
        std::cout
            << "DirectX12 Q-Learning demo\n"
            << "1=slow 2=normal 3=fast 4=max "
            << "Space=pause N=step Enter=target episode "
            << "R=reload map E=export csv Esc=quit\n";

<<<<<<< HEAD
        //========================================
        // 本体実行
        //========================================

        /* ここから先は Win32 と DirectX12 の初期化、メインループ実行を Application へ委ねます。 */
=======
        // アプリ本体の初期化とメインループ実行は Application へ委ねます。
>>>>>>> origin/main
        Application app;
        app.Run();
        return 0;
    } catch (const std::exception& exception) {
<<<<<<< HEAD
        //========================================
        // 例外通知
        //========================================

        /* 先に stderr へ流しておくと、ダイアログを閉じたあとでも原因を追いやすくなります。 */
        std::cerr << exception.what() << '\n';

        /* GUI アプリとして起動した場合でも見落とさないよう、ダイアログでも同じ内容を出します。 */
        MessageBoxA(
            nullptr,
            exception.what(),
            "DirectX12 Q-Learning Error",
            MB_OK | MB_ICONERROR);
        return 1;
    }
}
`
</details>
<details>
<summary><strong>Application::Run(): ????????</strong></summary>
### ???
`cpp

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
=======
        // まずコンソールへ出しておくと、ダイアログを閉じたあとも原因を追えます。
        std::cerr << exception.what() << '\n';

        // GUI 実行時でも気づけるよう、同じ内容をダイアログでも通知します。
        MessageBoxA(nullptr, exception.what(), "DirectX12 Q-Learning Error", MB_OK | MB_ICONERROR);
        return 1;
    }
}
```

### ここでやっていること

- 最初に操作説明を出します。
- `Application` を作ります。
- 本体は `Run()` へ全部渡します。
- 例外が出たらコンソールとダイアログの両方へ出します。

### なぜこの形にしているか

- `main()` に学習ロジックを直接書かないことで、入口が非常に薄くなります。
- 実際の処理を `Application` へ集めることで、起動処理と本体処理の責務が分かれます。
- エラー表示をコンソールとダイアログの両方へ出すのは、授業環境や GUI 起動でも気づきやすくするためです。

</details>

<details>
<summary><strong>Application::Run(): 毎フレームの本体</strong></summary>

### コード

```cpp
void Application::Run() {
    // まず Win32 ウィンドウを作ります。
    CreateMainWindow();

    // 続いて DirectX12 側を初期化します。
    renderer_.Initialize(hwnd_, kWindowWidth, kWindowHeight);

    // ウィンドウを表示し、通常速度の基準時刻も初期化します。
    ShowWindow(hwnd_, SW_SHOWDEFAULT);
    UpdateWindow(hwnd_);
    ResetStepTimer();

    MSG message = {};
    while (message.message != WM_QUIT) {
        // OS から届いたイベントは先に全部処理します。
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }

        // 終了要求が来ていたらループを抜けます。
        if (message.message == WM_QUIT) {
            break;
        }

        // 1 フレームぶん学習を進めます。
        AdvanceTraining();

        // 描画へ渡しやすいように UI 状態を 1 つへまとめます。
        const EpisodeRunUiState episodeRunUiState = {
>>>>>>> origin/main
            episodeTargetInput_,
            editingEpisodeTarget_,
            autoRunningToTargetEpisode_,
            targetEpisode_,
        };

<<<<<<< HEAD
        /* ワールド状態と UI 状態から、そのフレームに必要な図形頂点を全部組み立てます。 */
        const std::vector<Vertex> vertices = BuildSceneVertices(
=======
        // その時点の世界状態を描画用頂点列へ変換します。
        std::vector<Vertex> vertices = BuildSceneVertices(
>>>>>>> origin/main
            world_,
            kWindowWidth,
            kWindowHeight,
            kSpeedPresets[speedPresetIndex_].label,
            paused_,
            episodeRunUiState);

<<<<<<< HEAD
        //========================================
        // 描画
        //========================================

        /* GPU へ頂点を転送 */
        renderer_.UploadVertices(vertices);

        /* DirectX 描画 */
        renderer_.Render(static_cast<UINT>(vertices.size()));

        /* GDI テキスト重ね描き */
=======
        // GPU へ頂点を渡して 1 フレーム描画します。
        renderer_.UploadVertices(vertices);
        renderer_.Render(static_cast<UINT>(vertices.size()));

        // 現在は空実装ですが、文字描画の差し込み口として残っています。
>>>>>>> origin/main
        DrawSceneOverlayText(
            hwnd_,
            world_,
            kWindowWidth,
            kWindowHeight,
            kSpeedPresets[speedPresetIndex_].label,
            paused_,
            episodeRunUiState);

<<<<<<< HEAD
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
`
</details>
<details>
<summary><strong>Application::AdvanceTraining(): ??????????????</strong></summary>
### ???
`cpp

void Application::AdvanceTraining() {
    //========================================
    // 目標エピソード自動実行
    //========================================

    if (autoRunningToTargetEpisode_) {
        /* 目標到達まで回したい一方で、1 フレームで無制限に回すと描画が固まるので予算を切ります。 */
        int remainingBudget = kAutoRunStepsPerFrame;
        while (world_.GetEpisodeCount() < targetEpisode_ && remainingBudget > 0) {
=======
        // タイトルバーにも今の学習状況を反映します。
        SetWindowTextW(
            hwnd_,
            BuildWindowTitle(
                world_,
                kSpeedPresets[speedPresetIndex_].label,
                paused_,
                episodeRunUiState).c_str());
    }
}
```

### 処理の流れ

- まずウィンドウと DirectX12 を初期化します。
- OS のメッセージを処理します。
- `AdvanceTraining()` で学習を進めます。
- `BuildSceneVertices()` で今の世界を頂点列へ変換します。
- `Render()` で描画します。
- テキストとタイトルバーも毎フレーム更新します。

つまり、  
**学習を進める → 今の状態を描く**  
を繰り返しているのが `Run()` です。

### この関数を読むときのポイント

- `Run()` 自体は「個別の処理を細かく実装する場所」ではなく、全体の順番を決める場所です。
- 学習本体は `AdvanceTraining()` や `QLearningGrid` 側へ分けてあり、`Run()` はそれらを毎フレームどう呼ぶかに集中しています。
- 描画前に必ず `AdvanceTraining()` を呼んでいるので、画面には「学習を 1 回以上進めたあとの状態」が出ます。
- 頂点描画と文字描画を分けているので、図形は DirectX12、文字は GDI という責務分担になっています。

### なぜこの順番が大事か

- 先に OS メッセージを処理しないと、キー入力や終了操作への反応が悪くなります。
- 先に学習してから描くことで、今見えている盤面は常に最新状態になります。
- 最後にタイトルまで更新しているので、画面内の表示とタイトルバーの統計がずれにくくなっています。

</details>

<details>
<summary><strong>Application::AdvanceTraining(): 学習をどれだけ進めるか決める</strong></summary>

### コード

```cpp
void Application::AdvanceTraining() {
    // 目標エピソードへ自動到達したい場合は、専用モードを優先します。
    if (autoRunningToTargetEpisode_) {
        int remainingBudget = kAutoRunStepsPerFrame;
        // 1 フレームで無制限に回すと描画が止まるので、予算ぶんだけ進めます。
        while (remainingBudget > 0 && world_.GetEpisodeCount() < targetEpisode_) {
>>>>>>> origin/main
            world_.Train(1);
            --remainingBudget;
        }

<<<<<<< HEAD
        /* 所定のエピソード数へ届いたら、自動実行を終了してその場で一時停止します。 */
        if (world_.GetEpisodeCount() >= targetEpisode_) {
            autoRunningToTargetEpisode_ = false;
            paused_ = true;
            ResetStepTimer();
=======
        // 目標へ届いたら自動実行を止め、その場で一時停止します。
        if (world_.GetEpisodeCount() >= targetEpisode_) {
            autoRunningToTargetEpisode_ = false;
            paused_ = true;
>>>>>>> origin/main
        }
        return;
    }

<<<<<<< HEAD
    //========================================
    // 一時停止中
    //========================================

=======
>>>>>>> origin/main
    if (paused_) {
        return;
    }

<<<<<<< HEAD
    //========================================
    // 通常速度更新
    //========================================

    const SpeedPreset& preset = kSpeedPresets[speedPresetIndex_];

    /* Max モードは待ち時間を使わず、その場でまとめて複数 Step 進めます。 */
=======
    // 現在選んでいる速度プリセットを取得します。
    const SpeedPreset& preset = kSpeedPresets[speedPresetIndex_];

    // interval 0 以下は Max 速度で、待たずにまとめて進めます。
>>>>>>> origin/main
    if (preset.intervalMs <= 0) {
        world_.Train(preset.stepsPerTick);
        return;
    }

<<<<<<< HEAD
    /* 通常速度では予定時刻に追従しながら進め、多少遅れても数回だけ追いつかせます。 */
    const auto now = std::chrono::steady_clock::now();
    int catchUpCount = 0;
    while (now >= nextSimulationTime_ && catchUpCount < 4) {
        world_.Train(preset.stepsPerTick);
        nextSimulationTime_ += std::chrono::milliseconds(preset.intervalMs);
        ++catchUpCount;
    }
}
`
</details>
<details>
<summary><strong>Application ?????</strong></summary>
### BeginEpisodeTargetEdit()
`cpp

//========================================
// 目標エピソード入力
//========================================

void Application::BeginEpisodeTargetEdit() {
    /* クリック直後の 1 打目で既存値を書き換えられるよう、編集状態へ入ります。 */
    editingEpisodeTarget_ = true;

    /* 既定値の "50" を毎回手で消さなくて済むよう、次の数字で一度空にします。 */
    clearEpisodeTargetOnNextDigit_ = true;
}
`
### ConfirmEpisodeTargetEdit()
`cpp

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
`
### CancelEpisodeTargetEdit()
`cpp

void Application::CancelEpisodeTargetEdit() {
    /* 編集フラグ解除 */
    editingEpisodeTarget_ = false;
    clearEpisodeTargetOnNextDigit_ = false;

    /* 空欄のまま閉じると見た目が壊れるので、最後に確定していた目標値を戻します。 */
    if (episodeTargetInput_.empty()) {
        episodeTargetInput_ = std::to_wstring(std::max(1, targetEpisode_));
    }
}
`
### StopEpisodeTargetRun()
`cpp

void Application::StopEpisodeTargetRun() {
    /* 自動到達運転だけ止める */
    autoRunningToTargetEpisode_ = false;
}
`
### HandleKeyDown()
`cpp

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
`
### HandleCharInput()
`cpp

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
`
### HandleLeftButtonDown()
`cpp

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
`
</details>
<details>
<summary><strong>Application::CreateMainWindow(): ???????</strong></summary>
### ???
`cpp

//========================================
// ウィンドウ生成
//========================================

void Application::CreateMainWindow() {
    //========================================
    // クラス登録
    //========================================

    const HINSTANCE instance = GetModuleHandleW(nullptr);
    const wchar_t* className = L"DirectX12QLearningWindow";

=======
    // 通常速度では、予定時刻に追いつくまで必要回数だけ進めます。
    const auto now = std::chrono::steady_clock::now();
    while (now >= nextSimulationTime_) {
        world_.Train(preset.stepsPerTick);
        nextSimulationTime_ += std::chrono::milliseconds(preset.intervalMs);
    }
}
```

### ここでやっていること

- 目標エピソードまで自動実行中なら、そこへ着くまで `Train(1)` を回します。
- 一時停止中なら何もしません。
- `Max` 速度なら待ち時間なしでまとめて学習します。
- 通常速度ならタイマーに応じて学習します。

この関数は、  
**学習するか / しないか / どれだけ学習するか**  
を決める場所です。

### 分岐ごとの意味

- `autoRunningToTargetEpisode_`
  - 指定エピソード数まで自動で進めたい特別モードです。
  - この分岐が最優先なのは、通常の速度設定より「目標まで進める」という要求を優先するためです。
- `paused_`
  - 一時停止中は描画だけ続けて、学習だけ止めます。
  - これにより、止めた瞬間の状態を観察できます。
- `preset.intervalMs <= 0`
  - `Max` 速度専用です。
  - 待ち時間を完全になくして、1 フレーム内で学習だけを優先します。
- `while (now >= nextSimulationTime_)`
  - 通常速度では「何フレーム経過したか」ではなく「予定時刻に達したか」で進めます。
  - そのため、PC の描画負荷が少し上下しても速度感がぶれにくくなります。

### なぜ `Train(1)` と `Train(preset.stepsPerTick)` を使い分けるか

- 自動実行中は `Train(1)` を細かく回すことで、目標エピソードに達した瞬間で止めやすくなります。
- 速度プリセット側は、1 回の tick で何 step 進めるかを切り替えたいので `stepsPerTick` を使っています。

</details>

<details>
<summary><strong>Application の入力処理</strong></summary>

### 主な関数

- `HandleKeyDown()`
  - 速度切り替え、一時停止、1 step、再読み込み、CSV 出力を受けます。
- `HandleCharInput()`
  - 目標エピソード入力欄への数字入力を受けます。
- `HandleLeftButtonDown()`
  - 入力欄クリックを受けます。
- `BeginEpisodeTargetEdit()`
  - 編集開始です。
- `ConfirmEpisodeTargetEdit()`
  - 編集確定です。
- `CancelEpisodeTargetEdit()`
  - 編集中止です。
- `StopEpisodeTargetRun()`
  - 自動実行だけ止めます。

### 役割

ここは学習そのものではなく、  
**ユーザーが学習速度や停止状態をどう操作するか**  
を受け持っています。

### それぞれの関数が分かれている理由

- キー入力
  - 再生、一時停止、速度変更のような「即時操作」を受けます。
- 文字入力
  - 数字欄への入力のような「編集操作」を受けます。
- マウス入力
  - 入力欄へフォーカスを与える役目を持ちます。

これを分けているので、

- `WM_KEYDOWN` は操作キー
- `WM_CHAR` は文字
- `WM_LBUTTONDOWN` はクリック

という Win32 側のイベント意味を、そのままコードの責務へ対応させられます。

</details>

<details>
<summary><strong>Application::CreateMainWindow(): ウィンドウ生成</strong></summary>

### コード

```cpp
void Application::CreateMainWindow() {
    // 実行中モジュールのインスタンスと、登録に使うクラス名を決めます。
    const HINSTANCE instance = GetModuleHandleW(nullptr);
    const wchar_t* className = L"DirectX12QLearningWindow";

    // このウィンドウの性格を表す Win32 設定を組み立てます。
>>>>>>> origin/main
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

<<<<<<< HEAD
    //========================================
    // 表示サイズ補正
    //========================================

=======
    // クライアント領域が固定サイズになるよう、外枠ぶんを補正します。
>>>>>>> origin/main
    RECT rect = {
        0,
        0,
        static_cast<LONG>(kWindowWidth),
        static_cast<LONG>(kWindowHeight),
    };
    AdjustWindowRect(&rect, kWindowStyle, FALSE);

<<<<<<< HEAD
    //========================================
    // 実ウィンドウ生成
    //========================================

=======
    // 実際のウィンドウ本体をここで生成します。
>>>>>>> origin/main
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
<<<<<<< HEAD

    if (hwnd_ == nullptr) {
        ThrowWithMessage("CreateWindowExW failed.");
    }
}
`
</details>
<details>
<summary><strong>Application::ResetStepTimer(): ???????????????</strong></summary>
### ???
`cpp

//========================================
// 学習進行制御
//========================================

void Application::ResetStepTimer() {
    /* ここを現在時刻に合わせることで、再開直後に連続更新が走るのを防ぎます。 */
    nextSimulationTime_ = std::chrono::steady_clock::now();
}
`
</details>
---
## Learning Core
<details>
<summary><strong>QLearningGrid::Train(): Step ?????????</strong></summary>
### ???
`cpp

//========================================
// 学習更新
//========================================

void QLearningGrid::Train(int steps) {
    /* 指定回数だけ内部 Step を回す */
    for (int index = 0; index < steps; ++index) {
        Step();
    }
}
`
</details>
<details>
<summary><strong>QLearningGrid::Step(): 1 ????????</strong></summary>
### ???
`cpp

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
=======
}
```

### ここでやっていること

- `WNDCLASSEXW` を組みます。
- `RegisterClassExW()` でクラス登録します。
- `AdjustWindowRect()` でクライアント領域が固定サイズになるよう補正します。
- `CreateWindowExW()` で実際のウィンドウを作ります。

この関数は、  
**学習や描画を始める前の土台になる Win32 ウィンドウを作る場所**です。

### 見るポイント

- `WNDCLASSEXW`
  - ウィンドウの性格を定義する設定です。
- `lpfnWndProc`
  - 実際の入力や終了イベントを受ける関数を結びつけています。
- `AdjustWindowRect()`
  - 「外枠込み」ではなく「中身の描画領域」を指定サイズにそろえるために使っています。
- `CreateWindowExW()`
  - ここで初めて実体のあるウィンドウができます。

この関数は地味ですが、ここが崩れると学習以前に画面が成立しません。

</details>

<details>
<summary><strong>Application::ResetStepTimer(): 通常速度モードの基準時刻を戻す</strong></summary>

### コード

```cpp
void Application::ResetStepTimer() {
    // 再開直後に古い時刻差で連続更新しないよう、基準時刻を現在へ戻します。
    nextSimulationTime_ = std::chrono::steady_clock::now();
}
```

- `nextSimulationTime_` を現在時刻へ戻します。
- 速度切り替えや一時停止解除の直後に、古い待ち時間が残らないようにします。

つまりこれは、  
**通常速度モードを気持ちよく再開するための時刻リセット**です。

</details>

---

## 学習の中心

ここがこのプログラムでいちばん大事な部分です。  
Q 学習そのものは `QLearningGrid` にまとまっています。

<details>
<summary><strong>QLearningGrid::Train(): Step を指定回数回す入口</strong></summary>

### コード

```cpp
void QLearningGrid::Train(int steps) {
    // 学習本体は Step() なので、指定回数ぶんそれを回すだけです。
    for (int i = 0; i < steps; ++i) {
        Step();
    }
}
```

### ここでやっていること

- 学習本体は `Step()` です。
- `Train()` は「何手ぶん進めるか」を受けて `Step()` を回すだけです。

</details>

<details>
<summary><strong>QLearningGrid::Step(): 1 手ごとの学習本体</strong></summary>

### コード

```cpp
void QLearningGrid::Step() {
    // 今いる位置を状態として取り出します。
    const GridPoint statePoint = agent_;
    const int state = StateIndex(statePoint);

    // その状態で 1 手選び、進めた結果を仮想計算します。
    const Action action = SelectAction(statePoint);
    const StepResult simulated = Simulate(statePoint, action);

    // 今回の 1 手をエピソード手数と総手数へ反映します。
    ++episodeSteps_;
    ++totalStepCount_;

    float reward = simulated.reward;
    bool done = simulated.done;
    // 長くさまよいすぎたら、タイムアウト扱いで打ち切ります。
>>>>>>> origin/main
    if (!done && episodeSteps_ >= EpisodeStepLimit()) {
        reward += kTimeoutPenalty;
        done = true;
    }

<<<<<<< HEAD
    //========================================
    // Q 学習更新式
    //========================================

    /* 現在の Q 値を、観測した報酬 + 次状態の最大価値へ少しずつ寄せて更新します。 */
=======
    // 次状態の最大価値を見込みとして使い、Q 値を少しだけ更新します。
>>>>>>> origin/main
    const int actionIndex = static_cast<int>(action);
    const float future = done ? 0.0f : MaxQ(simulated.next);
    float& current = q_[state * kActionCount + actionIndex];
    current += kAlpha * (reward + kGamma * future - current);

<<<<<<< HEAD
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
=======
    // 仮想計算した結果を、実際の現在位置と累積報酬へ反映します。
    agent_ = simulated.next;
    episodeReward_ += reward;

    // 終端なら、成功か失敗かを判定してエピソードを締めます。
    if (done) {
>>>>>>> origin/main
        const bool success =
            GetTile(simulated.next.x, simulated.next.y) == Tile::Goal;
        FinishEpisode(success);
    }
}
<<<<<<< HEAD
`
</details>
<details>
<summary><strong>QLearningGrid::SelectAction(): ?????????????</strong></summary>
### ???
`cpp

Action QLearningGrid::SelectAction(const GridPoint& point) {
    /* epsilon 未満なら探索、それ以外なら現在の最良手を採用する epsilon-greedy です。 */
    if (randomUnit_(rng_) < epsilon_) {
        return RandomAction();
    }
    return SelectGreedyAction(point);
}
`
</details>
<details>
<summary><strong>QLearningGrid::Simulate(): ????????????????</strong></summary>
### ???
`cpp

QLearningGrid::StepResult QLearningGrid::Simulate(
    const GridPoint& point,
    Action action) const {
    GridPoint next = point;

    //========================================
    // 次座標計算
    //========================================

=======
```

### 処理の流れ

- 今の位置を状態として取り出します。
- `SelectAction()` で次の行動を選びます。
- `Simulate()` でその行動の結果を仮想計算します。
- 手数を増やします。
- 時間切れなら終了させます。
- 更新式で Q 値を直します。
- 実際の位置と累積報酬を更新します。
- 終了したら `FinishEpisode()` へ進みます。

### 行ごとの見方

```cpp
const float future = done ? 0.0f : MaxQ(simulated.next);
```

- もう終了しているなら、その先の価値は存在しないので `0.0f` を使います。
- まだ続くなら、次の状態 `simulated.next` で取りうる最大 Q 値を `future` に入れます。
- ここで「将来の見込み」を数値化しています。

```cpp
float& current = q_[state * kActionCount + actionIndex];
```

- `current` は値のコピーではなく、Q テーブル上のその場所そのものへの参照です。
- つまり、このあと `current += ...` をすると、ローカル変数ではなく **Q テーブル本体** が直接更新されます。

```cpp
agent_ = simulated.next;
episodeReward_ += reward;
```

- 学習用の更新式だけで終わらず、実際のエージェント位置も進めます。
- 同時に、そのエピソードでここまでに得た累積報酬へ今回の `reward` を足します。
- これで「今どこにいて、今までどれだけ良い / 悪い結果だったか」が次の Step へ引き継がれます。

```cpp
const bool success =
    GetTile(simulated.next.x, simulated.next.y) == Tile::Goal;
FinishEpisode(success);
```

- `done` になっただけでは、「成功終了」か「失敗終了」かはまだ分かりません。
- そこで終了先のマス `simulated.next` を見て、そのマスが `Tile::Goal` なら `success = true`、それ以外なら `false` にします。
- つまりこの 2 行は、  
  **終わった理由を Goal 到達かどうかで判定してから、終了集計関数へ渡している**  
  という意味です。
- `FinishEpisode(success)` 側では、この `success` を使って成功回数や成功率の記録を更新します。

### 更新式

```cpp
current += kAlpha * (reward + kGamma * future - current);
```

### この式の意味

- `current`
  - 今の Q 値です。
- `reward`
  - 今回の行動で得た報酬です。
- `future`
  - 次状態で期待できる最大価値です。
- `kAlpha`
  - 学習率です。どれだけ強く今回の結果を反映するかを決めます。
- `kGamma`
  - 将来価値の重みです。先の見込みをどれだけ重視するかを決めます。

### 式をそのまま読むと

- `reward + kGamma * future`
  - 今回の結果と将来の見込みを合わせた目標値です。
- `reward + kGamma * future - current`
  - 今の予測とのずれです。
- `kAlpha * (...)`
  - そのずれを少しだけ反映する修正量です。

つまり、  
**今の予測**  
**今回の結果**  
**次の見込み**  
を使って Q 値を少しずつ直しています。

</details>

<details>
<summary><strong>QLearningGrid::SelectAction(): 探索するか活用するか決める</strong></summary>

### コード

```cpp
Action QLearningGrid::SelectAction(const GridPoint& point) {
    // epsilon 未満なら探索としてランダム行動を選びます。
    if (randomUnit_(rng_) < epsilon_) {
        return RandomAction();
    }

    // そうでなければ、今いちばん良いと分かっている行動を使います。
    return SelectGreedyAction(point);
}
```

### この式の意味

```cpp
randomUnit_(rng_) < epsilon_
```

- `randomUnit_(rng_)`
  - `0.0` 以上 `1.0` 未満の乱数です。
- `epsilon_`
  - ランダム行動率です。

真なら探索、偽なら活用です。  
ここが **epsilon-greedy** の分岐です。

### なぜこの 1 行が大事か

- ここで探索を入れないと、初期のたまたま高く見えた行動だけを繰り返してしまいます。
- 逆に、ここで常にランダムだと学習した価値を活かせません。
- そのためこの分岐は、**試して学ぶ** と **学んだ結果を使う** のバランスを取るための中心です。

</details>

<details>
<summary><strong>QLearningGrid::Simulate(): 行動したらどうなるか仮想計算する</strong></summary>

### コード

```cpp
QLearningGrid::StepResult QLearningGrid::Simulate(
    const GridPoint& point,
    Action action) const {
    // まずは現在位置を基準に、1 手進んだ先の座標を仮に作ります。
    GridPoint next = point;

>>>>>>> origin/main
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

<<<<<<< HEAD
    //========================================
    // 壁判定
    //========================================

    /* 盤外か壁なら移動は成立しないので、座標は据え置きのまま壁ペナルティを適用します。 */
=======
    // 盤外や壁なら移動失敗として、その場据え置き + 壁ペナルティです。
>>>>>>> origin/main
    if (!IsInside(next) || GetTile(next.x, next.y) == Tile::Wall) {
        return {point, kWallPenalty, false};
    }

<<<<<<< HEAD
    //========================================
    // 距離報酬と終端判定
    //========================================

=======
    // 到達先の地形と、Goal までの距離変化を調べます。
>>>>>>> origin/main
    const Tile tile = GetTile(next.x, next.y);
    const int previousDistance = GoalDistance(point);
    const int nextDistance = GoalDistance(next);
    float progressReward = 0.0f;

<<<<<<< HEAD
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
=======
    // Goal に近づいたぶんだけ、補助的な進捗報酬を付けます。
    if (previousDistance >= 0 && nextDistance >= 0) {
        progressReward =
            static_cast<float>(previousDistance - nextDistance) * 0.08f;
    }

    // Goal は成功終了、Pit は失敗終了、それ以外は通常継続です。
    if (tile == Tile::Goal) {
        return {next, kGoalReward + progressReward, true};
    }

>>>>>>> origin/main
    if (tile == Tile::Pit) {
        return {next, kPitPenalty + progressReward, true};
    }

    return {next, kStepReward + progressReward, false};
}
<<<<<<< HEAD
`
</details>
<details>
<summary><strong>QLearningGrid::FinishEpisode(): 1 ???????????</strong></summary>
### ???
`cpp

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
=======
```

### ここでやっていること

- 行動方向に応じて 1 マス進めます。
- 壁や盤外ならその場に止めて減点します。
- ゴールまでの距離差を見て、進捗報酬を作ります。
- Goal なら成功報酬で終了します。
- Pit なら失敗報酬で終了します。
- それ以外は通常移動報酬で続行します。

### 進捗報酬の式

```cpp
progressReward =
    static_cast<float>(previousDistance - nextDistance) * 0.08f;
```

### この式の意味

- `previousDistance`
  - 行動前のゴール距離です。
- `nextDistance`
  - 行動後のゴール距離です。
- `previousDistance - nextDistance`
  - 今回どれだけゴールへ近づいたかです。
- `0.08f`
  - その改善をどれだけ報酬へ変換するかの重みです。

つまり、  
**どのマスへ着いたか**だけでなく、  
**その 1 手がゴールへ近づく動きだったか**も評価しています。

</details>

<details>
<summary><strong>QLearningGrid::FinishEpisode(): 1 エピソード終了後の集計</strong></summary>

### コード

```cpp
void QLearningGrid::FinishEpisode(bool success) {
    // 今回のエピソードに属する値を、リセット前に退避します。
    const int stepsThisEpisode = episodeSteps_;
    const float epsilonUsedThisEpisode = epsilon_;

    // 完了回数と成功回数を更新します。
>>>>>>> origin/main
    ++episodeCount_;
    if (success) {
        ++successfulEpisodeCount_;
    }

<<<<<<< HEAD
=======
    // 報酬窓と成功窓を更新し、画面統計に使えるようにします。
>>>>>>> origin/main
    lastEpisodeReward_ = episodeReward_;
    rewardWindow_[rewardWindowCursor_] = lastEpisodeReward_;
    successWindow_[rewardWindowCursor_] = success ? 1.0f : 0.0f;
    rewardWindowCursor_ = (rewardWindowCursor_ + 1) % kRewardWindowSize;
    rewardWindowCount_ = std::min(rewardWindowCount_ + 1, kRewardWindowSize);

<<<<<<< HEAD
    /* 大きいマップでは減衰を緩め、経路未発見の間は探索率が落ち切らないよう下限も上げます。 */
    epsilon_ = std::max(MinimumExplorationRate(), epsilon_ * EpsilonDecayFactor());

    //========================================
    // 履歴レコード追加
    //========================================

    /* その時点の平均報酬や成功率をスナップショットとして履歴へ残します。 */
=======
    // 次エピソードに向けて探索率を少し下げますが、下限は割らせません。
    epsilon_ = std::max(MinimumExplorationRate(), epsilon_ * EpsilonDecayFactor());

    // CSV や画面表示に使う履歴 1 行をここで確定保存します。
>>>>>>> origin/main
    episodeHistory_.push_back({
        episodeCount_,
        epsilonUsedThisEpisode,
        GetAverageReward(),
        GetRecentSuccessRate(),
        MeasureGreedyPathLength(),
        stepsThisEpisode,
    });

<<<<<<< HEAD
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
`
</details>
<details>
<summary><strong>QLearningGrid::ResetLearningState(): ??????????</strong></summary>
### ???
`cpp

void QLearningGrid::ResetLearningState() {
    /* 以前の学習結果が新しいマップへ混ざらないよう、Q 値と履歴窓を丸ごと消します。 */
=======
    // 次の挑戦を Start から始められるよう、エピソード状態を戻します。
    ResetEpisode();
}
```

### ここでやっていること

- エピソード数と成功数を更新します。
- 今回報酬を保存します。
- 平均報酬や直近成功率に使う窓を更新します。
- `epsilon_` を次回用に下げます。
- `episodeHistory_` へ 1 行追加します。
- `ResetEpisode()` で次回開始状態へ戻します。

### この関数の位置づけ

- `Step()` が 1 手の更新なら、`FinishEpisode()` は 1 回の挑戦を締める関数です。
- 画面に出る成功率や平均報酬も、ここで集計されて次に反映されます。
- つまりここは、**学習の内部更新** と **人が観察する統計更新** がつながる場所です。

### 探索率更新式

```cpp
epsilon_ = std::max(MinimumExplorationRate(), epsilon_ * EpsilonDecayFactor());
```

### この式の意味

- `epsilon_ * EpsilonDecayFactor()`
  - 今の探索率を少しだけ下げます。
- `MinimumExplorationRate()`
  - 下げすぎ防止の下限です。
- `std::max(...)`
  - 下限より小さくならないようにします。

探索率を毎回下げるだけだと、  
早い段階で探索が止まり、まだ粗い経路へ固まりやすくなります。  
この下限があるので、学習後半でも少しだけ別の手を試せます。

</details>

<details>
<summary><strong>学習を支える補助関数</strong></summary>

### `EpisodeStepLimit()`

- 1 エピソードの手数上限を返します。
- マップが大きいほど上限も大きくします。

```cpp
const int scaledLimit = gridWidth_ * gridHeight_ + gridWidth_ + gridHeight_;
return std::max(kBaseMaxEpisodeSteps, scaledLimit);
```

### `EpsilonDecayFactor()`

- 探索率の減衰係数を返します。
- 大きいマップほど探索率が急激に下がらないようにします。

```cpp
return 1.0f - ((1.0f - kBaseEpsilonDecay) / areaScale);
```

### `MinimumExplorationRate()`

- 探索率の下限です。
- まだ一度も成功していない間は探索を厚めに残します。

```cpp
return std::min(0.20f, 0.08f + (areaScale - 1.0f) * 0.04f);
```

### `BuildGreedyPath()`

- 今の Q テーブルだけで最善経路をたどります。
- 画面上の黄色い経路表示の元です。
- 途中でループや停止が起きたら、その時点で今の方策にはまだ問題があると分かります。

### `MeasureGreedyPathLength()`

- greedy 経路の長さを返します。
- ゴールへ届かなければ `-1` です。
- だから `-1` は、単に長さ不明ではなく **最善経路がまだ完成していない状態** を表します。

</details>

<details>
<summary><strong>QLearningGrid::ResetLearningState(): 学習状態を初期化する</strong></summary>

### コード

```cpp
void QLearningGrid::ResetLearningState() {
    // Q テーブルそのものをゼロから作り直します。
>>>>>>> origin/main
    q_.assign(static_cast<std::size_t>(gridWidth_ * gridHeight_ * kActionCount), 0.0f);
    rewardWindow_.fill(0.0f);
    successWindow_.fill(0.0f);

<<<<<<< HEAD
    /* 画面表示や CSV で使う集計値も、最初から学習し直す前提で初期化します。 */
=======
    // エピソード数や累積報酬などの統計もすべて初期状態へ戻します。
>>>>>>> origin/main
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

<<<<<<< HEAD
    ResetEpisode();
}
`
</details>
<details>
<summary><strong>QLearningGrid::ResetEpisode(): ???????????????</strong></summary>
### ???
`cpp

void QLearningGrid::ResetEpisode() {
    /* 次の試行は必ず Start から始めるため、位置・手数・累積報酬を初期化します。 */
=======
    // 最後に、進行中エピソードの位置と手数も Start 状態へ戻します。
    ResetEpisode();
}
```

### ここでやっていること

- `q_` をすべて `0` で埋めます。
- 報酬窓と成功窓を空に戻します。
- エピソード数、成功数、総 Step 数、探索率、累積報酬を初期化します。
- `episodeHistory_` を消します。
- 最後に `ResetEpisode()` を呼びます。

この関数は、  
**新しいマップへ切り替えた直後や初回起動時に、学習の痕跡を全部消す場所**です。

</details>

<details>
<summary><strong>QLearningGrid::ResetEpisode(): 次のエピソードの開始位置へ戻す</strong></summary>

### コード

```cpp
void QLearningGrid::ResetEpisode() {
    // エージェント位置だけを Start へ戻し、この挑戦の手数と報酬をリセットします。
>>>>>>> origin/main
    agent_ = start_;
    episodeSteps_ = 0;
    episodeReward_ = 0.0f;
}
<<<<<<< HEAD
`
</details>
<details>
<summary><strong>QLearningGrid::BuildGreedyPath(): ??????????????</strong></summary>
### ???
`cpp

std::vector<GridPoint> QLearningGrid::BuildGreedyPath(int maxSteps) const {
=======
```

- `agent_` を `start_` へ戻します。
- `episodeSteps_` を `0` に戻します。
- `episodeReward_` を `0.0f` に戻します。

`FinishEpisode()` が統計保存を終えたあと、  
**次の挑戦を最初から始められる状態へ戻す**のがこの関数です。

ここで消しているのは、

- 現在位置
- 今回の手数
- 今回の累積報酬

だけです。  
Q テーブルや成功履歴は消していないので、**学習成果は残したまま次エピソードへ入る** 形になります。

</details>

<details>
<summary><strong>QLearningGrid::BuildGreedyPath(): 今学習した最善経路を再生する</strong></summary>

### コード

```cpp
std::vector<GridPoint> QLearningGrid::BuildGreedyPath(int maxSteps) const {
    // 最大手数が未指定なら、盤面サイズをもとに上限を決めます。
>>>>>>> origin/main
    if (maxSteps <= 0) {
        maxSteps = std::max(1, gridWidth_ * gridHeight_);
    }

<<<<<<< HEAD
    std::vector<GridPoint> path;
    path.reserve(static_cast<std::size_t>(maxSteps) + 1);

    /* Start から始めて、毎手「今いちばん良い行動」だけを仮想的に追いかけます。 */

    /* 同じマスへ戻り続ける無限ループを止めるため、訪問済みマスを記録します。 */
    std::vector<unsigned char> visited(static_cast<std::size_t>(gridWidth_ * gridHeight_), 0);
    GridPoint position = start_;
    path.push_back(position);

    for (int step = 0; step < maxSteps; ++step) {
        /* ゴールへ着いた時点で、この方策は最後までたどれたので終了です。 */
=======
    // 経路本体をためる配列を用意します。
    std::vector<GridPoint> path;
    path.reserve(static_cast<std::size_t>(maxSteps) + 1);

    // 同じ状態を何度も回るループを止めるため、訪問済みを記録します。
    std::vector<unsigned char> visited(static_cast<std::size_t>(gridWidth_ * gridHeight_), 0);
    GridPoint position = start_;
    // 経路の先頭は必ず Start です。
    path.push_back(position);

    // 今の greedy 方策を 1 手ずつ仮想的にたどっていきます。
    for (int step = 0; step < maxSteps; ++step) {
        // もうゴールへ着いていたら、それ以上たどる必要はありません。
>>>>>>> origin/main
        if (position.x == goal_.x && position.y == goal_.y) {
            break;
        }

<<<<<<< HEAD
        /* 以前と同じマスへ戻ったら、その後も同じ循環に入るだけなので打ち切ります。 */
        const int state = StateIndex(position);
=======
        const int state = StateIndex(position);
        // 同じ状態へ戻ってきたら循環していると考えて打ち切ります。
>>>>>>> origin/main
        if (visited[state]) {
            break;
        }
        visited[state] = 1;

<<<<<<< HEAD
        /* そのマスでの最善行動を 1 手だけ試し、次にどこへ進むかを見ます。 */
        const StepResult result =
            Simulate(position, GetBestAction(position.x, position.y));

        /* 壁などで座標が変わらないなら、方策が前へ進めていないので終了です。 */
=======
        // その地点で最善と学習している 1 手を仮想的に進めます。
        const StepResult result =
            Simulate(position, GetBestAction(position.x, position.y));

        // 座標が変わらないなら、壁や盤外にぶつかって前進できていません。
>>>>>>> origin/main
        if (result.next.x == position.x && result.next.y == position.y) {
            break;
        }

<<<<<<< HEAD
        path.push_back(result.next);

        /* 落とし穴で終わる経路も「ここで途切れる方策」として記録して止めます。 */
=======
        // 進めた先を経路へ追加します。
        path.push_back(result.next);

        // 落とし穴へ入ったら、その方策はそこで終わりです。
>>>>>>> origin/main
        if (GetTile(result.next.x, result.next.y) == Tile::Pit) {
            break;
        }

<<<<<<< HEAD
=======
        // 次の周回では、今進んだ先を現在位置として続きを見ます。
>>>>>>> origin/main
        position = result.next;
    }

    return path;
}
<<<<<<< HEAD
`
</details>
<details>
<summary><strong>QLearningGrid::GetBestAction(): ?????????????</strong></summary>
### ???
`cpp

Action QLearningGrid::GetBestAction(int x, int y) const {
=======
```

### ここでやっていること

- `start_` から開始します。
- 毎回 `GetBestAction()` で今もっとも良い行動を取ります。
- `Simulate()` でその結果を仮想計算します。
- ゴール到達、ループ、進行不能、落とし穴のどれかで止めます。

この関数は、  
**今の Q テーブルだけを使ったらどう進むか**  
を確認するための関数です。

### 条件分岐の意味

```cpp
if (position.x == goal_.x && position.y == goal_.y) {
    break;
}
```

- 今いるマスがもうゴールそのものなら、その時点で経路再生を終えます。
- これ以上進める必要がないからです。

```cpp
if (visited[state]) {
    break;
}
visited[state] = 1;
```

- 以前にも来た状態へまた戻ってきたら、その先も同じ巡回を繰り返すだけになる可能性が高いです。
- そのため、同じ状態を 2 回目に見つけた時点で打ち切ります。
- `visited[state] = 1` は、「この状態はもう通った」と記録している行です。

```cpp
const StepResult result =
    Simulate(position, GetBestAction(position.x, position.y));
```

- 現在位置 `position` で、いちばん良いと学習している行動を `GetBestAction()` で取ります。
- その行動を本当に進めるのではなく、`Simulate()` で「進んだらどうなるか」だけを計算します。
- つまりこの 1 行は、  
  **greedy 方策で 1 手先を仮想的にのぞく**  
  処理です。

```cpp
if (result.next.x == position.x && result.next.y == position.y) {
    break;
}
```

- この条件は、「最善行動を選んだのに座標が 1 マスも変わっていない」ことを見ています。
- こうなるのは、たとえば最善と判断している行動が壁や盤外へぶつかっていて、`Simulate()` がその場据え置きで返した場合です。
- そのまま続けても次の周回でも同じ行動、同じ結果になり、前へ進まない経路になります。
- だからこの `if` は、  
  **この方策ではここから先へ進めない**  
  と判定して打ち切るための条件です。

```cpp
path.push_back(result.next);
```

- 1 手進んだ結果の座標を経路列へ追加します。
- ここで追加しているのは「次に進んだ先」であって、「元の位置」ではありません。

```cpp
if (GetTile(result.next.x, result.next.y) == Tile::Pit) {
    break;
}
```

- 落とし穴へ入った時点で、その greedy 方策はそこで途切れるので終了します。
- ただし `path.push_back(result.next);` は先に行っているため、  
  **どこで落ちたか** までは経路として残ります。

</details>

<details>
<summary><strong>QLearningGrid::GetBestAction(): そのマスで最善の行動を返す</strong></summary>

### コード

```cpp
Action QLearningGrid::GetBestAction(int x, int y) const {
    // 指定マスを Q テーブルの状態番号へ変換します。
>>>>>>> origin/main
    const int state = StateIndex({x, y});
    float bestValue = std::numeric_limits<float>::lowest();
    Action bestAction = Action::Up;

<<<<<<< HEAD
    /* 4 方向を順番に見ていき、その時点で一番良い方向を保持し続けます。 */
    for (int actionIndex = 0; actionIndex < kActionCount; ++actionIndex) {
        const float qValue = q_[state * kActionCount + actionIndex];

        /* 今見ている方向の評価が暫定 1 位を上回ったら、その方向へ更新します。 */
=======
    // 4 方向を全部見て、もっとも値が高い行動を保持し続けます。
    for (int actionIndex = 0; actionIndex < kActionCount; ++actionIndex) {
        const float qValue = q_[state * kActionCount + actionIndex];
>>>>>>> origin/main
        if (qValue > bestValue) {
            bestValue = qValue;
            bestAction = static_cast<Action>(actionIndex);
        }
    }

    return bestAction;
}
<<<<<<< HEAD
`
</details>
<details>
<summary><strong>QLearningGrid::MeasureGreedyPathLength(): ??????????</strong></summary>
### ???
`cpp

//========================================
// greedy 経路確認
//========================================

int QLearningGrid::MeasureGreedyPathLength() const {
    const std::vector<GridPoint> path = BuildGreedyPath();

    /* greedy でたどった軌跡の最後がゴールなら、「移動回数 = 点数 - 1」として数えます。 */
=======
```

### 役割

- 4 方向の Q 値を見て、もっとも値が高い行動を返します。
- 画面に出る矢印や `BuildGreedyPath()` の進行方向はこの関数の結果です。

### 意味

この関数が返す方向は、  
**今の学習結果の中で、ここから進むならどっちがいちばん良さそうか**  
を表しています。

ただし、これは「真の正解方向」ではなく、  
**今の Q 値から見た暫定 1 位** です。  
学習初期ではまだ十分な意味を持たないこともあります。

</details>

<details>
<summary><strong>QLearningGrid::MeasureGreedyPathLength(): 最善経路の長さを測る</strong></summary>

### コード

```cpp
int QLearningGrid::MeasureGreedyPathLength() const {
    // まず現在の greedy 方策でたどれる経路を組み立てます。
    const std::vector<GridPoint> path = BuildGreedyPath();

    // 最後が Goal まで届いているときだけ、移動回数を返します。
>>>>>>> origin/main
    if (!path.empty() &&
        path.back().x == goal_.x &&
        path.back().y == goal_.y) {
        return static_cast<int>(path.size()) - 1;
    }
    return -1;
}
<<<<<<< HEAD
`
</details>
<details>
<summary><strong>QLearningGrid::GetAverageReward() / GetRecentSuccessRate(): ????????</strong></summary>
### ???
`cpp

float QLearningGrid::GetAverageReward() const {
    /* まだ履歴窓に値が入っていない初期状態では、平均の基になる値がありません。 */
=======
```

### ここでやっていること

- `BuildGreedyPath()` の結果を取ります。
- 最後がゴールなら、移動回数として `path.size() - 1` を返します。
- ゴールへ届いていなければ `-1` を返します。

つまりこの関数は、  
**今の方策が「通れるか」だけでなく「どれくらい短いか」まで 1 つの整数で見せる**  
ための関数です。

</details>

<details>
<summary><strong>QLearningGrid::GetAverageReward() / GetRecentSuccessRate(): 画面へ出す統計値</strong></summary>

### `GetAverageReward()`

```cpp
float QLearningGrid::GetAverageReward() const {
    // まだ履歴が 1 件もなければ平均は 0 扱いです。
>>>>>>> origin/main
    if (rewardWindowCount_ <= 0) {
        return 0.0f;
    }

<<<<<<< HEAD
    /* 直近に保存した報酬だけを合計し、件数で割って現在の平均報酬を作ります。 */
=======
    // 直近窓に入っている報酬だけを合計して平均化します。
>>>>>>> origin/main
    float total = 0.0f;
    for (int index = 0; index < rewardWindowCount_; ++index) {
        total += rewardWindow_[index];
    }
    return total / static_cast<float>(rewardWindowCount_);
}
<<<<<<< HEAD

float QLearningGrid::GetRecentSuccessRate() const {
    /* 成功 / 失敗の履歴がまだ 1 件もない間は、成功率も 0 扱いにします。 */
=======
```

- 直近報酬窓の合計を件数で割ります。
- 画面右パネルの `平均報酬` に使います。

### `GetRecentSuccessRate()`

```cpp
float QLearningGrid::GetRecentSuccessRate() const {
    // 履歴が無い間は成功率も 0 扱いです。
>>>>>>> origin/main
    if (rewardWindowCount_ <= 0) {
        return 0.0f;
    }

<<<<<<< HEAD
    /* 成功を 1、失敗を 0 として持っているので、その平均がそのまま成功率になります。 */
=======
    // 成功を 1、失敗を 0 として持っているので、その平均がそのまま成功率です。
>>>>>>> origin/main
    float total = 0.0f;
    for (int index = 0; index < rewardWindowCount_; ++index) {
        total += successWindow_[index];
    }
    return total / static_cast<float>(rewardWindowCount_);
}
<<<<<<< HEAD
`
</details>
<details>
<summary><strong>????????????</strong></summary>
### ???
`cpp
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

`
</details>
---
## Map Loading
<details>
<summary><strong>QLearningGrid::ResetMap(): ??????????</strong></summary>
### ???
`cpp

void QLearningGrid::ResetMap() {
    //========================================
    // 初期状態の組み込みマップ
    //========================================

=======
```

- 成功を `1.0f`
- 失敗を `0.0f`

として記録した窓の平均を返します。

つまり、  
**平均報酬** は「どれだけ良い結果を出しているか」、  
**直近成功率** は「どれだけ安定してゴールできているか」  
を見るための関数です。

この 2 つは似ているようで役割が違います。

- 平均報酬
  - 遠回りや落下、壁衝突が減っているかも含めて見ます。
- 直近成功率
  - 最終的にゴールへ着けたかだけを見ます。

そのため、  
平均報酬が改善しても成功率が低ければ、まだ慎重に動いているだけかもしれません。  
逆に成功率が上がっても平均報酬が悪ければ、かなり無駄な遠回りをしている可能性があります。

</details>

<details>
<summary><strong>報酬設定と学習パラメータ</strong></summary>

### 報酬

```cpp
static constexpr float kStepReward = -0.04f;
static constexpr float kWallPenalty = -0.80f;
static constexpr float kGoalReward = 10.0f;
static constexpr float kPitPenalty = -8.0f;
static constexpr float kTimeoutPenalty = -1.5f;
```

- `kStepReward`
  - 普通に 1 手進むだけでも少し減点し、無駄足を減らします。
- `kWallPenalty`
  - 壁や盤外へ当たった手を強めに減点します。
- `kGoalReward`
  - ゴールしたら大きく加点します。
- `kPitPenalty`
  - 落とし穴に入ったら大きく減点します。
- `kTimeoutPenalty`
  - 手数上限で終わったときの追加減点です。

### 学習パラメータ

```cpp
static constexpr float kAlpha = 0.16f;
static constexpr float kGamma = 0.95f;
static constexpr float kMinEpsilon = 0.00f;
static constexpr float kBaseEpsilonDecay = 0.998f;
```

- `kAlpha`
  - 学習率です。
- `kGamma`
  - 将来価値の重みです。
- `kMinEpsilon`
  - 成功後の探索率下限です。
- `kBaseEpsilonDecay`
  - 小さいマップ向けの基準減衰率です。

</details>

---

## マップ生成と読み込み

<details>
<summary><strong>QLearningGrid::QLearningGrid(): 初期状態と外部マップ読込の入口</strong></summary>

### コード

```cpp
QLearningGrid::QLearningGrid() : rng_(std::random_device{}()) {
    // 外部マップ読込に使う既定パスと既定レベル番号です。
    /* 既定の LDtk パス設定 */
    configuredLdtkPath_ = std::filesystem::path("maps") / "qlearning_demo.ldtk";
    configuredLevelIndex_ = 0;

    // まずは組み込みマップで確実に動ける状態を作ります。
    /* まず組み込みマップで初期化 */
    ResetMap();
    ResetLearningState();

    // 外部マップが存在するなら、その内容で上書きします。
    /* 外部マップがあれば上書きで読み込む */
    try {
        if (std::filesystem::exists(configuredLdtkPath_)) {
            LoadMapFromLdtk(configuredLdtkPath_, configuredLevelIndex_);
        }
    } catch (const std::exception& exception) {
        // 読み込み失敗時は、組み込みマップへ戻して起動自体は継続します。
        /* 外部マップが壊れていても起動自体は続けられるよう、既定マップへ切り替えます。 */
        ResetMap();
        ResetLearningState();
        usingLdtkMap_ = false;
        loadedLevelName_ = "BuiltIn";
        std::cout << "LDtk map load skipped: " << exception.what() << '\n';
    }
}
```

### ここでやっていること

- 乱数生成器 `rng_` を初期化します。
- 既定の LDtk パスとレベル番号を設定します。
- まずは必ず組み込みマップで安全に起動できる状態を作ります。
- そのあと外部 LDtk があれば読み込みます。
- 読み込み失敗時は、例外で停止せず組み込みマップへ戻します。

### この関数が大事な理由

- いきなり外部ファイル前提で起動しないので、授業環境や配布先でも起動失敗しにくいです。
- まず `ResetMap()` と `ResetLearningState()` を呼んで土台を作ってから外部マップを試すので、失敗時も最低限の実行状態が残ります。
- つまりこのコンストラクタは、**「とにかく起動できる状態を作る」ことを最優先にした入口**です。

</details>

<details>
<summary><strong>QLearningGrid::ReloadMapFromLdtk(): 同じ外部マップを読み直す</strong></summary>

### コード

```cpp
void QLearningGrid::ReloadMapFromLdtk() {
    // 前回の読み込み先が無ければ、同じ設定での再読み込みはできません。
    /* パス未設定は再読み込みできない */
    if (configuredLdtkPath_.empty()) {
        throw std::runtime_error("LDtk file path is not configured.");
    }

    // 前回と同じファイル・同じレベル番号でもう一度読み込みます。
    LoadMapFromLdtk(configuredLdtkPath_, configuredLevelIndex_);
}
```

### ここでやっていること

- まず再読み込み元のパスが設定済みか確認します。
- 問題なければ、前回と同じパス・同じレベル番号で `LoadMapFromLdtk()` を呼びます。

### 意味

これは新しい読み込みロジックを持つ関数ではなく、  
**「前回の外部マップをもう一度適用する」ための薄い窓口**です。  
`R` キー再読み込みのような操作から呼びやすいように分けてあります。

</details>

<details>
<summary><strong>QLearningGrid::ResetMap(): 組み込みマップを作る</strong></summary>

### コード

```cpp
void QLearningGrid::ResetMap() {
    // まず盤面サイズとマス配列を既定状態へ戻します。
>>>>>>> origin/main
    gridWidth_ = kBuiltInGridWidth;
    gridHeight_ = kBuiltInGridHeight;
    tiles_.assign(static_cast<std::size_t>(gridWidth_ * gridHeight_), Tile::Empty);

<<<<<<< HEAD
=======
    // 開始位置、ゴール位置、現在位置の基準を置き直します。
>>>>>>> origin/main
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

<<<<<<< HEAD
=======
    // 進捗報酬に使う Goal 距離表も、この地形に合わせて再計算します。
>>>>>>> origin/main
    /* Goal までの経路距離表も、このマップ配置に合わせて作り直します。 */
    RebuildGoalDistanceMap();

    usingLdtkMap_ = false;
    loadedLevelName_ = "BuiltIn";
}
<<<<<<< HEAD
`
</details>
<details>
<summary><strong>QLearningGrid::LoadMapFromLdtk(): LDtk ?????</strong></summary>
### ???
`cpp

void QLearningGrid::LoadMapFromLdtk(
    const std::filesystem::path& filePath,
    int levelIndex) {
    //========================================
    // プロジェクト読み込み
    //========================================

=======
```

### ここでやっていること

- 既定サイズへ戻します。
- `tiles_` を空マスで埋めます。
- Start / Goal / Pit / Wall を固定配置します。
- `RebuildGoalDistanceMap()` で距離表を作ります。

この関数は、  
**外部マップが使えないときの初期盤面を作る場所**です。

同時に、  
**学習しやすさを確認するための標準問題** を用意している場所でもあります。  
壁、落とし穴、回り込み経路があるので、ランダム移動から方策が育つ様子を観察しやすくなっています。

</details>

<details>
<summary><strong>QLearningGrid::LoadMapFromLdtk(): LDtk を読み込む</strong></summary>

### コード

```cpp
void QLearningGrid::LoadMapFromLdtk(
    const std::filesystem::path& filePath,
    int levelIndex) {
    // まず .ldtk 全体を読み込みます。
>>>>>>> origin/main
    const LdtkProjectData project = LoadLdtkProject(filePath);
    if (levelIndex < 0 || levelIndex >= static_cast<int>(project.levels.size())) {
        throw std::runtime_error("LDtk level index is out of range.");
    }

<<<<<<< HEAD
    //========================================
    // 対象レベル抽出
    //========================================

    const LdtkLevelData& level = project.levels[static_cast<std::size_t>(levelIndex)];

=======
    // その中から対象レベル 1 件だけを取り出します。
    const LdtkLevelData& level = project.levels[static_cast<std::size_t>(levelIndex)];

    // エンティティを、このプログラムが使う座標形式へ詰め直します。
>>>>>>> origin/main
    /* エンティティを内部形式へ詰め直す */
    std::vector<std::pair<std::string, GridPoint>> entities;
    entities.reserve(level.entities.size());
    for (const LdtkEntityData& entity : level.entities) {
        entities.push_back({entity.identifier, {entity.gridX, entity.gridY}});
    }

<<<<<<< HEAD
    //========================================
    // 内部マップへ反映
    //========================================

=======
    // レベル情報を内部グリッドへ反映します。
>>>>>>> origin/main
    ApplyLoadedMap(
        level.identifier,
        level.gridWidth,
        level.gridHeight,
        level.intGridCsv,
        entities);

    configuredLdtkPath_ = filePath;
    configuredLevelIndex_ = levelIndex;
    usingLdtkMap_ = true;

<<<<<<< HEAD
    /* Q テーブルと履歴は読み込み直し時にリセット */
    ResetLearningState();
}
`
</details>
<details>
<summary><strong>QLearningGrid::ApplyLoadedMap(): ???????????????????</strong></summary>
### ???
`cpp

=======
    // 地形が変わるので、Q テーブルや履歴もこのマップ向けに初期化し直します。
    /* Q テーブルと履歴は読み込み直し時にリセット */
    ResetLearningState();
}
```

### ここでやっていること

- `LoadLdtkProject()` で `.ldtk` 全体を読みます。
- 指定レベルを 1 件選びます。
- エンティティを内部形式へ詰め替えます。
- `ApplyLoadedMap()` で内部グリッドへ反映します。
- `ResetLearningState()` で学習状態もサイズに合わせて作り直します。

ここで重要なのは、  
**マップだけ差し替えて学習結果を使い回さない** ことです。  
もし Q テーブルを残したまま別マップへ切り替えると、前の地形で覚えた値が新しい地形に混ざってしまいます。

</details>

<details>
<summary><strong>QLearningGrid::ApplyLoadedMap(): 読み込んだレベルを内部マップへ変換する</strong></summary>

### コード

```cpp
>>>>>>> origin/main
void QLearningGrid::ApplyLoadedMap(
    const std::string& levelIdentifier,
    int gridWidth,
    int gridHeight,
    const std::vector<int>& intGridCsv,
    const std::vector<std::pair<std::string, GridPoint>>& entities) {
<<<<<<< HEAD
    //========================================
    // サイズ検証
    //========================================

=======
    // サイズ 0 以下のレベルは学習盤面として成立しないので弾きます。
>>>>>>> origin/main
    if (gridWidth <= 0 || gridHeight <= 0) {
        throw std::runtime_error("LDtk level size must be positive.");
    }

    const int cellCount = gridWidth * gridHeight;
    std::vector<Tile> loadedTiles(static_cast<std::size_t>(cellCount), Tile::Empty);
    GridPoint loadedStart = {0, 0};
    GridPoint loadedGoal = {gridWidth - 1, gridHeight - 1};

    bool hasStart = false;
    bool hasGoal = false;

<<<<<<< HEAD
    //========================================
    // IntGrid 反映
    //========================================

    /* まずセル単位の地形情報を敷き詰め、開始点とゴール候補もここで拾います。 */

=======
    // まず IntGrid から地形の土台を作ります。
>>>>>>> origin/main
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

<<<<<<< HEAD
    //========================================
    // エンティティ上書き
    //========================================

    /* その上からエンティティ情報を重ね、LDtk 側で明示した配置を優先させます。 */

=======
    // 次に Entities を重ねて、明示配置された Start / Goal / Pit / Wall を優先します。
>>>>>>> origin/main
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

<<<<<<< HEAD
    //========================================
    // 最低条件確認
    //========================================

=======
    // 学習を始めるには Start と Goal が必須です。
>>>>>>> origin/main
    if (!hasStart || !hasGoal) {
        throw std::runtime_error("LDtk map must contain both Start and Goal.");
    }

<<<<<<< HEAD
    /* 最後に Start と Goal を再代入し、他レイヤーの上書きで消えないよう確定させます。 */
=======
    // 最終的な地形・開始位置・目標位置をメンバへ確定反映します。
>>>>>>> origin/main
    loadedTiles[static_cast<std::size_t>(loadedStart.y * gridWidth + loadedStart.x)] = Tile::Start;
    loadedTiles[static_cast<std::size_t>(loadedGoal.y * gridWidth + loadedGoal.x)] = Tile::Goal;

    gridWidth_ = gridWidth;
    gridHeight_ = gridHeight;
    tiles_ = std::move(loadedTiles);
    start_ = loadedStart;
    goal_ = loadedGoal;
    agent_ = start_;

<<<<<<< HEAD
    /* 新しい地形へ切り替わったので、Goal までの距離地図も張り直します。 */
    RebuildGoalDistanceMap();
    loadedLevelName_ = levelIdentifier;
}
`
</details>
<details>
<summary><strong>QLearningGrid::RebuildGoalDistanceMap(): ????????????</strong></summary>
### ???
`cpp

void QLearningGrid::RebuildGoalDistanceMap() {
    /* 壁と落とし穴を避けた通行可能マスだけで、Goal から逆向き BFS を張ります。 */
    goalDistance_.assign(static_cast<std::size_t>(gridWidth_ * gridHeight_), -1);

=======
    RebuildGoalDistanceMap();
    loadedLevelName_ = levelIdentifier;
}
```

### ここでやっていること

- `IntGrid` を `Tile` へ変換します。
- Start / Goal の候補を拾います。
- エンティティで地形を上書きします。
- `tiles_` `start_` `goal_` `agent_` をまとめて更新します。
- `RebuildGoalDistanceMap()` を呼びます。

この関数は、  
**LDtk 側の表現を、このプログラムが本当に学習に使う内部表現へ落とし込む変換点**  
だと見ると分かりやすいです。

</details>

<details>
<summary><strong>QLearningGrid::RebuildGoalDistanceMap(): ゴールまでの距離表を作る</strong></summary>

### コード

```cpp
void QLearningGrid::RebuildGoalDistanceMap() {
    // まず全マスを未到達(-1)で初期化します。
    goalDistance_.assign(static_cast<std::size_t>(gridWidth_ * gridHeight_), -1);

    // Goal 自体が盤面外なら距離表は作れません。
>>>>>>> origin/main
    if (!IsInside(goal_)) {
        return;
    }

<<<<<<< HEAD
=======
    // Goal を始点に逆向き BFS を始めます。
>>>>>>> origin/main
    std::queue<GridPoint> frontier;
    goalDistance_[static_cast<std::size_t>(StateIndex(goal_))] = 0;
    frontier.push(goal_);

    while (!frontier.empty()) {
<<<<<<< HEAD
=======
        // 今取り出したマスの距離を基準に隣接 4 マスへ広げます。
>>>>>>> origin/main
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
<<<<<<< HEAD
=======
            // 盤面外は無視します。
>>>>>>> origin/main
            if (!IsInside(next)) {
                continue;
            }

            const Tile tile = GetTile(next.x, next.y);
<<<<<<< HEAD
=======
            // 壁と落とし穴は通れないので距離計算の対象外です。
>>>>>>> origin/main
            if (tile == Tile::Wall || tile == Tile::Pit) {
                continue;
            }

            const int nextIndex = StateIndex(next);
<<<<<<< HEAD
=======
            // すでに距離が確定しているマスは再訪しません。
>>>>>>> origin/main
            if (goalDistance_[static_cast<std::size_t>(nextIndex)] >= 0) {
                continue;
            }

<<<<<<< HEAD
=======
            // 1 手ぶん距離を足して記録し、次の探索候補へ入れます。
>>>>>>> origin/main
            goalDistance_[static_cast<std::size_t>(nextIndex)] = currentDistance + 1;
            frontier.push(next);
        }
    }
}
<<<<<<< HEAD
`
</details>
<details>
<summary><strong>LdtkLoader.cpp: JSON ???????</strong></summary>
### ParseIntGridCsv()
`cpp

//========================================
// LDtk レイヤー変換
//========================================

/* IntGrid 配列抽出 */
std::vector<int> ParseIntGridCsv(const JsonValue& layerValue) {
    std::vector<int> values;

    const JsonValue* csvValue = FindField(layerValue, "intGridCsv");
    if (csvValue == nullptr || csvValue->type == JsonType::Null) {
        return values;
    }

    const auto& csvArray = AsArray(*csvValue, "intGridCsv");
    values.reserve(csvArray.size());
    for (const JsonValue& entry : csvArray) {
        values.push_back(AsInt(entry, "intGridCsv entry"));
    }

    return values;
}
`
### ParseEntities()
`cpp

/* Entities 配列抽出 */
std::vector<LdtkEntityData> ParseEntities(const JsonValue& layerValue, int gridSize) {
    std::vector<LdtkEntityData> entities;

    const JsonValue* entitiesValue = FindField(layerValue, "entityInstances");
    if (entitiesValue == nullptr || entitiesValue->type == JsonType::Null) {
        return entities;
    }

    const auto& entityArray = AsArray(*entitiesValue, "entityInstances");
    entities.reserve(entityArray.size());

    for (const JsonValue& entityValue : entityArray) {
        AsObject(entityValue, "entity");

        LdtkEntityData entity;
        entity.identifier =
            AsString(RequireField(entityValue, "__identifier"), "__identifier");

        if (const JsonValue* gridValue = FindField(entityValue, "__grid");
            gridValue != nullptr && gridValue->type == JsonType::Array &&
            gridValue->arrayValue.size() >= 2) {
            entity.gridX = AsInt(gridValue->arrayValue[0], "__grid[0]");
            entity.gridY = AsInt(gridValue->arrayValue[1], "__grid[1]");
        } else {
            const JsonValue& pxValue = RequireField(entityValue, "px");
            const auto& pxArray = AsArray(pxValue, "px");
            if (pxArray.size() < 2) {
                throw std::runtime_error("px must contain at least 2 elements");
            }

            entity.gridX = AsInt(pxArray[0], "px[0]") / gridSize;
            entity.gridY = AsInt(pxArray[1], "px[1]") / gridSize;
        }

        entities.push_back(entity);
    }

    return entities;
}
`
</details>
<details>
<summary><strong>LoadLdtkProject(): .ldtk ?????????</strong></summary>
### ???
`cpp

//========================================
// 公開読み込み関数
//========================================

/* プロジェクト全体読み込み */
LdtkProjectData LoadLdtkProject(const std::filesystem::path& filePath) {
    /* まずファイル全体を読み込み、BOM 除去後の JSON テキストをパーサへ渡します。 */
    const std::string text = ReadTextFile(filePath);
    JsonParser parser(text);
    const JsonValue root = parser.Parse();

    AsObject(root, "root");

    const auto& levelArray = AsArray(RequireField(root, "levels"), "levels");

    /* `levels` 配列を上から順に変換し、このデモで使いやすい構造体列へ積み直します。 */
=======
```

### ここでやっていること

- `goalDistance_` を全部 `-1` で初期化します。
- Goal を始点に逆向き BFS をします。
- `Wall` と `Pit` は通しません。
- 通れるマスだけに最短距離を書き込みます。

`Simulate()` の進捗報酬は、  
**この距離表があるから「ゴールへ近づいたか」を判断できます。**

### なぜ Goal から逆向きに作るか

- Goal を始点にすると、すべての到達可能マスへ一度ずつ距離を広げていけます。
- BFS なので、最初に書かれた距離がそのまま最短距離になります。
- その結果、各マスから Goal までの「実際に通れる最短距離表」を 1 回で作れます。

</details>

<details>
<summary><strong>LdtkLoader.cpp: JSON を読む側の役割</strong></summary>

### 主な関数

- `LoadLdtkProject()`
  - ファイル全体読み込みと JSON 解析の入口です。
- `ParseLevel()`
  - レベル 1 件を `LdtkLevelData` に変換します。
- `ParseIntGridCsv()`
  - `intGridCsv` を整数列へ直します。
- `ParseEntities()`
  - `entityInstances` をエンティティ列へ直します。

### 役割

このファイルは、  
**LDtk の JSON を読みやすい内部構造へ変える担当**です。  
盤面そのものへ反映するのは `QLearningGrid::ApplyLoadedMap()` 側です。

</details>

<details>
<summary><strong>LoadLdtkProject(): .ldtk 全体の読み込み入口</strong></summary>

### コード

```cpp
LdtkProjectData LoadLdtkProject(const std::filesystem::path& filePath) {
    // まず .ldtk ファイル全体を文字列として読み込みます。
    const std::string text = ReadTextFile(filePath);

    // 続いて JSON として解析します。
    JsonParser parser(text);
    const JsonValue root = parser.Parse();

    // ルートが object であることを確認します。
    AsObject(root, "root");

    // levels 配列を取り出します。
    const auto& levelArray = AsArray(RequireField(root, "levels"), "levels");

    // 各 level を内部形式へ変換して project へ積みます。
>>>>>>> origin/main
    LdtkProjectData project;
    project.levels.reserve(levelArray.size());
    for (const JsonValue& levelValue : levelArray) {
        project.levels.push_back(ParseLevel(levelValue));
    }

<<<<<<< HEAD
=======
    // 1 件も level が無いファイルは、このデモでは無効とします。
>>>>>>> origin/main
    if (project.levels.empty()) {
        throw std::runtime_error("the LDtk file does not contain any levels");
    }

    return project;
}
<<<<<<< HEAD
`
</details>
<details>
<summary><strong>ParseLevel(): ??? 1 ???????????</strong></summary>
### ???
`cpp

/* レベル 1 件分の変換 */
LdtkLevelData ParseLevel(const JsonValue& levelValue) {
    AsObject(levelValue, "level");

    LdtkLevelData level;
    level.identifier = AsString(RequireField(levelValue, "identifier"), "identifier");

    /* LDtk の外部レベル分割はこの簡易ローダでは追わないので、同一ファイル内レベルだけ扱います。 */
=======
```

### ここでやっていること

- ファイル全体をテキストで読みます。
- JSON として解析します。
- `levels` 配列を順に `ParseLevel()` します。
- 最後に `LdtkProjectData` として返します。

この関数は、  
**LDtk ファイルからレベル群を取り出す最初の入口**です。

</details>

<details>
<summary><strong>ParseLevel(): レベル 1 件を内部形式へ変換する</strong></summary>

### コード

```cpp
LdtkLevelData ParseLevel(const JsonValue& levelValue) {
    // まず level オブジェクトとして扱えるか確認します。
    AsObject(levelValue, "level");

    // 結果を格納するレベル構造体を用意します。
    LdtkLevelData level;
    level.identifier = AsString(RequireField(levelValue, "identifier"), "identifier");

    // このデモでは外部レベルファイル分割には対応していません。
>>>>>>> origin/main
    const JsonValue* layersValue = FindField(levelValue, "layerInstances");
    if (layersValue == nullptr || layersValue->type == JsonType::Null) {
        throw std::runtime_error("external level files are not supported in this demo");
    }

    const auto& layers = AsArray(*layersValue, "layerInstances");
    bool hasCollision = false;

<<<<<<< HEAD
    /* 各レイヤーを見ながら、地形用 IntGrid と補助用 Entities を取り分けます。 */
=======
    // 各レイヤーを見て、IntGrid と Entities を拾います。
>>>>>>> origin/main
    for (const JsonValue& layerValue : layers) {
        AsObject(layerValue, "layer");

        const std::string layerType =
            AsString(RequireField(layerValue, "__type"), "__type");
        const std::string layerIdentifier =
            AsString(RequireField(layerValue, "__identifier"), "__identifier");

        if (layerType == "IntGrid") {
<<<<<<< HEAD
            /* 実際にマップ本体として使う IntGrid を採用し、サイズ情報もここで拾います。 */
=======
            // IntGrid から地形セル値を取ります。
>>>>>>> origin/main
            const std::vector<int> gridValues = ParseIntGridCsv(layerValue);
            if (gridValues.empty()) {
                continue;
            }

<<<<<<< HEAD
=======
            // Collision レイヤーを優先し、それ以外の IntGrid は必要に応じて読み飛ばします。
>>>>>>> origin/main
            if (hasCollision && layerIdentifier != "Collision") {
                continue;
            }

            level.gridWidth = AsInt(RequireField(layerValue, "__cWid"), "__cWid");
            level.gridHeight = AsInt(RequireField(layerValue, "__cHei"), "__cHei");
            level.intGridCsv = gridValues;
            hasCollision = true;
        } else if (layerType == "Entities") {
<<<<<<< HEAD
            /* Start / Goal などの個別エンティティは、別レイヤーから補助情報として集めます。 */
=======
            // Entities から Start / Goal などの配置情報を取ります。
>>>>>>> origin/main
            const int gridSize =
                AsInt(RequireField(layerValue, "__gridSize"), "__gridSize");
            level.entities = ParseEntities(layerValue, gridSize);
        }
    }

<<<<<<< HEAD
    /* 学習デモとして最低限必要な地形サイズと IntGrid 本体がそろっているか確認します。 */
=======
    // 学習盤面には IntGrid が必須です。
>>>>>>> origin/main
    if (!hasCollision) {
        throw std::runtime_error("IntGrid layer was not found");
    }

<<<<<<< HEAD
=======
    // サイズやセル数が壊れていないか最終確認します。
>>>>>>> origin/main
    if (level.gridWidth <= 0 || level.gridHeight <= 0) {
        throw std::runtime_error("invalid LDtk grid size");
    }

    if (level.intGridCsv.size() !=
        static_cast<std::size_t>(level.gridWidth * level.gridHeight)) {
        throw std::runtime_error("intGridCsv size does not match the level size");
    }

    return level;
}
<<<<<<< HEAD
`
</details>
---
## Rendering
<details>
<summary><strong>BuildSceneVertices(): ??????????</strong></summary>
### ???
`cpp

//==================================
// 公開関数
//==================================

=======
```

### ここでやっていること

- レベル名を取ります。
- 横幅と縦幅を取ります。
- レイヤー配列を走査します。
- `IntGrid` レイヤーから地形値を取ります。
- `Entities` レイヤーから Start / Goal などの配置を取ります。

つまり、  
**LDtk の 1 レベルを、このプログラムが使える形へほどく関数**です。

</details>

---

## 描画

<details>
<summary><strong>BuildSceneVertices(): 画面全体の頂点を作る</strong></summary>

### コード

```cpp
>>>>>>> origin/main
std::vector<Vertex> BuildSceneVertices(
    const QLearningGrid& world,
    unsigned int windowWidth,
    unsigned int windowHeight,
    const std::wstring& speedLabel,
    bool paused,
    const EpisodeRunUiState& episodeRunUiState) {
<<<<<<< HEAD
    /* シーン組み立て本体 */
    // 背景 -> 盤面 -> パネル -> 文字の順で頂点を積み上げます。
=======
    // まず現在のウィンドウサイズと盤面サイズから配置を決めます。
>>>>>>> origin/main
    const float width = static_cast<float>(windowWidth);
    const float height = static_cast<float>(windowHeight);
    const SceneLayout layout =
        BuildLayout(width, height, world.GetGridWidth(), world.GetGridHeight());

<<<<<<< HEAD
    std::vector<Vertex> vertices;
    vertices.reserve(120000);

    // 小見出し
    // 背景 -> 盤面 -> 情報パネル -> 文字の順に積むと重なり順が自然です。

=======
    // 1 フレームぶんの図形を全部ここへ積んでいきます。
    std::vector<Vertex> vertices;
    vertices.reserve(120000);

    // 背景 -> 盤面 -> 右パネル -> 文字の順で重ねると見た目が自然です。
>>>>>>> origin/main
    AppendBackground(vertices, layout, width, height);
    AppendGrid(vertices, world, layout, width, height);
    AppendMetricsPanel(vertices, world, layout, width, height);
    AppendLegendPanel(vertices, layout, width, height);
    AppendTextBitmapGeometry(vertices, layout, world, speedLabel, paused, episodeRunUiState, width, height);
    return vertices;
}
<<<<<<< HEAD
`
</details>
<details>
<summary><strong>AppendGrid(): ???????????</strong></summary>
### ???
`cpp

=======
```

### ここでやっていること

- `BuildLayout()` で盤面と右 UI の矩形を決めます。
- `AppendBackground()` で背景を積みます。
- `AppendGrid()` で盤面、経路、アイコン、エージェントを積みます。
- `AppendMetricsCard()` で学習状況カードを積みます。
- `AppendLegendCard()` で凡例カードを積みます。

つまり、  
**今の世界の見た目を全部三角形列へ変換する関数**です。

### ここをそう読むと分かりやすい

- `QLearningGrid`
  - 学習世界そのもの
- `BuildSceneVertices()`
  - 学習世界を見た目の部品へ翻訳する場所
- `Dx12Renderer`
  - その部品を画面へ出す場所

この関数は、学習ロジックと描画 API のちょうど中間にあります。

</details>

<details>
<summary><strong>AppendGrid(): 盤面をどう描いているか</strong></summary>

### コード

```cpp
>>>>>>> origin/main
void AppendGrid(
    std::vector<Vertex>& vertices,
    const QLearningGrid& world,
    const SceneLayout& layout,
    float width,
    float height) {
<<<<<<< HEAD
    /* 盤面の外枠 */
    // まずグリッド全体の背景と枠線を描きます。
    AppendQuad(vertices, layout.grid, width, height, {0.05f, 0.07f, 0.11f, 1.0f});
    AppendFrame(vertices, layout.grid, 4.0f, width, height, {0.27f, 0.43f, 0.64f, 1.0f});

    /* 推定経路ライン */
    // 現在の最善行動に従った経路を半透明ラインで重ねます。
    const std::vector<GridPoint> path = world.BuildGreedyPath();
    const float cellSize = CellSizePx(layout, world);
    const float pathThickness = std::max(2.0f, cellSize * 0.09f);
    // 小見出し
    // greedy 経路は各セル背景の上、アイコンの下に薄く通します。
=======
    AppendQuad(vertices, layout.grid, width, height, {0.05f, 0.07f, 0.11f, 1.0f});
    AppendFrame(vertices, layout.grid, 4.0f, width, height, {0.27f, 0.43f, 0.64f, 1.0f});

    const std::vector<GridPoint> path = world.BuildGreedyPath();
    const float cellSize = CellSizePx(layout, world);
    const float pathThickness = std::max(2.0f, cellSize * 0.09f);
>>>>>>> origin/main
    for (size_t i = 1; i < path.size(); ++i) {
        const PointPx from = CellCenter(layout, world, path[i - 1].x, path[i - 1].y);
        const PointPx to = CellCenter(layout, world, path[i].x, path[i].y);
        AppendSegment(vertices, from, to, pathThickness, width, height, {0.98f, 0.80f, 0.24f, 0.48f});
    }

<<<<<<< HEAD
    /* 各セルの描画 */
    // マス背景、地形アイコン、推奨行動矢印をセルごとに描きます。
=======
>>>>>>> origin/main
    for (int y = 0; y < world.GetGridHeight(); ++y) {
        for (int x = 0; x < world.GetGridWidth(); ++x) {
            const RectPx cell = CellRect(layout, world, x, y);
            const float innerPadding = std::max(1.0f, cellSize * 0.05f);
            const float wallInset = std::max(1.0f, cellSize * 0.12f);
            const float crossThickness = std::max(1.5f, cellSize * 0.045f);
            const RectPx inner = {
                cell.left + innerPadding,
                cell.top + innerPadding,
                cell.right - innerPadding,
                cell.bottom - innerPadding,
            };
            AppendQuad(vertices, inner, width, height, CellBaseColor(world, x, y));
            AppendFrame(vertices, inner, 1.5f, width, height, {0.10f, 0.14f, 0.20f, 1.0f});

            const Tile tile = world.GetTile(x, y);
            const PointPx center = CellCenter(layout, world, x, y);
            const float iconRadius = (inner.right - inner.left) * 0.18f;

<<<<<<< HEAD
            // 小見出し
            // 壁だけは塗りつぶし専用で処理し、矢印などは載せません。
=======
>>>>>>> origin/main
            if (tile == Tile::Wall) {
                AppendQuad(
                    vertices,
                    {inner.left + wallInset, inner.top + wallInset, inner.right - wallInset, inner.bottom - wallInset},
                    width,
                    height,
                    {0.28f, 0.31f, 0.37f, 1.0f});
                continue;
            }

            if (tile == Tile::Start) {
                AppendDiamond(vertices, center.x, center.y, iconRadius * 1.15f, width, height, {0.84f, 0.97f, 1.0f, 0.96f});
                AppendDiamond(vertices, center.x, center.y, iconRadius * 0.65f, width, height, {0.13f, 0.66f, 0.88f, 1.0f});
            } else if (tile == Tile::Goal) {
                AppendDiamond(vertices, center.x, center.y, iconRadius * 1.15f, width, height, {0.91f, 1.0f, 0.92f, 0.96f});
                AppendDiamond(vertices, center.x, center.y, iconRadius * 0.70f, width, height, {0.10f, 0.58f, 0.24f, 1.0f});
            } else if (tile == Tile::Pit) {
                AppendCross(vertices, center.x, center.y, iconRadius * 1.10f, crossThickness, width, height, {1.0f, 0.92f, 0.92f, 0.96f});
                AppendDiamond(vertices, center.x, center.y, iconRadius * 0.85f, width, height, {0.60f, 0.10f, 0.10f, 1.0f});
            }

            if (!world.IsTerminal(x, y) && tile != Tile::Wall) {
<<<<<<< HEAD
                // 小見出し
                // 通常マスには、現在もっとも有望な行動方向を矢印で載せます。
=======
>>>>>>> origin/main
                AppendArrow(
                    vertices,
                    center.x,
                    center.y,
                    iconRadius * 0.95f,
                    world.GetBestAction(x, y),
                    width,
                    height,
                    {0.02f, 0.04f, 0.08f, 0.86f});
            }
        }
    }

<<<<<<< HEAD
    /* エージェント描画 */
    // 現在位置だけは強調表示して目立たせます。
=======
>>>>>>> origin/main
    const GridPoint agent = world.GetAgent();
    const PointPx center = CellCenter(layout, world, agent.x, agent.y);
    const float glowHalfSize = cellSize * 0.27f;
    const float outerAgentRadius = cellSize * 0.22f;
    const float innerAgentRadius = cellSize * 0.13f;
    AppendQuad(
        vertices,
        {center.x - glowHalfSize, center.y - glowHalfSize, center.x + glowHalfSize, center.y + glowHalfSize},
        width,
        height,
        {1.0f, 0.88f, 0.35f, 0.22f});
    AppendDiamond(vertices, center.x, center.y, outerAgentRadius, width, height, {1.0f, 0.94f, 0.72f, 1.0f});
    AppendDiamond(vertices, center.x, center.y, innerAgentRadius, width, height, {0.95f, 0.73f, 0.18f, 1.0f});
}
<<<<<<< HEAD
`
</details>
<details>
<summary><strong>CellBaseColor(): ?????????</strong></summary>
### ???
`cpp

Color CellBaseColor(const QLearningGrid& world, int x, int y) {
    /* セル基本色 */
    // 壁やゴールは固定色、通常マスは Q 値に応じたグラデーションで色付けします。
=======
```

### ここでやっていること

- 盤面の外枠を描きます。
- `BuildGreedyPath()` の結果を黄色ラインで描きます。
- 各セルの背景色を描きます。
- Start / Goal / Pit / Wall のアイコンを描きます。
- そのセルでの最善行動を矢印で描きます。
- エージェントを最後に重ねます。

描画順にも意味があります。

- 先にセル背景
- その上に経路やアイコン
- 最後にエージェント

と重ねることで、今どこにいるかが埋もれにくくなっています。

</details>

<details>
<summary><strong>CellBaseColor(): 各セルの色を決める</strong></summary>

### コード

```cpp
Color CellBaseColor(const QLearningGrid& world, int x, int y) {
    // 特殊マスは役割が一目で分かるよう固定色にします。
>>>>>>> origin/main
    switch (world.GetTile(x, y)) {
    case Tile::Wall:
        return {0.16f, 0.18f, 0.24f, 1.0f};
    case Tile::Start:
        return {0.13f, 0.66f, 0.88f, 1.0f};
    case Tile::Goal:
        return {0.16f, 0.77f, 0.34f, 1.0f};
    case Tile::Pit:
        return {0.84f, 0.24f, 0.24f, 1.0f};
    case Tile::Empty:
    default:
        break;
    }

<<<<<<< HEAD
=======
    // 通常マスだけは、その地点の価値に応じて色を補間します。
>>>>>>> origin/main
    const float value = world.GetBestValue(x, y);
    const float normalized = NormalizeRange(value, -1.5f, 8.0f);
    return LerpColor(
        {0.08f, 0.12f, 0.19f, 1.0f},
        {0.29f, 0.78f, 0.99f, 1.0f},
        normalized);
}
<<<<<<< HEAD
`
</details>
<details>
<summary><strong>BuildLayout(): ???? UI ???????</strong></summary>
### ???
`cpp

//==================================
// レイアウト
//==================================

SceneLayout BuildLayout(float width, float height, int gridWidth, int gridHeight) {
    /* 全体レイアウト計算 */
    // 画面サイズと現在マップの縦横比から、セル形状を崩さず収まる盤面矩形を決めます。
=======
```

### ここでやっていること

- `Wall`、`Start`、`Goal`、`Pit` は固定色を返します。
- `Empty` だけは `GetBestValue()` を使って色を補間します。

### 意味

通常マスは、  
**価値が高いほど明るく、低いほど暗く**  
見えるようにしてあります。

</details>

<details>
<summary><strong>BuildLayout(): 盤面と右 UI の配置を決める</strong></summary>

### コード

```cpp
SceneLayout BuildLayout(float width, float height, int gridWidth, int gridHeight) {
    // 外周余白、盤面とパネルの隙間、右パネル幅を固定で持ちます。
>>>>>>> origin/main
    constexpr float margin = 34.0f;
    constexpr float gap = 26.0f;
    constexpr float panelWidth = 380.0f;

<<<<<<< HEAD
    const float gridAreaWidth = width - margin * 2.0f - gap - panelWidth;
    const float gridAreaHeight = height - margin * 2.0f;
=======
    // 盤面に使える残り領域を計算します。
    const float gridAreaWidth = width - margin * 2.0f - gap - panelWidth;
    const float gridAreaHeight = height - margin * 2.0f;

    // セルが正方形のまま収まる最大サイズを選びます。
>>>>>>> origin/main
    const float cellSize = std::min(
        gridAreaWidth / static_cast<float>(std::max(1, gridWidth)),
        gridAreaHeight / static_cast<float>(std::max(1, gridHeight)));
    const float gridPixelWidth = cellSize * static_cast<float>(gridWidth);
    const float gridPixelHeight = cellSize * static_cast<float>(gridHeight);
    const float gridLeft = margin + (gridAreaWidth - gridPixelWidth) * 0.5f;
    const float gridTop = margin + (gridAreaHeight - gridPixelHeight) * 0.5f;

<<<<<<< HEAD
=======
    // 盤面、右パネル、各カードの矩形をまとめて返します。
>>>>>>> origin/main
    SceneLayout layout = {};
    layout.grid = {gridLeft, gridTop, gridLeft + gridPixelWidth, gridTop + gridPixelHeight};
    layout.panel = {layout.grid.right + gap, margin, width - margin, height - margin};
    layout.metricsCard = {
        layout.panel.left + 18.0f,
        layout.panel.top + 18.0f,
        layout.panel.right - 18.0f,
        layout.panel.top + 448.0f,
    };
    layout.legendCard = {
        layout.panel.left + 18.0f,
        layout.metricsCard.bottom + 18.0f,
        layout.panel.right - 18.0f,
        layout.panel.bottom - 18.0f,
    };
<<<<<<< HEAD
    // 小見出し
    // こうしておくと、以後の描画関数は「どのカードを使うか」だけで座標を共有できます。
    return layout;
}
`
</details>
<details>
<summary><strong>AppendMetricsPanel(): ??????????</strong></summary>
### ???
`cpp

=======
    return layout;
}
```

### ここでやっていること

- ウィンドウの幅と高さを見ます。
- 現在のマップの横幅と縦幅を見ます。
- セルが正方形のまま収まる大きさを計算します。
- その結果から `grid`、`panel`、`metricsCard`、`legendCard` の矩形を決めます。

この関数があるので、  
**10x10 でも長方形マップでも、盤面と UI の配置が崩れにくくなっています。**

レイアウトを 1 か所へまとめているので、  
盤面サイズが変わっても「描画側」と「クリック判定側」が同じ基準で動けます。  
これは UI の見た目と当たり判定をずらさないために大事です。

</details>

<details>
<summary><strong>AppendMetricsPanel(): 学習状況カードを描く</strong></summary>

### コード

```cpp
>>>>>>> origin/main
void AppendMetricsPanel(
    std::vector<Vertex>& vertices,
    const QLearningGrid& world,
    const SceneLayout& layout,
    float width,
    float height) {
<<<<<<< HEAD
    /* カード背景 */
    AppendQuad(vertices, layout.metricsCard, width, height, {0.09f, 0.12f, 0.19f, 0.97f});
    AppendFrame(vertices, layout.metricsCard, 2.0f, width, height, {0.22f, 0.32f, 0.46f, 1.0f});

    /* メトリクス値計算 */
    // Q 学習の主要指標を 0..1 に正規化してバーで表せる形へ変換します。
=======
    AppendQuad(vertices, layout.metricsCard, width, height, {0.09f, 0.12f, 0.19f, 0.97f});
    AppendFrame(vertices, layout.metricsCard, 2.0f, width, height, {0.22f, 0.32f, 0.46f, 1.0f});

>>>>>>> origin/main
    const int greedyPath = world.MeasureGreedyPathLength();
    const int maxSimplePath = std::max(1, world.GetGridWidth() * world.GetGridHeight() - 1);
    const float pathRatio = greedyPath >= 0
        ? Clamp01((static_cast<float>(maxSimplePath) - static_cast<float>(greedyPath)) /
                  static_cast<float>(maxSimplePath))
        : 0.0f;

    const std::array<float, 4> values = {
        world.GetEpsilon(),
        NormalizeRange(world.GetAverageReward(), -8.0f, 10.0f),
        world.GetRecentSuccessRate(),
        pathRatio,
    };
    const std::array<Color, 4> colors = {
        Color{0.92f, 0.62f, 0.17f, 1.0f},
        Color{0.24f, 0.72f, 0.96f, 1.0f},
        Color{0.20f, 0.78f, 0.34f, 1.0f},
        Color{0.80f, 0.38f, 0.92f, 1.0f},
    };

<<<<<<< HEAD
    /* メトリクスバー描画 */
    // 小見出し
    // バーの土台、現在値、外枠を行ごとに重ねます。
=======
>>>>>>> origin/main
    for (int row = 0; row < 4; ++row) {
        const RectPx bar = MetricBarRect(layout, row);
        const RectPx fill = {
            bar.left,
            bar.top,
            bar.left + (bar.right - bar.left) * Clamp01(values[row]),
            bar.bottom,
        };
        AppendQuad(vertices, bar, width, height, {0.13f, 0.17f, 0.25f, 1.0f});
        AppendQuad(vertices, fill, width, height, colors[row]);
        AppendFrame(vertices, bar, 1.0f, width, height, {0.22f, 0.30f, 0.41f, 1.0f});
    }

<<<<<<< HEAD
    /* 入力欄背景 */
    // 右下のエピソード目標入力欄だけは白背景で目立たせます。
=======
>>>>>>> origin/main
    const RectPx inputRect = EpisodeTargetInputRectPx(layout);
    AppendQuad(vertices, inputRect, width, height, {0.98f, 0.99f, 1.0f, 1.0f});
    AppendFrame(vertices, inputRect, 2.0f, width, height, {0.70f, 0.75f, 0.82f, 1.0f});
}
<<<<<<< HEAD
`
</details>
<details>
<summary><strong>AppendLegendPanel(): ????????????</strong></summary>
### ???
`cpp

=======
```

### ここでやっていること

- カード背景と枠を描きます。
- ランダム率、平均報酬、直近成功率のバーを描きます。
- 目標エピソード入力欄の背景と枠も描きます。

右側の `学習状況` パネルの見た目を作っているのがこの関数です。

</details>

<details>
<summary><strong>AppendLegendPanel(): 凡例と経路サンプルを描く</strong></summary>

### コード

```cpp
>>>>>>> origin/main
void AppendLegendPanel(
    std::vector<Vertex>& vertices,
    const SceneLayout& layout,
    float width,
    float height) {
<<<<<<< HEAD
    /* 凡例カード背景 */
=======
>>>>>>> origin/main
    AppendQuad(vertices, layout.legendCard, width, height, {0.09f, 0.12f, 0.19f, 0.97f});
    AppendFrame(vertices, layout.legendCard, 2.0f, width, height, {0.22f, 0.32f, 0.46f, 1.0f});

    const float left = layout.legendCard.left + 22.0f;
    const float top = layout.legendCard.top + 72.0f;
    const float rowGap = 54.0f;

<<<<<<< HEAD
    // 小見出し
    // 凡例のアイコンは盤面と同じ見た目で揃えています。

    /* スウォッチ枠 */
=======
>>>>>>> origin/main
    for (int row = 0; row < 5; ++row) {
        const RectPx swatch = {
            left,
            top + rowGap * static_cast<float>(row),
            left + 54.0f,
            top + rowGap * static_cast<float>(row) + 36.0f,
        };
        AppendQuad(vertices, swatch, width, height, {0.11f, 0.15f, 0.23f, 1.0f});
        AppendFrame(vertices, swatch, 1.0f, width, height, {0.22f, 0.30f, 0.41f, 1.0f});
    }

    AppendDiamond(vertices, left + 27.0f, top + 18.0f, 13.0f, width, height, {0.84f, 0.97f, 1.0f, 0.96f});
    AppendDiamond(vertices, left + 27.0f, top + 18.0f, 8.0f, width, height, {0.13f, 0.66f, 0.88f, 1.0f});
<<<<<<< HEAD

    AppendDiamond(vertices, left + 27.0f, top + rowGap + 18.0f, 14.0f, width, height, {0.91f, 1.0f, 0.92f, 0.96f});
    AppendDiamond(vertices, left + 27.0f, top + rowGap + 18.0f, 8.5f, width, height, {0.10f, 0.58f, 0.24f, 1.0f});

    AppendCross(vertices, left + 27.0f, top + rowGap * 2.0f + 18.0f, 13.0f, 4.0f, width, height, {1.0f, 0.92f, 0.92f, 0.96f});
    AppendDiamond(vertices, left + 27.0f, top + rowGap * 2.0f + 18.0f, 9.0f, width, height, {0.60f, 0.10f, 0.10f, 1.0f});

    AppendQuad(vertices, {left + 10.0f, top + rowGap * 3.0f + 8.0f, left + 44.0f, top + rowGap * 3.0f + 28.0f}, width, height, {0.28f, 0.31f, 0.37f, 1.0f});

    AppendDiamond(vertices, left + 27.0f, top + rowGap * 4.0f + 18.0f, 14.0f, width, height, {1.0f, 0.94f, 0.72f, 1.0f});
    AppendDiamond(vertices, left + 27.0f, top + rowGap * 4.0f + 18.0f, 9.0f, width, height, {0.95f, 0.73f, 0.18f, 1.0f});

    /* 経路サンプル */
    // 下部には最善経路ラインの見本も載せています。
=======
    AppendDiamond(vertices, left + 27.0f, top + rowGap + 18.0f, 14.0f, width, height, {0.91f, 1.0f, 0.92f, 0.96f});
    AppendDiamond(vertices, left + 27.0f, top + rowGap + 18.0f, 8.5f, width, height, {0.10f, 0.58f, 0.24f, 1.0f});
    AppendCross(vertices, left + 27.0f, top + rowGap * 2.0f + 18.0f, 13.0f, 4.0f, width, height, {1.0f, 0.92f, 0.92f, 0.96f});
    AppendDiamond(vertices, left + 27.0f, top + rowGap * 2.0f + 18.0f, 9.0f, width, height, {0.60f, 0.10f, 0.10f, 1.0f});
    AppendQuad(vertices, {left + 10.0f, top + rowGap * 3.0f + 8.0f, left + 44.0f, top + rowGap * 3.0f + 28.0f}, width, height, {0.28f, 0.31f, 0.37f, 1.0f});
    AppendDiamond(vertices, left + 27.0f, top + rowGap * 4.0f + 18.0f, 14.0f, width, height, {1.0f, 0.94f, 0.72f, 1.0f});
    AppendDiamond(vertices, left + 27.0f, top + rowGap * 4.0f + 18.0f, 9.0f, width, height, {0.95f, 0.73f, 0.18f, 1.0f});

>>>>>>> origin/main
    const RectPx routeBox = {
        layout.legendCard.left + 18.0f,
        layout.legendCard.bottom - 130.0f,
        layout.legendCard.right - 18.0f,
        layout.legendCard.bottom - 18.0f,
    };
    AppendFrame(vertices, routeBox, 1.0f, width, height, {0.22f, 0.30f, 0.41f, 1.0f});

    const PointPx a{routeBox.left + 18.0f, routeBox.bottom - 24.0f};
    const PointPx b{routeBox.left + 96.0f, routeBox.top + 78.0f};
    const PointPx c{routeBox.right - 24.0f, routeBox.top + 74.0f};
    AppendSegment(vertices, a, b, 8.0f, width, height, {0.98f, 0.80f, 0.24f, 0.60f});
    AppendSegment(vertices, b, c, 8.0f, width, height, {0.98f, 0.80f, 0.24f, 0.60f});
    AppendArrow(vertices, b.x, b.y, 12.0f, Action::Right, width, height, {0.03f, 0.05f, 0.09f, 0.95f});
}
<<<<<<< HEAD
`
</details>
<details>
<summary><strong>DrawSceneOverlayText(): ?????????</strong></summary>
### ???
`cpp

=======
```

### ここでやっていること

- 凡例カードの背景と枠を描きます。
- 開始地点、ゴール、落とし穴、壁、エージェントの見本を置きます。
- 最良経路のサンプル線も描きます。

右下の `見方と操作` に近い説明領域を構成しているのがこの関数です。

</details>

<details>
<summary><strong>DrawSceneOverlayText(): 文字だけ別で重ねる</strong></summary>

### コード

```cpp
>>>>>>> origin/main
void DrawSceneOverlayText(
    HWND hwnd,
    const QLearningGrid& world,
    unsigned int windowWidth,
    unsigned int windowHeight,
    const std::wstring& speedLabel,
    bool paused,
    const EpisodeRunUiState& episodeRunUiState) {
<<<<<<< HEAD
    // 小見出し
    // 現在は未使用ですが、将来の拡張用に空実装として残しています。
=======
    // 現在は文字描画を別経路へ移したため、この関数は空の受け口だけ残しています。
>>>>>>> origin/main
    (void)hwnd;
    (void)world;
    (void)windowWidth;
    (void)windowHeight;
    (void)speedLabel;
    (void)paused;
    (void)episodeRunUiState;
}
<<<<<<< HEAD
`
</details>
<details>
<summary><strong>BuildWindowTitle(): ????????????</strong></summary>
### ???
`cpp

=======
```

- この公開関数自体は現在は空実装です。
- 文字表示の実体は `AppendTextBitmapGeometry()` 側へ寄っています。

ただし現在の実装では、  
この関数は **空実装のまま残されています。**  
文字表示は `AppendTextBitmapGeometry()` 側へ移っているので、README 上でもその事実を押さえて読む必要があります。

</details>

<details>
<summary><strong>BuildWindowTitle(): タイトルバー文字列を作る</strong></summary>

### コード

```cpp
>>>>>>> origin/main
std::wstring BuildWindowTitle(
    const QLearningGrid& world,
    const std::wstring& speedLabel,
    bool paused,
    const EpisodeRunUiState& episodeRunUiState) {
<<<<<<< HEAD
    /* タイトル行組み立て */
    // 現在の学習状況を 1 行で素早く確認できる文字列にまとめます。
=======
>>>>>>> origin/main
    std::wostringstream stream;
    stream << L"強化学習デモ | エピソード " << world.GetEpisodeCount()
           << L" | 総歩数 " << world.GetTrainingStepCount()
           << L" | 今回 " << world.GetCurrentEpisodeSteps() << L" 歩"
           << L" | サイズ " << world.GetGridWidth() << L"x" << world.GetGridHeight()
           << L" | マップ " << world.GetMapDisplayName()
           << L" | 速度 " << (paused ? L"一時停止" : speedLabel)
           << L" | ランダム " << std::fixed << std::setprecision(2) << world.GetEpsilon()
           << L" | 成功率 " << std::setprecision(2) << world.GetRecentSuccessRate();

    const int greedyPath = world.MeasureGreedyPathLength();
    if (greedyPath >= 0) {
        stream << L" | 最良経路 " << greedyPath << L" 歩";
    } else {
        stream << L" | 最良経路 まだ未発見";
    }

    if (episodeRunUiState.autoRunning && episodeRunUiState.targetEpisode >= 0) {
        stream << L" | 自動 " << episodeRunUiState.targetEpisode << L" まで";
    } else if (episodeRunUiState.editing) {
        stream << L" | 目標入力 " << FormatEpisodeTargetInput(episodeRunUiState);
    }

    return stream.str();
}
<<<<<<< HEAD
`
</details>
<details>
<summary><strong>GetEpisodeTargetInputRect(): ???????????????</strong></summary>
### ???
`cpp

=======
```

- エピソード数
- 速度
- 停止状態
- マップ名

を 1 行へまとめます。

</details>

<details>
<summary><strong>GetEpisodeTargetInputRect(): 入力欄のクリック判定矩形を返す</strong></summary>

### コード

```cpp
>>>>>>> origin/main
RECT GetEpisodeTargetInputRect(
    const QLearningGrid& world,
    unsigned int windowWidth,
    unsigned int windowHeight) {
<<<<<<< HEAD
    /* 入力欄矩形の公開版 */
=======
>>>>>>> origin/main
    const SceneLayout layout = BuildLayout(
        static_cast<float>(windowWidth),
        static_cast<float>(windowHeight),
        world.GetGridWidth(),
        world.GetGridHeight());
    const RectPx rect = EpisodeTargetInputRectPx(layout);
    return {
        static_cast<LONG>(std::lround(rect.left)),
        static_cast<LONG>(std::lround(rect.top)),
        static_cast<LONG>(std::lround(rect.right)),
        static_cast<LONG>(std::lround(rect.bottom)),
    };
}
<<<<<<< HEAD
`
</details>
---
## Renderer Backend
<details>
<summary><strong>Dx12Renderer: DirectX12 ??????</strong></summary>
### ???
`cpp
#pragma once

#define NOMINMAX
#include <windows.h>
#include <wrl/client.h>

#include <d3d12.h>
#include <dxgi1_4.h>

#include <vector>

#include "SharedTypes.h"

//========================================
// Dx12Renderer クラス宣言
//========================================
// 画面初期化、頂点アップロード、1 フレーム描画を担当する DirectX12 ラッパーです。

class Dx12Renderer {
public:
    //========================================
    // 公開操作
    //========================================

    /* Map したバッファと同期イベントを安全に解放します。 */
    ~Dx12Renderer();

    /* Device 生成から PSO 構築まで、描画に必要な DirectX12 オブジェクトをそろえます。 */
    void Initialize(HWND hwnd, UINT width, UINT height);

    /* CPU で組んだ頂点列を upload heap へコピーし、描画ビューを更新します。 */
    void UploadVertices(const std::vector<Vertex>& vertices);

    /* コマンド記録から Present までを 1 フレーム分まとめて実行します。 */
    void Render(UINT vertexCount);

private:
    //========================================
    // 固定設定
    //========================================

    /* Present と描画を交互に回しやすい、最小構成のダブルバッファです。 */
    static constexpr UINT kFrameCount = 2;

    /* 毎フレーム再確保しないよう、十分大きい頂点バッファを最初に確保します。 */
    static constexpr size_t kMaxVertices = 262144;

    //========================================
    // 初期化工程
    //========================================

    /* DXGI ファクトリを作り、使える GPU から D3D12 Device を確立します。 */
    void CreateFactoryAndDevice();

    /* コマンドキュー、アロケータ、コマンドリストをそろえます。 */
    void CreateCommandObjects();

    /* ウィンドウ表示用のバックバッファ列を作ります。 */
    void CreateSwapChain();

    /* 各バックバッファへ描き込むための RTV を割り当てます。 */
    void CreateRenderTargetViews();

    /* シェーダーをコンパイルし、固定機能設定と合わせて PSO を作ります。 */
    void CreatePipeline();

    /* 毎フレーム使い回す upload heap の頂点バッファを確保します。 */
    void CreateVertexBuffer();

    /* Present 後に GPU 完了を待つためのフェンスとイベントを作ります。 */
    void CreateFence();

    //========================================
    // 描画補助
    //========================================

    /* 直前フレームの GPU 実行完了を待ち、次のフレームで安全に再利用できるようにします。 */
    void WaitForGpu();

    /* 現在の frameIndex_ が指すバックバッファ用 RTV ハンドルを計算します。 */
    D3D12_CPU_DESCRIPTOR_HANDLE CurrentRenderTargetView() const;

    //========================================
    // 基本情報
    //========================================

    /* Present 先になる Win32 ウィンドウです。 */
    HWND hwnd_ = nullptr;

    /* ビューポートとスワップチェーンに使う描画幅です。 */
    UINT width_ = 0;

    /* ビューポートとスワップチェーンに使う描画高さです。 */
    UINT height_ = 0;

    //========================================
    // 実行時状態
    //========================================

    /* 今から描く先のバックバッファ番号です。Present ごとに切り替わります。 */
    UINT frameIndex_ = 0;

    /* RTV ヒープ内で次ハンドルへ進むためのサイズです。 */
    UINT rtvDescriptorSize_ = 0;

    /* 各フレーム送信後に GPU 完了待ちへ使う単調増加カウンタです。 */
    UINT64 fenceValue_ = 0;

    /* フェンス完了時に OS が通知するイベントハンドルです。 */
    HANDLE fenceEvent_ = nullptr;

    /* upload heap を Map したまま保持する CPU 書き込み先です。 */
    Vertex* mappedVertexData_ = nullptr;

    /* 今回描く頂点数に合わせて毎フレーム更新する IA 用ビューです。 */
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_ = {};

    //========================================
    // DirectX オブジェクト
    //========================================

    /* DXGI オブジェクト群です。 */
    Microsoft::WRL::ComPtr<IDXGIFactory4> factory_;

    /* GPU 本体へリソース生成やコマンド作成を依頼する窓口です。 */
    Microsoft::WRL::ComPtr<ID3D12Device> device_;

    /* 記録済みコマンド列を GPU へ送るキューです。 */
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue_;

    /* 画面表示用のバックバッファ列です。 */
    Microsoft::WRL::ComPtr<IDXGISwapChain3> swapChain_;

    /* バックバッファ用 RTV を並べるディスクリプタヒープです。 */
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap_;

    /* 実際のバックバッファリソース群です。 */
    Microsoft::WRL::ComPtr<ID3D12Resource> renderTargets_[kFrameCount];

    /* フレームごとのコマンド記録領域です。 */
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator_;

    /* 描画コマンドを書き込むリストです。 */
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_;

    /* GPU 完了を CPU 側が待つための同期オブジェクトです。 */
    Microsoft::WRL::ComPtr<ID3D12Fence> fence_;

    /* このアプリの単純な頂点色描画に必要なルートシグネチャです。 */
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;

    /* シェーダーと固定機能設定を束ねた描画パイプライン状態です。 */
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;

    /* 毎フレーム CPU から直接書き込む頂点バッファです。 */
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer_;
};
`
</details>
<details>
<summary><strong>Dx12Renderer::Initialize(): DirectX12 ??????</strong></summary>
### ???
`cpp

void Dx12Renderer::Initialize(HWND hwnd, UINT width, UINT height) {
    //========================================
    // 基本情報保持
    //========================================

    hwnd_ = hwnd;
    width_ = width;
    height_ = height;

    //========================================
    // 初期化工程
    //========================================

    CreateFactoryAndDevice();
    CreateCommandObjects();
    CreateSwapChain();
    CreateRenderTargetViews();
    CreatePipeline();
}
`
</details>
<details>
<summary><strong>Dx12Renderer::UploadVertices(): CPU ??????? GPU ???</strong></summary>
### ???
`cpp

//========================================
// 頂点アップロード
//========================================

void Dx12Renderer::UploadVertices(const std::vector<Vertex>& vertices) {
    /* 固定長 upload heap を使っているので、想定上限を超えた時点で明示的に止めます。 */
    if (vertices.size() > kMaxVertices) {
        ThrowWithMessage("vertex count exceeds upload buffer capacity");
    }

    /* CPU で組んだ頂点列を、そのまま Map 済みメモリへ連続コピーします。 */
    if (!vertices.empty()) {
        std::memcpy(
            mappedVertexData_,
            vertices.data(),
            vertices.size() * sizeof(Vertex));
    }

    /* 今回描く頂点数だけ IA が読むよう、ビューのサイズをその都度合わせます。 */
    vertexBufferView_.BufferLocation = vertexBuffer_->GetGPUVirtualAddress();
    vertexBufferView_.StrideInBytes = sizeof(Vertex);
    vertexBufferView_.SizeInBytes = static_cast<UINT>(vertices.size() * sizeof(Vertex));
}
`
</details>
<details>
<summary><strong>Dx12Renderer::Render(): 1 ??????????</strong></summary>
### ???
`cpp

//========================================
// 1 フレーム描画
//========================================

void Dx12Renderer::Render(UINT vertexCount) {
    //========================================
    // コマンドリスト準備
    //========================================

=======
```

### ここでやっていること

- `BuildLayout()` で今の UI 配置を作ります。
- `EpisodeTargetInputRectPx()` で入力欄の矩形を取ります。
- それを Win32 の `RECT` に変換して返します。

この関数があるので、  
**マップサイズが変わっても、入力欄の見た目とクリック判定位置がずれません。**

</details>

<details>
<summary><strong>Dx12Renderer: DirectX12 側の描画処理</strong></summary>

### 重要な関数

- `Initialize()`
  - DirectX12 の初期化全体です。
- `UploadVertices()`
  - CPU 側頂点列を GPU へコピーします。
- `Render()`
  - 1 フレーム描画します。

### `Render()` でやっていること

- コマンドリストをリセットします。
- バックバッファを RenderTarget 状態へ遷移します。
- 画面をクリアします。
- ルートシグネチャ、PSO、頂点バッファを設定します。
- `DrawInstanced()` で描きます。
- Present 状態へ戻します。
- `Present()` します。

ここで `SceneBuilder` と違うのは、  
「どんな見た目か」を考えていない点です。  
`Dx12Renderer` は、渡された頂点を正しく GPU へ通して表示することだけに集中しています。

</details>

<details>
<summary><strong>Dx12Renderer::Render(): 1 フレームを実際に描く</strong></summary>

### コード

```cpp
void Dx12Renderer::Render(UINT vertexCount) {
    // まず前フレームの記録領域を再利用できる状態へ戻します。
>>>>>>> origin/main
    ThrowIfFailed(commandAllocator_->Reset(), "ID3D12CommandAllocator::Reset");
    ThrowIfFailed(
        commandList_->Reset(commandAllocator_.Get(), pipelineState_.Get()),
        "ID3D12GraphicsCommandList::Reset");

<<<<<<< HEAD
    //========================================
    // 描画範囲設定
    //========================================

    /* ウィンドウ全体へ描くので、ビューポートとシザーは画面サイズそのままを使います。 */
=======
    // 描画先はウィンドウ全体なので、ビューポートとシザーを全面に設定します。
>>>>>>> origin/main
    D3D12_VIEWPORT viewport = {};
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = static_cast<float>(width_);
    viewport.Height = static_cast<float>(height_);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    commandList_->RSSetViewports(1, &viewport);

    D3D12_RECT scissorRect = {};
    scissorRect.left = 0;
    scissorRect.top = 0;
    scissorRect.right = static_cast<LONG>(width_);
    scissorRect.bottom = static_cast<LONG>(height_);
    commandList_->RSSetScissorRects(1, &scissorRect);

<<<<<<< HEAD
    //========================================
    // パイプライン設定
    //========================================

=======
    // どのルートシグネチャ・トポロジ・頂点バッファで描くかを指定します。
>>>>>>> origin/main
    commandList_->SetGraphicsRootSignature(rootSignature_.Get());
    commandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList_->IASetVertexBuffers(0, 1, &vertexBufferView_);

<<<<<<< HEAD
    //========================================
    // 描画開始前の状態遷移
    //========================================

=======
    // Present 用バッファを、描画できる RenderTarget 状態へ切り替えます。
>>>>>>> origin/main
    D3D12_RESOURCE_BARRIER toRender = MakeTransitionBarrier(
        renderTargets_[frameIndex_].Get(),
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET);
    commandList_->ResourceBarrier(1, &toRender);

<<<<<<< HEAD
    //========================================
    // 画面クリア
    //========================================

    /* 今回のバックバッファを取得し、まずは背景色で全面クリアします。 */
=======
    // まず背景色で全面クリアしてから図形を重ねます。
>>>>>>> origin/main
    const D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = CurrentRenderTargetView();
    constexpr float clearColor[] = {0.03f, 0.05f, 0.09f, 1.0f};
    commandList_->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
    commandList_->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

<<<<<<< HEAD
    //========================================
    // 頂点描画
    //========================================

    if (vertexCount > 0) {
        commandList_->DrawInstanced(vertexCount, 1, 0, 0);
    }

    //========================================
    // 表示直前の状態遷移
    //========================================

    D3D12_RESOURCE_BARRIER toPresent = MakeTransitionBarrier(
        renderTargets_[frameIndex_].Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT);
    commandList_->ResourceBarrier(1, &toPresent);

    //========================================
    // 実行と表示
    //========================================

    ThrowIfFailed(commandList_->Close(), "ID3D12GraphicsCommandList::Close");

    ID3D12CommandList* commandLists[] = {commandList_.Get()};
    commandQueue_->ExecuteCommandLists(1, commandLists);

    ThrowIfFailed(swapChain_->Present(1, 0), "IDXGISwapChain::Present");
    WaitForGpu();
}
`
</details>
<details>
<summary><strong>Dx12Renderer::WaitForGpu(): GPU ????</strong></summary>
### ???
`cpp

//========================================
// GPU 同期
//========================================

void Dx12Renderer::WaitForGpu() {
    /* 現在フレーム終了をシグナル */
=======
    // 頂点が 1 つでもあれば、その個数ぶん三角形列として描きます。
    if (vertexCount > 0) {
        commandList_->DrawInstanced(vertexCount, 1, 0, 0);
    }
}
```

### 役割

- コマンドリストを準備します。
- バックバッファを描画可能状態へ遷移します。
- 画面をクリアします。
- 頂点を描きます。
- Present して表示します。

`BuildSceneVertices()` が「何を描くか」を作る側なら、  
`Render()` は **それを本当に画面へ出す最後の関数**です。

DirectX12 の流れに慣れていない場合は、

1. コマンド準備
2. 状態遷移
3. クリア
4. 描画
5. 表示

の 5 段階で読むと理解しやすいです。

</details>

<details>
<summary><strong>Dx12Renderer::WaitForGpu(): GPU 完了待ち</strong></summary>

### コード

```cpp
void Dx12Renderer::WaitForGpu() {
    // 今回のフレーム完了を示す番号を GPU へ送ります。
>>>>>>> origin/main
    const UINT64 signalValue = fenceValue_++;
    ThrowIfFailed(
        commandQueue_->Signal(fence_.Get(), signalValue),
        "ID3D12CommandQueue::Signal");

<<<<<<< HEAD
    /* GPU が追い付いていなければ待つ */
=======
    // GPU がまだそこへ届いていなければ、イベント待ちで同期します。
>>>>>>> origin/main
    if (fence_->GetCompletedValue() < signalValue) {
        ThrowIfFailed(
            fence_->SetEventOnCompletion(signalValue, fenceEvent_),
            "ID3D12Fence::SetEventOnCompletion");
        WaitForSingleObject(fenceEvent_, INFINITE);
    }

<<<<<<< HEAD
    /* Present 後のバックバッファ番号を取り直す */
    frameIndex_ = swapChain_->GetCurrentBackBufferIndex();
}
`
</details>
---
## CSV Export
<details>
<summary><strong>ExportEpisodeHistoryCsv(): ?????????</strong></summary>
### ???
`cpp

//========================================
// 公開 CSV 出力
//========================================

std::filesystem::path ExportEpisodeHistoryCsv(
    const QLearningGrid& world,
    const std::filesystem::path& outputDirectory) {
    //========================================
    // 出力先準備
    //========================================

    /* フォルダ作成 */
    std::filesystem::create_directories(outputDirectory);

    /* 保存先パス確定 */
    const std::filesystem::path outputPath = outputDirectory / "episode_history.csv";

    //========================================
    // ファイルオープン
    //========================================

    /* 上書き保存で開く */
=======
    // Present 後に現在のバックバッファ番号を取り直します。
    frameIndex_ = swapChain_->GetCurrentBackBufferIndex();
}
```

### 役割

- GPU が前のフレームを使い終わるまで待ちます。
- 次フレームで同じバッファやコマンド領域を安全に再利用できるようにします。

これは学習とは直接関係ありませんが、  
描画の安定動作には必須です。  
待たずに再利用すると、まだ GPU が読んでいる資源を CPU が上書きしてしまう危険があります。

</details>

<details>
<summary><strong>Dx12Renderer::Initialize(): DirectX12 の初期化全体</strong></summary>

### コード

```cpp
void Dx12Renderer::Initialize(HWND hwnd, UINT width, UINT height) {
    // 描画先ウィンドウとサイズをメンバへ保存します。
    hwnd_ = hwnd;
    width_ = width;
    height_ = height;

    // DirectX12 描画に必要な要素を順番にそろえます。
    CreateFactoryAndDevice();
    CreateCommandObjects();
    CreateSwapChain();
    CreateRenderTargetViews();
    CreatePipeline();
}
```

### ここでやっていること

- `CreateFactoryAndDevice()` で device を作ります。
- `CreateCommandObjects()` でコマンド系を作ります。
- `CreateSwapChain()` でバックバッファ列を作ります。
- `CreateRenderTargetViews()` で RTV を作ります。
- `CreatePipeline()` でシェーダーと PSO を作ります。
- `CreateVertexBuffer()` で頂点バッファを確保します。
- `CreateFence()` で同期オブジェクトを作ります。

この関数は、  
**描画が始まる前に DirectX12 の必要物を一式そろえる場所**です。

初期化段階を 1 つずつ分けているので、  
もし失敗しても「device 作成で止まったのか」「swap chain で止まったのか」を追いやすくなっています。

</details>

<details>
<summary><strong>Dx12Renderer::UploadVertices(): CPU で組んだ頂点を GPU へ渡す</strong></summary>

### コード

```cpp
void Dx12Renderer::UploadVertices(const std::vector<Vertex>& vertices) {
    // 固定長バッファを使うので、上限超過は明示的に止めます。
    if (vertices.size() > kMaxVertices) {
        ThrowWithMessage("vertex count exceeds upload buffer capacity");
    }

    // CPU で組んだ頂点列を、そのまま upload heap へ連続コピーします。
    if (!vertices.empty()) {
        std::memcpy(
            mappedVertexData_,
            vertices.data(),
            vertices.size() * sizeof(Vertex));
    }

    // 今回描く頂点数に合わせて、GPU 側が読むビュー情報を更新します。
    vertexBufferView_.BufferLocation = vertexBuffer_->GetGPUVirtualAddress();
    vertexBufferView_.StrideInBytes = sizeof(Vertex);
    vertexBufferView_.SizeInBytes = static_cast<UINT>(vertices.size() * sizeof(Vertex));
}
```

### ここでやっていること

- `BuildSceneVertices()` が返した頂点列を upload heap へコピーします。
- 頂点数に合わせて `vertexBufferView_` を更新します。

つまり、  
**SceneBuilder が作った見た目を、実際に GPU が読める形へ渡す橋渡し**です。

ここでは図形の意味は一切見ていません。  
四角形なのか矢印なのか経路なのかは関係なく、  
**頂点列をそのまま GPU 用バッファへ写す** ことだけを担当しています。

</details>

---

## CSV 出力

<details>
<summary><strong>ExportEpisodeHistoryCsv(): 学習履歴を書き出す</strong></summary>

### コード

```cpp
std::filesystem::path ExportEpisodeHistoryCsv(
    const QLearningGrid& world,
    const std::filesystem::path& outputDirectory) {
    // 出力先フォルダが無ければ先に作ります。
    std::filesystem::create_directories(outputDirectory);

    // 今回の保存先ファイル名を決めます。
    const std::filesystem::path outputPath = outputDirectory / "episode_history.csv";

    // 既存内容は使わないので、毎回上書き保存で開きます。
>>>>>>> origin/main
    std::ofstream output(outputPath, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("failed to open CSV output file");
    }

<<<<<<< HEAD
    //========================================
    // ヘッダー出力
    //========================================

    /* BOM 付与 */
    WriteUtf8Bom(output);

    /* 列名行 */
    output << "エピソード,ランダム率,平均報酬,直近成功率,最良経路長,手数\n";

    //========================================
    // 本文出力
    //========================================

    /* 画面で蓄積してきた履歴スナップショットを、そのまま CSV の元データとして使います。 */
    const auto& history = world.GetEpisodeHistory();

    /* 各レコードを「数値列 + 特殊文字列表現」の順で 1 行ずつ組み立てます。 */
    for (const auto& record : history) {

        // まず毎回必ず存在する数値項目を、列順を崩さずに並べます。
=======
    // Excel で崩れにくいよう BOM 付き UTF-8 にします。
    WriteUtf8Bom(output);

    // 先頭に列名を 1 行出します。
    output << "エピソード,ランダム率,平均報酬,直近成功率,最良経路長,手数\n";

    // 蓄積済みの履歴を 1 レコードずつ CSV 行へ変換します。
    const auto& history = world.GetEpisodeHistory();
    for (const auto& record : history) {
        // まずは必ず存在する数値列を順に出力します。
>>>>>>> origin/main
        output << record.episode << ','
               << FormatFloat(record.epsilon) << ','
               << FormatFloat(record.averageReward) << ','
               << FormatFloat(record.recentSuccessRate) << ',';

<<<<<<< HEAD

        // greedy 経路がまだゴールへ届いていない段階だけ、人が読める文字列へ置き換えます。
=======
        // 最良経路がまだ見つかっていない間だけは説明文字列を入れます。
>>>>>>> origin/main
        if (record.bestPathLength >= 0) {
            output << record.bestPathLength;
        } else {
            output << "経路未発見";
        }

<<<<<<< HEAD

        // 最後にそのエピソードの手数を付けて 1 行を閉じます。
        output << ',' << record.steps << '\n';
    }

    //========================================
    // 保存先通知
    //========================================

    /* 保存ダイアログを使わないので、書き込んだ実パスを呼び出し元が通知表示に使えるよう渡します。 */
    return outputPath;
}
`
</details>
<details>
<summary><strong>WriteUtf8Bom() / FormatFloat(): CSV ?????</strong></summary>
### WriteUtf8Bom()
`cpp

//========================================
// ローカル補助関数
//========================================

/* UTF-8 BOM 書き込み */
// Excel などで日本語ヘッダーが崩れにくいよう、先頭へ BOM を付けます。
void WriteUtf8Bom(std::ofstream& stream) {
    const unsigned char bom[] = {0xEF, 0xBB, 0xBF};
    stream.write(reinterpret_cast<const char*>(bom), sizeof(bom));
}
`
### FormatFloat()
`cpp

/* 小数点書式統一 */
// 比較しやすいよう、小数の出力桁数を固定します。
std::string FormatFloat(float value) {
=======
        // 最後に手数を書いて 1 行を閉じます。
        output << ',' << record.steps << '\n';
    }

    return outputPath;
}
```

### ここでやっていること

- 出力先フォルダを準備します。
- 日時つきファイル名を作ります。
- UTF-8 BOM を付けます。
- ヘッダ行を書きます。
- `world.GetEpisodeHistory()` の各行を書きます。

出力内容は主に、

- エピソード番号
- 探索率
- 平均報酬
- 直近成功率
- 最良経路長
- 手数

です。

</details>

<details>
<summary><strong>WriteUtf8Bom() / FormatFloat(): CSV 出力の補助</strong></summary>

### `WriteUtf8Bom()`

```cpp
void WriteUtf8Bom(std::ofstream& stream) {
    // UTF-8 BOM を先頭へ付け、Excel で日本語が崩れにくい形にします。
    const unsigned char bom[] = {0xEF, 0xBB, 0xBF};
    stream.write(reinterpret_cast<const char*>(bom), sizeof(bom));
}
```

- CSV の先頭へ UTF-8 BOM を付けます。
- Excel で日本語が崩れにくいようにするための処理です。

### `FormatFloat()`

```cpp
std::string FormatFloat(float value) {
    // 小数桁数を固定して、CSV の見た目をそろえます。
>>>>>>> origin/main
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(4) << value;
    return stream.str();
}
<<<<<<< HEAD
`
</details>
---
## Shared Types
<details>
<summary><strong>SharedTypes.h</strong></summary>
### ???
`cpp
#pragma once

#include <algorithm>
#include <cstdint>

//========================================
// 共有定数
//========================================
// 複数ファイルで共通利用する組み込みマップ既定値やウィンドウサイズを定義します。

/* 組み込みマップを生成するときに使う既定の横マス数です。 */
constexpr int kBuiltInGridWidth = 10;

/* 組み込みマップを生成するときに使う既定の縦マス数です。 */
constexpr int kBuiltInGridHeight = 10;

/* Up / Right / Down / Left の 4 行動を前提に Q テーブルを確保します。 */
constexpr int kActionCount = 4;

/* 盤面と右パネルを並べても余白が取れる固定ウィンドウ幅です。 */
constexpr unsigned int kWindowWidth = 1400;

/* HUD のカード群を縦に積んでも詰まりにくい固定ウィンドウ高さです。 */
constexpr unsigned int kWindowHeight = 920;

/* 旧実装互換のため残している定数で、現在は主に参照用です。 */
constexpr int kTrainingStepsPerFrame = 1;

//========================================
// 座標と列挙型
//========================================

/* 盤面上のマス位置を整数の列・行で表す最小単位です。 */
struct GridPoint {
    int x = 0;
    int y = 0;
};

/* 学習盤面の各セルがどんな役割を持つかを表す列挙です。 */
enum class Tile {
    Empty,
    Start,
    Goal,
    Wall,
    Pit,
};

/* エージェントが 1 手で選べる 4 方向です。Q テーブルの添字順でもあります。 */
enum class Action : int {
    Up = 0,
    Right = 1,
    Down = 2,
    Left = 3,
};

//========================================
// 描画データ
//========================================

/* 画面描画で使う RGBA 色です。0..1 範囲の float を想定しています。 */
struct Color {
    float r;
    float g;
    float b;
    float a;
};

/* DirectX へ渡す 1 頂点分の位置と色です。 */
struct Vertex {
    float x;
    float y;
    float z;
    float r;
    float g;
    float b;
    float a;
};

//========================================
// 補助関数
//========================================

/* 補間やゲージ描画で使う値を、必ず 0..1 の範囲へ収めます。 */
inline float Clamp01(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

/* 実数範囲を 0..1 へ写し、ゲージや色補間へ使いやすい形へ変換します。 */
inline float NormalizeRange(float value, float minValue, float maxValue) {
    if (maxValue <= minValue) {
        return 0.0f;
    }
    return Clamp01((value - minValue) / (maxValue - minValue));
}

/* 2 色の間を t に応じて線形補間し、連続的な色変化を作ります。 */
inline Color LerpColor(const Color& from, const Color& to, float t) {
    const float clamped = Clamp01(t);
    return {
        from.r + (to.r - from.r) * clamped,
        from.g + (to.g - from.g) * clamped,
        from.b + (to.b - from.b) * clamped,
        from.a + (to.a - from.a) * clamped,
    };
}
`
=======
```

- 小数を文字列へ整えます。
- 小数桁をそろえて CSV を読みやすくする役目です。

</details>

---

## 補助の共有型

<details>
<summary><strong>SharedTypes.h</strong></summary>

### 主なもの

- `GridPoint`
  - グリッド座標です。
- `Tile`
  - `Empty / Start / Goal / Wall / Pit`
- `Action`
  - `Up / Right / Down / Left`
- `Color`
  - 描画色です。
- `Vertex`
  - DirectX へ渡す頂点です。

### 補助関数

- `Clamp01()`
  - 値を `0..1` に収めます。
- `NormalizeRange()`
  - 任意範囲を `0..1` に正規化します。
- `LerpColor()`
  - 2 色を補間します。

SceneBuilder の色補間やバー表示は、ここにある補助関数を使っています。

>>>>>>> origin/main
</details>
