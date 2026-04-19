#include "Application.h"

#include "CsvExporter.h"
#include "SceneBuilder.h"
#include "SharedTypes.h"

#include <stdexcept>
#include <string>
#include <vector>
#include <windowsx.h>

namespace {

[[noreturn]] void ThrowWithMessage(const std::string& message) {
    throw std::runtime_error(message);
}

constexpr Application::SpeedPreset kSpeedPresets[] = {
    {L"Slow", 260, 1},
    {L"Normal", 120, 1},
    {L"Fast", 45, 1},
    {L"Max", 0, 6},
};

Application* GetApplication(HWND hwnd) {
    return reinterpret_cast<Application*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
}

}  // namespace

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_NCCREATE) {
        const auto* createStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);
        auto* app = static_cast<Application*>(createStruct->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
        return TRUE;
    }

    Application* app = GetApplication(hwnd);

    switch (message) {
    case WM_KEYDOWN:
        if (app != nullptr) {
            app->HandleKeyDown(wParam);
            return 0;
        }
        break;

    case WM_CHAR:
        if (app != nullptr) {
            app->HandleCharInput(static_cast<wchar_t>(wParam));
            return 0;
        }
        break;

    case WM_LBUTTONDOWN:
        if (app != nullptr) {
            app->HandleLeftButtonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;
        }
        break;

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    default:
        break;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

void Application::Run() {
    CreateMainWindow();
    renderer_.Initialize(hwnd_, kWindowWidth, kWindowHeight);
    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);
    ResetStepTimer();

    MSG msg = {};
    while (msg.message != WM_QUIT) {
        if (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            continue;
        }

        AdvanceTraining();

        const EpisodeRunUiState episodeRunUiState{
            episodeTargetInput_,
            editingEpisodeTarget_,
            autoRunningToTargetEpisode_,
            targetEpisode_,
        };

        const std::vector<Vertex> vertices = BuildSceneVertices(
            world_,
            kWindowWidth,
            kWindowHeight,
            kSpeedPresets[speedPresetIndex_].label,
            paused_,
            episodeRunUiState);

        renderer_.UploadVertices(vertices);
        renderer_.Render(static_cast<UINT>(vertices.size()));
        DrawSceneOverlayText(
            hwnd_,
            world_,
            kWindowWidth,
            kWindowHeight,
            kSpeedPresets[speedPresetIndex_].label,
            paused_,
            episodeRunUiState);

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

        ++frameCounter_;
    }
}

void Application::BeginEpisodeTargetEdit() {
    editingEpisodeTarget_ = true;
    clearEpisodeTargetOnNextDigit_ = true;
}

void Application::ConfirmEpisodeTargetEdit() {
    if (episodeTargetInput_.empty()) {
        MessageBoxW(
            hwnd_,
            L"Please enter a target episode.",
            L"Target Episode",
            MB_OK | MB_ICONWARNING);
        return;
    }

    try {
        std::size_t processed = 0;
        const int parsed = std::stoi(episodeTargetInput_, &processed);
        if (processed != episodeTargetInput_.size() || parsed <= 0) {
            throw std::runtime_error("invalid target episode");
        }

        targetEpisode_ = parsed;
        editingEpisodeTarget_ = false;
        clearEpisodeTargetOnNextDigit_ = false;

        if (targetEpisode_ > world_.GetEpisodeCount()) {
            autoRunningToTargetEpisode_ = true;
            paused_ = false;
            ResetStepTimer();
        } else {
            autoRunningToTargetEpisode_ = false;
        }
    } catch (const std::exception&) {
        MessageBoxW(
            hwnd_,
            L"Please enter a positive integer.",
            L"Target Episode",
            MB_OK | MB_ICONWARNING);
    }
}

void Application::CancelEpisodeTargetEdit() {
    editingEpisodeTarget_ = false;
    clearEpisodeTargetOnNextDigit_ = false;
    if (episodeTargetInput_.empty()) {
        episodeTargetInput_ = std::to_wstring(std::max(1, targetEpisode_));
    }
}

void Application::StopEpisodeTargetRun() {
    autoRunningToTargetEpisode_ = false;
}

void Application::HandleKeyDown(WPARAM key) {
    if (editingEpisodeTarget_) {
        switch (key) {
        case VK_RETURN:
            ConfirmEpisodeTargetEdit();
            return;

        case VK_BACK:
            if (clearEpisodeTargetOnNextDigit_) {
                episodeTargetInput_.clear();
                clearEpisodeTargetOnNextDigit_ = false;
            }
            if (!episodeTargetInput_.empty()) {
                episodeTargetInput_.pop_back();
            }
            return;

        case VK_ESCAPE:
            CancelEpisodeTargetEdit();
            return;

        default:
            return;
        }
    }

    switch (key) {
    case VK_ESCAPE:
        DestroyWindow(hwnd_);
        return;

    case '1':
        StopEpisodeTargetRun();
        speedPresetIndex_ = 0;
        paused_ = false;
        ResetStepTimer();
        return;

    case '2':
        StopEpisodeTargetRun();
        speedPresetIndex_ = 1;
        paused_ = false;
        ResetStepTimer();
        return;

    case '3':
        StopEpisodeTargetRun();
        speedPresetIndex_ = 2;
        paused_ = false;
        ResetStepTimer();
        return;

    case '4':
        StopEpisodeTargetRun();
        speedPresetIndex_ = 3;
        paused_ = false;
        ResetStepTimer();
        return;

    case VK_RETURN:
        BeginEpisodeTargetEdit();
        return;

    case VK_SPACE:
        StopEpisodeTargetRun();
        paused_ = !paused_;
        ResetStepTimer();
        return;

    case 'N':
        StopEpisodeTargetRun();
        if (paused_) {
            world_.Train(1);
        }
        return;

    case 'R':
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
    if (!editingEpisodeTarget_) {
        return;
    }

    if (character < L'0' || character > L'9') {
        return;
    }

    if (clearEpisodeTargetOnNextDigit_) {
        episodeTargetInput_.clear();
        clearEpisodeTargetOnNextDigit_ = false;
    }

    if (episodeTargetInput_.size() >= 6) {
        return;
    }

    episodeTargetInput_.push_back(character);
}

void Application::HandleLeftButtonDown(int x, int y) {
    const RECT inputRect = GetEpisodeTargetInputRect(kWindowWidth, kWindowHeight);
    const POINT point{x, y};

    if (PtInRect(&inputRect, point)) {
        BeginEpisodeTargetEdit();
        return;
    }

    if (editingEpisodeTarget_) {
        CancelEpisodeTargetEdit();
    }
}

void Application::ResetStepTimer() {
    nextSimulationTime_ = std::chrono::steady_clock::now();
}

void Application::AdvanceTraining() {
    if (autoRunningToTargetEpisode_) {
        int remainingBudget = kAutoRunStepsPerFrame;
        while (world_.GetEpisodeCount() < targetEpisode_ && remainingBudget > 0) {
            world_.Train(1);
            --remainingBudget;
        }

        if (world_.GetEpisodeCount() >= targetEpisode_) {
            autoRunningToTargetEpisode_ = false;
        }
        return;
    }

    if (paused_) {
        return;
    }

    const SpeedPreset& preset = kSpeedPresets[speedPresetIndex_];
    if (preset.intervalMs <= 0) {
        world_.Train(preset.stepsPerTick);
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    int catchUpCount = 0;
    while (now >= nextSimulationTime_ && catchUpCount < 4) {
        world_.Train(preset.stepsPerTick);
        nextSimulationTime_ += std::chrono::milliseconds(preset.intervalMs);
        ++catchUpCount;
    }
}

void Application::CreateMainWindow() {
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

    RECT rect = {
        0,
        0,
        static_cast<LONG>(kWindowWidth),
        static_cast<LONG>(kWindowHeight),
    };
    AdjustWindowRect(&rect, kWindowStyle, FALSE);

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
