#include "SceneBuilder.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct PointPx {
    float x = 0.0f;
    float y = 0.0f;
};

struct RectPx {
    float left = 0.0f;
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;
};

struct SceneLayout {
    RectPx grid;
    RectPx panel;
    RectPx metricsCard;
    RectPx legendCard;
};

SceneLayout BuildLayout(float width, float height) {
    constexpr float margin = 34.0f;
    constexpr float gap = 26.0f;
    constexpr float panelWidth = 380.0f;

    const float gridSize = std::max(
        240.0f,
        std::min(
            height - margin * 2.0f,
            width - margin * 2.0f - gap - panelWidth));
    const float gridTop = (height - gridSize) * 0.5f;
    const float gridLeft = margin;

    SceneLayout layout = {};
    layout.grid = {gridLeft, gridTop, gridLeft + gridSize, gridTop + gridSize};
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
    return layout;
}

RectPx MetricBarRect(const SceneLayout& layout, int row) {
    const float top = layout.metricsCard.top + 156.0f + static_cast<float>(row) * 60.0f;
    return {
        layout.metricsCard.left + 18.0f,
        top + 22.0f,
        layout.metricsCard.right - 18.0f,
        top + 38.0f,
    };
}

RectPx EpisodeTargetInputRectPx(const SceneLayout& layout) {
    return {
        layout.metricsCard.left + 18.0f,
        layout.metricsCard.bottom - 72.0f,
        layout.metricsCard.right - 18.0f,
        layout.metricsCard.bottom - 28.0f,
    };
}

RectPx CellRect(const SceneLayout& layout, int x, int y) {
    const float cellWidth =
        (layout.grid.right - layout.grid.left) / static_cast<float>(kGridWidth);
    const float cellHeight =
        (layout.grid.bottom - layout.grid.top) / static_cast<float>(kGridHeight);

    return {
        layout.grid.left + cellWidth * static_cast<float>(x),
        layout.grid.top + cellHeight * static_cast<float>(y),
        layout.grid.left + cellWidth * static_cast<float>(x + 1),
        layout.grid.top + cellHeight * static_cast<float>(y + 1),
    };
}

PointPx CellCenter(const SceneLayout& layout, int x, int y) {
    const RectPx rect = CellRect(layout, x, y);
    return {
        (rect.left + rect.right) * 0.5f,
        (rect.top + rect.bottom) * 0.5f,
    };
}

float ToClipX(float x, float width) {
    return (x / width) * 2.0f - 1.0f;
}

float ToClipY(float y, float height) {
    return 1.0f - (y / height) * 2.0f;
}

Vertex MakeVertex(float x, float y, float width, float height, const Color& color) {
    return {
        ToClipX(x, width),
        ToClipY(y, height),
        0.0f,
        color.r,
        color.g,
        color.b,
        color.a,
    };
}

void AppendTriangle(
    std::vector<Vertex>& vertices,
    const PointPx& a,
    const PointPx& b,
    const PointPx& c,
    float width,
    float height,
    const Color& color) {
    vertices.push_back(MakeVertex(a.x, a.y, width, height, color));
    vertices.push_back(MakeVertex(b.x, b.y, width, height, color));
    vertices.push_back(MakeVertex(c.x, c.y, width, height, color));
}

void AppendQuad(
    std::vector<Vertex>& vertices,
    const RectPx& rect,
    float width,
    float height,
    const Color& color) {
    const PointPx a{rect.left, rect.top};
    const PointPx b{rect.right, rect.top};
    const PointPx c{rect.right, rect.bottom};
    const PointPx d{rect.left, rect.bottom};
    AppendTriangle(vertices, a, b, c, width, height, color);
    AppendTriangle(vertices, a, c, d, width, height, color);
}

void AppendFrame(
    std::vector<Vertex>& vertices,
    const RectPx& rect,
    float thickness,
    float width,
    float height,
    const Color& color) {
    AppendQuad(vertices, {rect.left, rect.top, rect.right, rect.top + thickness}, width, height, color);
    AppendQuad(vertices, {rect.left, rect.bottom - thickness, rect.right, rect.bottom}, width, height, color);
    AppendQuad(vertices, {rect.left, rect.top, rect.left + thickness, rect.bottom}, width, height, color);
    AppendQuad(vertices, {rect.right - thickness, rect.top, rect.right, rect.bottom}, width, height, color);
}

void AppendDiamond(
    std::vector<Vertex>& vertices,
    float centerX,
    float centerY,
    float radius,
    float width,
    float height,
    const Color& color) {
    const PointPx top{centerX, centerY - radius};
    const PointPx right{centerX + radius, centerY};
    const PointPx bottom{centerX, centerY + radius};
    const PointPx left{centerX - radius, centerY};
    AppendTriangle(vertices, top, right, bottom, width, height, color);
    AppendTriangle(vertices, top, bottom, left, width, height, color);
}

void AppendCross(
    std::vector<Vertex>& vertices,
    float centerX,
    float centerY,
    float radius,
    float thickness,
    float width,
    float height,
    const Color& color) {
    AppendQuad(
        vertices,
        {centerX - thickness, centerY - radius, centerX + thickness, centerY + radius},
        width,
        height,
        color);
    AppendQuad(
        vertices,
        {centerX - radius, centerY - thickness, centerX + radius, centerY + thickness},
        width,
        height,
        color);
}

void AppendSegment(
    std::vector<Vertex>& vertices,
    const PointPx& from,
    const PointPx& to,
    float thickness,
    float width,
    float height,
    const Color& color) {
    const float dx = to.x - from.x;
    const float dy = to.y - from.y;
    const float length = std::sqrt(dx * dx + dy * dy);
    if (length <= 0.0001f) {
        return;
    }

    const float nx = -dy / length;
    const float ny = dx / length;
    const float ox = nx * thickness * 0.5f;
    const float oy = ny * thickness * 0.5f;

    const PointPx a{from.x - ox, from.y - oy};
    const PointPx b{from.x + ox, from.y + oy};
    const PointPx c{to.x + ox, to.y + oy};
    const PointPx d{to.x - ox, to.y - oy};
    AppendTriangle(vertices, a, b, c, width, height, color);
    AppendTriangle(vertices, a, c, d, width, height, color);
}

void AppendArrow(
    std::vector<Vertex>& vertices,
    float centerX,
    float centerY,
    float size,
    Action action,
    float width,
    float height,
    const Color& color) {
    switch (action) {
    case Action::Up:
        AppendTriangle(
            vertices,
            {centerX, centerY - size},
            {centerX - size * 0.65f, centerY + size * 0.45f},
            {centerX + size * 0.65f, centerY + size * 0.45f},
            width,
            height,
            color);
        break;

    case Action::Right:
        AppendTriangle(
            vertices,
            {centerX + size, centerY},
            {centerX - size * 0.45f, centerY - size * 0.65f},
            {centerX - size * 0.45f, centerY + size * 0.65f},
            width,
            height,
            color);
        break;

    case Action::Down:
        AppendTriangle(
            vertices,
            {centerX, centerY + size},
            {centerX - size * 0.65f, centerY - size * 0.45f},
            {centerX + size * 0.65f, centerY - size * 0.45f},
            width,
            height,
            color);
        break;

    case Action::Left:
        AppendTriangle(
            vertices,
            {centerX - size, centerY},
            {centerX + size * 0.45f, centerY - size * 0.65f},
            {centerX + size * 0.45f, centerY + size * 0.65f},
            width,
            height,
            color);
        break;
    }
}

Color TileColor(Tile tile) {
    switch (tile) {
    case Tile::Empty:
        return {0.11f, 0.15f, 0.24f, 1.0f};
    case Tile::Start:
        return {0.12f, 0.26f, 0.33f, 1.0f};
    case Tile::Goal:
        return {0.15f, 0.31f, 0.19f, 1.0f};
    case Tile::Wall:
        return {0.26f, 0.28f, 0.32f, 1.0f};
    case Tile::Pit:
        return {0.35f, 0.12f, 0.13f, 1.0f};
    }
    return {0.1f, 0.1f, 0.1f, 1.0f};
}

Color ArrowColor(float value) {
    if (value >= 0.0f) {
        return LerpColor(
            {0.52f, 0.68f, 0.98f, 0.45f},
            {0.95f, 0.95f, 1.0f, 0.9f},
            NormalizeRange(value, 0.0f, 10.0f));
    }

    return LerpColor(
        {0.98f, 0.52f, 0.52f, 0.45f},
        {1.0f, 0.88f, 0.88f, 0.85f},
        NormalizeRange(-value, 0.0f, 8.0f));
}

std::wstring FormatFloat(float value, int precision = 2) {
    std::wostringstream stream;
    stream << std::fixed << std::setprecision(precision) << value;
    return stream.str();
}

COLORREF ToColorRef(const Color& color) {
    const auto toByte = [](float component) -> BYTE {
        return static_cast<BYTE>(std::clamp(component, 0.0f, 1.0f) * 255.0f);
    };
    return RGB(toByte(color.r), toByte(color.g), toByte(color.b));
}

HFONT CreateUiFont(int height, int weight) {
    return CreateFontW(
        -height,
        0,
        0,
        0,
        weight,
        FALSE,
        FALSE,
        FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        L"Segoe UI");
}

void DrawTextLine(
    HDC dc,
    int x,
    int y,
    COLORREF color,
    HFONT font,
    const std::wstring& text) {
    const HFONT oldFont = static_cast<HFONT>(SelectObject(dc, font));
    SetTextColor(dc, color);
    TextOutW(dc, x, y, text.c_str(), static_cast<int>(text.size()));
    SelectObject(dc, oldFont);
}

}  // namespace

std::vector<Vertex> BuildSceneVertices(
    const QLearningGrid& world,
    unsigned int windowWidth,
    unsigned int windowHeight,
    const std::wstring& speedLabel,
    bool paused,
    const EpisodeRunUiState& episodeRunUiState) {
    (void)speedLabel;
    (void)paused;
    (void)episodeRunUiState;

    const float width = static_cast<float>(windowWidth);
    const float height = static_cast<float>(windowHeight);
    const SceneLayout layout = BuildLayout(width, height);

    std::vector<Vertex> vertices;
    vertices.reserve(12000);

    AppendQuad(vertices, {0.0f, 0.0f, width, height}, width, height, {0.03f, 0.05f, 0.09f, 1.0f});
    AppendQuad(vertices, layout.panel, width, height, {0.06f, 0.08f, 0.13f, 0.98f});
    AppendQuad(vertices, layout.metricsCard, width, height, {0.09f, 0.12f, 0.19f, 0.97f});
    AppendQuad(vertices, layout.legendCard, width, height, {0.09f, 0.12f, 0.19f, 0.97f});
    AppendFrame(vertices, layout.metricsCard, 2.0f, width, height, {0.22f, 0.32f, 0.46f, 1.0f});
    AppendFrame(vertices, layout.legendCard, 2.0f, width, height, {0.22f, 0.32f, 0.46f, 1.0f});

    AppendQuad(vertices, layout.grid, width, height, {0.05f, 0.07f, 0.11f, 1.0f});
    AppendFrame(vertices, layout.grid, 3.0f, width, height, {0.30f, 0.36f, 0.48f, 1.0f});

    const float epsilonNormalized = Clamp01(1.0f - world.GetEpsilon());
    const float successNormalized = Clamp01(world.GetRecentSuccessRate());
    const float rewardNormalized = NormalizeRange(world.GetAverageReward(), -8.0f, 10.0f);
    const float pathNormalized = NormalizeRange(
        static_cast<float>(std::max(world.MeasureGreedyPathLength(), 0)),
        0.0f,
        30.0f);
    const std::array<float, 4> metricValues = {
        epsilonNormalized,
        successNormalized,
        rewardNormalized,
        pathNormalized,
    };
    const std::array<Color, 4> metricColors = {
        Color{0.49f, 0.74f, 0.99f, 0.85f},
        Color{0.43f, 0.88f, 0.62f, 0.85f},
        Color{0.99f, 0.77f, 0.42f, 0.85f},
        Color{0.96f, 0.56f, 0.60f, 0.85f},
    };
    for (int row = 0; row < 4; ++row) {
        const RectPx bar = MetricBarRect(layout, row);
        AppendQuad(vertices, bar, width, height, {0.12f, 0.16f, 0.24f, 0.95f});
        RectPx fill = bar;
        fill.right = fill.left + (bar.right - bar.left) * metricValues[static_cast<std::size_t>(row)];
        AppendQuad(vertices, fill, width, height, metricColors[static_cast<std::size_t>(row)]);
        AppendFrame(vertices, bar, 1.0f, width, height, {0.24f, 0.32f, 0.46f, 1.0f});
    }

    for (int y = 0; y < kGridHeight; ++y) {
        for (int x = 0; x < kGridWidth; ++x) {
            const RectPx rect = CellRect(layout, x, y);
            const Tile tile = world.GetTile(x, y);
            const PointPx center = CellCenter(layout, x, y);
            const float radius = (rect.right - rect.left) * 0.18f;

            AppendQuad(vertices, rect, width, height, TileColor(tile));
            AppendFrame(vertices, rect, 1.0f, width, height, {0.18f, 0.22f, 0.30f, 1.0f});

            if (tile == Tile::Start) {
                AppendDiamond(vertices, center.x, center.y, radius, width, height, {0.63f, 0.95f, 1.0f, 0.95f});
            } else if (tile == Tile::Goal) {
                AppendDiamond(vertices, center.x, center.y, radius, width, height, {0.58f, 1.0f, 0.62f, 0.95f});
            } else if (tile == Tile::Pit) {
                AppendCross(vertices, center.x, center.y, radius, radius * 0.25f, width, height, {1.0f, 0.85f, 0.85f, 0.95f});
            }

            if (tile != Tile::Wall && !world.IsTerminal(x, y)) {
                AppendArrow(
                    vertices,
                    center.x,
                    center.y,
                    radius * 0.9f,
                    world.GetBestAction(x, y),
                    width,
                    height,
                    ArrowColor(world.GetBestValue(x, y)));
            }
        }
    }

    const std::vector<GridPoint> path = world.BuildGreedyPath();
    for (size_t index = 1; index < path.size(); ++index) {
        AppendSegment(
            vertices,
            CellCenter(layout, path[index - 1].x, path[index - 1].y),
            CellCenter(layout, path[index].x, path[index].y),
            6.0f,
            width,
            height,
            {0.95f, 0.96f, 1.0f, 0.35f});
    }

    const GridPoint agent = world.GetAgent();
    const PointPx agentCenter = CellCenter(layout, agent.x, agent.y);
    AppendDiamond(vertices, agentCenter.x, agentCenter.y, 14.0f, width, height, {1.0f, 0.95f, 0.58f, 0.98f});
    AppendFrame(vertices, layout.grid, 2.0f, width, height, {0.40f, 0.46f, 0.58f, 1.0f});

    const RectPx inputRect = EpisodeTargetInputRectPx(layout);
    AppendQuad(vertices, inputRect, width, height, {0.05f, 0.07f, 0.12f, 1.0f});
    AppendFrame(
        vertices,
        inputRect,
        2.0f,
        width,
        height,
        episodeRunUiState.editing
            ? Color{0.98f, 0.86f, 0.48f, 1.0f}
            : Color{0.25f, 0.34f, 0.48f, 1.0f});

    return vertices;
}

void DrawSceneOverlayText(
    HWND hwnd,
    const QLearningGrid& world,
    unsigned int windowWidth,
    unsigned int windowHeight,
    const std::wstring& speedLabel,
    bool paused,
    const EpisodeRunUiState& episodeRunUiState) {
    const float width = static_cast<float>(windowWidth);
    const float height = static_cast<float>(windowHeight);
    const SceneLayout layout = BuildLayout(width, height);

    HDC dc = GetDC(hwnd);
    if (dc == nullptr) {
        return;
    }

    SetBkMode(dc, TRANSPARENT);

    HFONT titleFont = CreateUiFont(26, FW_BOLD);
    HFONT bodyFont = CreateUiFont(19, FW_NORMAL);
    HFONT monoFont = CreateUiFont(19, FW_MEDIUM);

    const int left = static_cast<int>(layout.metricsCard.left + 18.0f);
    int y = static_cast<int>(layout.metricsCard.top + 20.0f);

    DrawTextLine(dc, left, y, RGB(244, 247, 255), titleFont, L"Q-Learning Grid");
    y += 38;
    DrawTextLine(dc, left, y, RGB(170, 185, 210), bodyFont, world.GetMapDisplayName());
    y += 40;

    const std::wstring stateText =
        episodeRunUiState.autoRunning ? L"Auto run"
                                      : (paused ? L"Paused" : L"Running");
    DrawTextLine(dc, left, y, RGB(234, 238, 248), bodyFont, L"State: " + stateText);
    y += 28;
    DrawTextLine(dc, left, y, RGB(234, 238, 248), bodyFont, L"Speed: " + speedLabel);
    y += 36;

    DrawTextLine(
        dc,
        left,
        y,
        RGB(230, 236, 246),
        bodyFont,
        L"Episode: " + std::to_wstring(world.GetEpisodeCount()));
    y += 28;
    DrawTextLine(
        dc,
        left,
        y,
        RGB(230, 236, 246),
        bodyFont,
        L"Steps: " + std::to_wstring(world.GetTrainingStepCount()));
    y += 28;
    DrawTextLine(
        dc,
        left,
        y,
        RGB(230, 236, 246),
        bodyFont,
        L"Epsilon: " + FormatFloat(world.GetEpsilon(), 3));
    y += 28;
    DrawTextLine(
        dc,
        left,
        y,
        RGB(230, 236, 246),
        bodyFont,
        L"Average reward: " + FormatFloat(world.GetAverageReward(), 2));
    y += 28;
    DrawTextLine(
        dc,
        left,
        y,
        RGB(230, 236, 246),
        bodyFont,
        L"Success rate: " + FormatFloat(world.GetRecentSuccessRate() * 100.0f, 1) + L"%");
    y += 28;

    const int greedyPath = world.MeasureGreedyPathLength();
    DrawTextLine(
        dc,
        left,
        y,
        RGB(230, 236, 246),
        bodyFont,
        greedyPath >= 0
            ? L"Greedy path: " + std::to_wstring(greedyPath)
            : L"Greedy path: not found");

    const RectPx inputRect = EpisodeTargetInputRectPx(layout);
    DrawTextLine(
        dc,
        left,
        static_cast<int>(inputRect.top - 26.0f),
        RGB(206, 219, 239),
        bodyFont,
        L"Target episode");
    DrawTextLine(
        dc,
        static_cast<int>(inputRect.left + 12.0f),
        static_cast<int>(inputRect.top + 10.0f),
        episodeRunUiState.editing ? RGB(255, 228, 140) : RGB(244, 247, 255),
        monoFont,
        episodeRunUiState.inputText.empty() ? L"(empty)" : episodeRunUiState.inputText);

    const int legendLeft = static_cast<int>(layout.legendCard.left + 18.0f);
    int legendY = static_cast<int>(layout.legendCard.top + 22.0f);
    DrawTextLine(dc, legendLeft, legendY, RGB(244, 247, 255), titleFont, L"Controls");
    legendY += 40;
    DrawTextLine(dc, legendLeft, legendY, RGB(214, 223, 239), bodyFont, L"1-4  Change speed");
    legendY += 28;
    DrawTextLine(dc, legendLeft, legendY, RGB(214, 223, 239), bodyFont, L"Space  Pause / resume");
    legendY += 28;
    DrawTextLine(dc, legendLeft, legendY, RGB(214, 223, 239), bodyFont, L"N  Single step while paused");
    legendY += 28;
    DrawTextLine(dc, legendLeft, legendY, RGB(214, 223, 239), bodyFont, L"Enter  Edit target episode");
    legendY += 28;
    DrawTextLine(dc, legendLeft, legendY, RGB(214, 223, 239), bodyFont, L"R  Reload LDtk map");
    legendY += 28;
    DrawTextLine(dc, legendLeft, legendY, RGB(214, 223, 239), bodyFont, L"E  Export CSV");
    legendY += 28;
    DrawTextLine(dc, legendLeft, legendY, RGB(214, 223, 239), bodyFont, L"Esc  Quit");

    DeleteObject(titleFont);
    DeleteObject(bodyFont);
    DeleteObject(monoFont);
    ReleaseDC(hwnd, dc);
}

std::wstring BuildWindowTitle(
    const QLearningGrid& world,
    const std::wstring& speedLabel,
    bool paused,
    const EpisodeRunUiState& episodeRunUiState) {
    std::wostringstream stream;
    stream << L"DirectX12 Q-Learning"
           << L" | Episode " << world.GetEpisodeCount()
           << L" | Speed " << speedLabel
           << L" | " << (episodeRunUiState.autoRunning ? L"Auto" : (paused ? L"Paused" : L"Running"))
           << L" | Map " << world.GetMapDisplayName();
    return stream.str();
}

RECT GetEpisodeTargetInputRect(unsigned int windowWidth, unsigned int windowHeight) {
    const SceneLayout layout =
        BuildLayout(static_cast<float>(windowWidth), static_cast<float>(windowHeight));
    const RectPx inputRect = EpisodeTargetInputRectPx(layout);
    return RECT{
        static_cast<LONG>(inputRect.left),
        static_cast<LONG>(inputRect.top),
        static_cast<LONG>(inputRect.right),
        static_cast<LONG>(inputRect.bottom),
    };
}
