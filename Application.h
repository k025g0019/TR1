#pragma once

#define NOMINMAX
#include <windows.h>

#include <chrono>
#include <cstdint>
#include <string>

#include "Dx12Renderer.h"
#include "QLearningGrid.h"

class Application {
public:
    struct SpeedPreset {
        const wchar_t* label;
        int intervalMs;
        int stepsPerTick;
    };

    void Run();

private:
    friend LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    void HandleKeyDown(WPARAM key);
    void HandleCharInput(wchar_t character);
    void HandleLeftButtonDown(int x, int y);
    void BeginEpisodeTargetEdit();
    void ConfirmEpisodeTargetEdit();
    void CancelEpisodeTargetEdit();
    void StopEpisodeTargetRun();

    void ResetStepTimer();
    void AdvanceTraining();
    void CreateMainWindow();

    static constexpr DWORD kWindowStyle =
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    static constexpr int kAutoRunStepsPerFrame = 2048;

    HWND hwnd_ = nullptr;
    Dx12Renderer renderer_;
    QLearningGrid world_;

    std::uint64_t frameCounter_ = 0;
    int speedPresetIndex_ = 1;
    bool paused_ = false;
    std::chrono::steady_clock::time_point nextSimulationTime_{};

    bool editingEpisodeTarget_ = false;
    bool clearEpisodeTargetOnNextDigit_ = false;
    std::wstring episodeTargetInput_ = L"50";
    bool autoRunningToTargetEpisode_ = false;
    int targetEpisode_ = 50;
};
