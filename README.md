# Q-Learning Demo README
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
        std::cout
            << "DirectX12 Q-Learning demo\n"
            << "1=slow 2=normal 3=fast 4=max "
            << "Space=pause N=step Enter=target episode "
            << "R=reload map E=export csv Esc=quit\n";

        //========================================
        // 本体実行
        //========================================

        /* ここから先は Win32 と DirectX12 の初期化、メインループ実行を Application へ委ねます。 */
        Application app;
        app.Run();
        return 0;
    } catch (const std::exception& exception) {
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
`
</details>
<details>
<summary><strong>QLearningGrid::ResetLearningState(): ??????????</strong></summary>
### ???
`cpp

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
`
</details>
<details>
<summary><strong>QLearningGrid::ResetEpisode(): ???????????????</strong></summary>
### ???
`cpp

void QLearningGrid::ResetEpisode() {
    /* 次の試行は必ず Start から始めるため、位置・手数・累積報酬を初期化します。 */
    agent_ = start_;
    episodeSteps_ = 0;
    episodeReward_ = 0.0f;
}
`
</details>
<details>
<summary><strong>QLearningGrid::BuildGreedyPath(): ??????????????</strong></summary>
### ???
`cpp

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
`
</details>
<details>
<summary><strong>QLearningGrid::GetBestAction(): ?????????????</strong></summary>
### ???
`cpp

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
    if (!path.empty() &&
        path.back().x == goal_.x &&
        path.back().y == goal_.y) {
        return static_cast<int>(path.size()) - 1;
    }
    return -1;
}
`
</details>
<details>
<summary><strong>QLearningGrid::GetAverageReward() / GetRecentSuccessRate(): ????????</strong></summary>
### ???
`cpp

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
`
</details>
<details>
<summary><strong>QLearningGrid::ApplyLoadedMap(): ???????????????????</strong></summary>
### ???
`cpp

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
`
</details>
<details>
<summary><strong>QLearningGrid::RebuildGoalDistanceMap(): ????????????</strong></summary>
### ???
`cpp

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
    LdtkProjectData project;
    project.levels.reserve(levelArray.size());
    for (const JsonValue& levelValue : levelArray) {
        project.levels.push_back(ParseLevel(levelValue));
    }

    if (project.levels.empty()) {
        throw std::runtime_error("the LDtk file does not contain any levels");
    }

    return project;
}
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
    const JsonValue* layersValue = FindField(levelValue, "layerInstances");
    if (layersValue == nullptr || layersValue->type == JsonType::Null) {
        throw std::runtime_error("external level files are not supported in this demo");
    }

    const auto& layers = AsArray(*layersValue, "layerInstances");
    bool hasCollision = false;

    /* 各レイヤーを見ながら、地形用 IntGrid と補助用 Entities を取り分けます。 */
    for (const JsonValue& layerValue : layers) {
        AsObject(layerValue, "layer");

        const std::string layerType =
            AsString(RequireField(layerValue, "__type"), "__type");
        const std::string layerIdentifier =
            AsString(RequireField(layerValue, "__identifier"), "__identifier");

        if (layerType == "IntGrid") {
            /* 実際にマップ本体として使う IntGrid を採用し、サイズ情報もここで拾います。 */
            const std::vector<int> gridValues = ParseIntGridCsv(layerValue);
            if (gridValues.empty()) {
                continue;
            }

            if (hasCollision && layerIdentifier != "Collision") {
                continue;
            }

            level.gridWidth = AsInt(RequireField(layerValue, "__cWid"), "__cWid");
            level.gridHeight = AsInt(RequireField(layerValue, "__cHei"), "__cHei");
            level.intGridCsv = gridValues;
            hasCollision = true;
        } else if (layerType == "Entities") {
            /* Start / Goal などの個別エンティティは、別レイヤーから補助情報として集めます。 */
            const int gridSize =
                AsInt(RequireField(layerValue, "__gridSize"), "__gridSize");
            level.entities = ParseEntities(layerValue, gridSize);
        }
    }

    /* 学習デモとして最低限必要な地形サイズと IntGrid 本体がそろっているか確認します。 */
    if (!hasCollision) {
        throw std::runtime_error("IntGrid layer was not found");
    }

    if (level.gridWidth <= 0 || level.gridHeight <= 0) {
        throw std::runtime_error("invalid LDtk grid size");
    }

    if (level.intGridCsv.size() !=
        static_cast<std::size_t>(level.gridWidth * level.gridHeight)) {
        throw std::runtime_error("intGridCsv size does not match the level size");
    }

    return level;
}
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

std::vector<Vertex> BuildSceneVertices(
    const QLearningGrid& world,
    unsigned int windowWidth,
    unsigned int windowHeight,
    const std::wstring& speedLabel,
    bool paused,
    const EpisodeRunUiState& episodeRunUiState) {
    /* シーン組み立て本体 */
    // 背景 -> 盤面 -> パネル -> 文字の順で頂点を積み上げます。
    const float width = static_cast<float>(windowWidth);
    const float height = static_cast<float>(windowHeight);
    const SceneLayout layout =
        BuildLayout(width, height, world.GetGridWidth(), world.GetGridHeight());

    std::vector<Vertex> vertices;
    vertices.reserve(120000);

    // 小見出し
    // 背景 -> 盤面 -> 情報パネル -> 文字の順に積むと重なり順が自然です。

    AppendBackground(vertices, layout, width, height);
    AppendGrid(vertices, world, layout, width, height);
    AppendMetricsPanel(vertices, world, layout, width, height);
    AppendLegendPanel(vertices, layout, width, height);
    AppendTextBitmapGeometry(vertices, layout, world, speedLabel, paused, episodeRunUiState, width, height);
    return vertices;
}
`
</details>
<details>
<summary><strong>AppendGrid(): ???????????</strong></summary>
### ???
`cpp

void AppendGrid(
    std::vector<Vertex>& vertices,
    const QLearningGrid& world,
    const SceneLayout& layout,
    float width,
    float height) {
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
    for (size_t i = 1; i < path.size(); ++i) {
        const PointPx from = CellCenter(layout, world, path[i - 1].x, path[i - 1].y);
        const PointPx to = CellCenter(layout, world, path[i].x, path[i].y);
        AppendSegment(vertices, from, to, pathThickness, width, height, {0.98f, 0.80f, 0.24f, 0.48f});
    }

    /* 各セルの描画 */
    // マス背景、地形アイコン、推奨行動矢印をセルごとに描きます。
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

            // 小見出し
            // 壁だけは塗りつぶし専用で処理し、矢印などは載せません。
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
                // 小見出し
                // 通常マスには、現在もっとも有望な行動方向を矢印で載せます。
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

    /* エージェント描画 */
    // 現在位置だけは強調表示して目立たせます。
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
`
</details>
<details>
<summary><strong>CellBaseColor(): ?????????</strong></summary>
### ???
`cpp

Color CellBaseColor(const QLearningGrid& world, int x, int y) {
    /* セル基本色 */
    // 壁やゴールは固定色、通常マスは Q 値に応じたグラデーションで色付けします。
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

    const float value = world.GetBestValue(x, y);
    const float normalized = NormalizeRange(value, -1.5f, 8.0f);
    return LerpColor(
        {0.08f, 0.12f, 0.19f, 1.0f},
        {0.29f, 0.78f, 0.99f, 1.0f},
        normalized);
}
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
    constexpr float margin = 34.0f;
    constexpr float gap = 26.0f;
    constexpr float panelWidth = 380.0f;

    const float gridAreaWidth = width - margin * 2.0f - gap - panelWidth;
    const float gridAreaHeight = height - margin * 2.0f;
    const float cellSize = std::min(
        gridAreaWidth / static_cast<float>(std::max(1, gridWidth)),
        gridAreaHeight / static_cast<float>(std::max(1, gridHeight)));
    const float gridPixelWidth = cellSize * static_cast<float>(gridWidth);
    const float gridPixelHeight = cellSize * static_cast<float>(gridHeight);
    const float gridLeft = margin + (gridAreaWidth - gridPixelWidth) * 0.5f;
    const float gridTop = margin + (gridAreaHeight - gridPixelHeight) * 0.5f;

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

void AppendMetricsPanel(
    std::vector<Vertex>& vertices,
    const QLearningGrid& world,
    const SceneLayout& layout,
    float width,
    float height) {
    /* カード背景 */
    AppendQuad(vertices, layout.metricsCard, width, height, {0.09f, 0.12f, 0.19f, 0.97f});
    AppendFrame(vertices, layout.metricsCard, 2.0f, width, height, {0.22f, 0.32f, 0.46f, 1.0f});

    /* メトリクス値計算 */
    // Q 学習の主要指標を 0..1 に正規化してバーで表せる形へ変換します。
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

    /* メトリクスバー描画 */
    // 小見出し
    // バーの土台、現在値、外枠を行ごとに重ねます。
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

    /* 入力欄背景 */
    // 右下のエピソード目標入力欄だけは白背景で目立たせます。
    const RectPx inputRect = EpisodeTargetInputRectPx(layout);
    AppendQuad(vertices, inputRect, width, height, {0.98f, 0.99f, 1.0f, 1.0f});
    AppendFrame(vertices, inputRect, 2.0f, width, height, {0.70f, 0.75f, 0.82f, 1.0f});
}
`
</details>
<details>
<summary><strong>AppendLegendPanel(): ????????????</strong></summary>
### ???
`cpp

void AppendLegendPanel(
    std::vector<Vertex>& vertices,
    const SceneLayout& layout,
    float width,
    float height) {
    /* 凡例カード背景 */
    AppendQuad(vertices, layout.legendCard, width, height, {0.09f, 0.12f, 0.19f, 0.97f});
    AppendFrame(vertices, layout.legendCard, 2.0f, width, height, {0.22f, 0.32f, 0.46f, 1.0f});

    const float left = layout.legendCard.left + 22.0f;
    const float top = layout.legendCard.top + 72.0f;
    const float rowGap = 54.0f;

    // 小見出し
    // 凡例のアイコンは盤面と同じ見た目で揃えています。

    /* スウォッチ枠 */
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

    AppendDiamond(vertices, left + 27.0f, top + rowGap + 18.0f, 14.0f, width, height, {0.91f, 1.0f, 0.92f, 0.96f});
    AppendDiamond(vertices, left + 27.0f, top + rowGap + 18.0f, 8.5f, width, height, {0.10f, 0.58f, 0.24f, 1.0f});

    AppendCross(vertices, left + 27.0f, top + rowGap * 2.0f + 18.0f, 13.0f, 4.0f, width, height, {1.0f, 0.92f, 0.92f, 0.96f});
    AppendDiamond(vertices, left + 27.0f, top + rowGap * 2.0f + 18.0f, 9.0f, width, height, {0.60f, 0.10f, 0.10f, 1.0f});

    AppendQuad(vertices, {left + 10.0f, top + rowGap * 3.0f + 8.0f, left + 44.0f, top + rowGap * 3.0f + 28.0f}, width, height, {0.28f, 0.31f, 0.37f, 1.0f});

    AppendDiamond(vertices, left + 27.0f, top + rowGap * 4.0f + 18.0f, 14.0f, width, height, {1.0f, 0.94f, 0.72f, 1.0f});
    AppendDiamond(vertices, left + 27.0f, top + rowGap * 4.0f + 18.0f, 9.0f, width, height, {0.95f, 0.73f, 0.18f, 1.0f});

    /* 経路サンプル */
    // 下部には最善経路ラインの見本も載せています。
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
`
</details>
<details>
<summary><strong>DrawSceneOverlayText(): ?????????</strong></summary>
### ???
`cpp

void DrawSceneOverlayText(
    HWND hwnd,
    const QLearningGrid& world,
    unsigned int windowWidth,
    unsigned int windowHeight,
    const std::wstring& speedLabel,
    bool paused,
    const EpisodeRunUiState& episodeRunUiState) {
    // 小見出し
    // 現在は未使用ですが、将来の拡張用に空実装として残しています。
    (void)hwnd;
    (void)world;
    (void)windowWidth;
    (void)windowHeight;
    (void)speedLabel;
    (void)paused;
    (void)episodeRunUiState;
}
`
</details>
<details>
<summary><strong>BuildWindowTitle(): ????????????</strong></summary>
### ???
`cpp

std::wstring BuildWindowTitle(
    const QLearningGrid& world,
    const std::wstring& speedLabel,
    bool paused,
    const EpisodeRunUiState& episodeRunUiState) {
    /* タイトル行組み立て */
    // 現在の学習状況を 1 行で素早く確認できる文字列にまとめます。
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
`
</details>
<details>
<summary><strong>GetEpisodeTargetInputRect(): ???????????????</strong></summary>
### ???
`cpp

RECT GetEpisodeTargetInputRect(
    const QLearningGrid& world,
    unsigned int windowWidth,
    unsigned int windowHeight) {
    /* 入力欄矩形の公開版 */
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

    ThrowIfFailed(commandAllocator_->Reset(), "ID3D12CommandAllocator::Reset");
    ThrowIfFailed(
        commandList_->Reset(commandAllocator_.Get(), pipelineState_.Get()),
        "ID3D12GraphicsCommandList::Reset");

    //========================================
    // 描画範囲設定
    //========================================

    /* ウィンドウ全体へ描くので、ビューポートとシザーは画面サイズそのままを使います。 */
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

    //========================================
    // パイプライン設定
    //========================================

    commandList_->SetGraphicsRootSignature(rootSignature_.Get());
    commandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList_->IASetVertexBuffers(0, 1, &vertexBufferView_);

    //========================================
    // 描画開始前の状態遷移
    //========================================

    D3D12_RESOURCE_BARRIER toRender = MakeTransitionBarrier(
        renderTargets_[frameIndex_].Get(),
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET);
    commandList_->ResourceBarrier(1, &toRender);

    //========================================
    // 画面クリア
    //========================================

    /* 今回のバックバッファを取得し、まずは背景色で全面クリアします。 */
    const D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = CurrentRenderTargetView();
    constexpr float clearColor[] = {0.03f, 0.05f, 0.09f, 1.0f};
    commandList_->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
    commandList_->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

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
    const UINT64 signalValue = fenceValue_++;
    ThrowIfFailed(
        commandQueue_->Signal(fence_.Get(), signalValue),
        "ID3D12CommandQueue::Signal");

    /* GPU が追い付いていなければ待つ */
    if (fence_->GetCompletedValue() < signalValue) {
        ThrowIfFailed(
            fence_->SetEventOnCompletion(signalValue, fenceEvent_),
            "ID3D12Fence::SetEventOnCompletion");
        WaitForSingleObject(fenceEvent_, INFINITE);
    }

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
    std::ofstream output(outputPath, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("failed to open CSV output file");
    }

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
        output << record.episode << ','
               << FormatFloat(record.epsilon) << ','
               << FormatFloat(record.averageReward) << ','
               << FormatFloat(record.recentSuccessRate) << ',';


        // greedy 経路がまだゴールへ届いていない段階だけ、人が読める文字列へ置き換えます。
        if (record.bestPathLength >= 0) {
            output << record.bestPathLength;
        } else {
            output << "経路未発見";
        }


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
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(4) << value;
    return stream.str();
}
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
</details>
