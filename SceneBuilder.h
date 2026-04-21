#pragma once

#define NOMINMAX
#include <windows.h>

#include "QLearningGrid.h"
#include "SharedTypes.h"

#include <string>
#include <vector>

//========================================
// 描画用 UI 状態
//========================================
// Application 側が持つ「目標エピソード入力」の見た目用情報をまとめた構造体です。

struct EpisodeRunUiState {
    /* 入力欄の文字列 */
    std::wstring inputText;

    /* 編集中フラグ */
    bool editing = false;

    /* 自動実行フラグ */
    bool autoRunning = false;

    /* 目標エピソード値 */
    int targetEpisode = -1;
};

//========================================
// シーン構築関数
//========================================

/* 頂点列の構築 */
// 盤面と右パネルの見た目を 1 フレームぶん組み立て、DirectX へ渡す頂点列を作ります。
std::vector<Vertex> BuildSceneVertices(
    const QLearningGrid& world,
    unsigned int windowWidth,
    unsigned int windowHeight,
    const std::wstring& speedLabel,
    bool paused,
    const EpisodeRunUiState& episodeRunUiState);

/* GDI テキスト描画 */
// DirectX 側で描かない文字情報を、ウィンドウへ重ねて表示します。
void DrawSceneOverlayText(
    HWND hwnd,
    const QLearningGrid& world,
    unsigned int windowWidth,
    unsigned int windowHeight,
    const std::wstring& speedLabel,
    bool paused,
    const EpisodeRunUiState& episodeRunUiState);

/* タイトル文字列生成 */
// ウィンドウタイトルバーへ載せる要約文を組み立てます。
std::wstring BuildWindowTitle(
    const QLearningGrid& world,
    const std::wstring& speedLabel,
    bool paused,
    const EpisodeRunUiState& episodeRunUiState);

/* 入力欄矩形取得 */
// 現在のマップサイズに合わせて入力欄レイアウトを再計算し、クリック判定用の矩形を作ります。
RECT GetEpisodeTargetInputRect(
    const QLearningGrid& world,
    unsigned int windowWidth,
    unsigned int windowHeight);
