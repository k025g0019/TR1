#pragma once

#define NOMINMAX
#include <windows.h>

#include "QLearningGrid.h"
#include "SharedTypes.h"

#include <string>
#include <vector>

struct EpisodeRunUiState {
    std::wstring inputText;
    bool editing = false;
    bool autoRunning = false;
    int targetEpisode = -1;
};

std::vector<Vertex> BuildSceneVertices(
    const QLearningGrid& world,
    unsigned int windowWidth,
    unsigned int windowHeight,
    const std::wstring& speedLabel,
    bool paused,
    const EpisodeRunUiState& episodeRunUiState);

void DrawSceneOverlayText(
    HWND hwnd,
    const QLearningGrid& world,
    unsigned int windowWidth,
    unsigned int windowHeight,
    const std::wstring& speedLabel,
    bool paused,
    const EpisodeRunUiState& episodeRunUiState);

std::wstring BuildWindowTitle(
    const QLearningGrid& world,
    const std::wstring& speedLabel,
    bool paused,
    const EpisodeRunUiState& episodeRunUiState);

RECT GetEpisodeTargetInputRect(unsigned int windowWidth, unsigned int windowHeight);
