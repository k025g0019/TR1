#include "SceneBuilder.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

//========================================
// SceneBuilder 実装
//========================================
// このファイルでは、学習世界と UI 情報をすべて頂点列へ変換しています。
// 盤面、右側パネル、アイコン、テキストの見た目を段階的に積み上げる構成です。

namespace {

//==================================
// 内部型
//==================================

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
        layout.panel.top + 430.0f,
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

RectPx MetricBarRect(const SceneLayout& layout, int row) {
    /* ゲージ矩形 */
    // 指標は等間隔に縦へ並べたいので、行番号から上端をずらして同じ形のバーを作ります。
    const float top = layout.metricsCard.top + 138.0f + static_cast<float>(row) * 46.0f;
    return {
        layout.metricsCard.left + 18.0f,
        top + 18.0f,
        layout.metricsCard.right - 18.0f,
        top + 32.0f,
    };
}

RectPx EpisodeTargetInputRectPx(const SceneLayout& layout) {
    /* 入力欄矩形 */
    // 入力欄はメトリクスカードの下端へ寄せ、統計表示と操作欄が自然につながるように置きます。
    return {
        layout.metricsCard.left + 18.0f,
        layout.metricsCard.bottom - 60.0f,
        layout.metricsCard.right - 18.0f,
        layout.metricsCard.bottom - 20.0f,
    };
}

//==================================
// 座標変換
//==================================

float ToClipX(float x, float width) {
    /* クリップ座標変換 X */
    return (x / width) * 2.0f - 1.0f;
}

float ToClipY(float y, float height) {
    return 1.0f - (y / height) * 2.0f;
}

Vertex MakeVertex(float x, float y, float width, float height, const Color& color) {
    /* 頂点生成 */
    // ピクセル座標と色を DirectX 用の頂点形式へ変換します。
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

//==================================
// 基本図形
//==================================

void AppendTriangle(
    std::vector<Vertex>& vertices,
    const PointPx& a,
    const PointPx& b,
    const PointPx& c,
    float width,
    float height,
    const Color& color) {
    // 小見出し
    // 三角形 1 枚をそのまま頂点 3 つとして追加します。
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
    // 小見出し
    // 四角形は 2 枚の三角形へ分けて表現します。
    const PointPx a{rect.left, rect.top};
    const PointPx b{rect.right, rect.top};
    const PointPx c{rect.right, rect.bottom};
    const PointPx d{rect.left, rect.bottom};
    AppendTriangle(vertices, a, c, d, width, height, color);
    AppendTriangle(vertices, a, b, c, width, height, color);
}

void AppendFrame(
    std::vector<Vertex>& vertices,
    const RectPx& rect,
    float thickness,
    float width,
    float height,
    const Color& color) {
    // 小見出し
    // 上下左右の細い四角形を並べて枠線を作ります。
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
    // 小見出し
    // 菱形は上下と左右の 4 点から 2 枚の三角形で作ります。
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
    // 小見出し
    // 縦棒と横棒の 2 つの四角形を重ねて十字形を作ります。
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
    // 小見出し
    // 線分は法線方向へ厚みを持たせた細長い四角形として描きます。
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
    // 小見出し
    // 行動方向ごとに三角形の向きを切り替えて矢印を作ります。
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

//==================================
// マップ補助
//==================================

RectPx CellRect(const SceneLayout& layout, const QLearningGrid& world, int x, int y) {
    /* セル矩形 */
    // グリッド座標を実際の描画矩形へ変換します。
    const float cellWidth =
        (layout.grid.right - layout.grid.left) / static_cast<float>(std::max(1, world.GetGridWidth()));
    const float cellHeight =
        (layout.grid.bottom - layout.grid.top) / static_cast<float>(std::max(1, world.GetGridHeight()));
    return {
        layout.grid.left + cellWidth * static_cast<float>(x),
        layout.grid.top + cellHeight * static_cast<float>(y),
        layout.grid.left + cellWidth * static_cast<float>(x + 1),
        layout.grid.top + cellHeight * static_cast<float>(y + 1),
    };
}

float CellSizePx(const SceneLayout& layout, const QLearningGrid& world) {
    /* セル 1 辺の基準長 */
    // BuildLayout で正方セルになるよう合わせているので、短辺側を基準サイズとして使えます。
    const float cellWidth =
        (layout.grid.right - layout.grid.left) / static_cast<float>(std::max(1, world.GetGridWidth()));
    const float cellHeight =
        (layout.grid.bottom - layout.grid.top) / static_cast<float>(std::max(1, world.GetGridHeight()));
    return std::min(cellWidth, cellHeight);
}

PointPx CellCenter(const SceneLayout& layout, const QLearningGrid& world, int x, int y) {
    /* セル中心 */
    // セル矩形の中央を取り、菱形アイコンや矢印を常にマスの真ん中へそろえます。
    const RectPx rect = CellRect(layout, world, x, y);
    return {(rect.left + rect.right) * 0.5f, (rect.top + rect.bottom) * 0.5f};
}

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

//==================================
// パネル描画
//==================================

void AppendBackground(
    std::vector<Vertex>& vertices,
    const SceneLayout& layout,
    float width,
    float height) {
    // 小見出し
    // 画面全体背景、右パネル背景、右パネル枠の順に重ねます。
    AppendQuad(vertices, {0.0f, 0.0f, width, height}, width, height, {0.03f, 0.05f, 0.09f, 1.0f});
    AppendQuad(vertices, layout.panel, width, height, {0.05f, 0.08f, 0.14f, 0.97f});
    AppendFrame(vertices, layout.panel, 2.0f, width, height, {0.16f, 0.26f, 0.40f, 1.0f});
}

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

    /* 敵描画 */
    // 先に敵群を描いておくと、その上へプレイヤーを重ねたときの視認性が安定します。
    const float enemyGlowHalfSize = cellSize * 0.25f;
    const float enemyOuterRadius = cellSize * 0.21f;
    const float enemyInnerRadius = cellSize * 0.11f;
    for (const GridPoint& enemy : world.GetEnemies()) {
        const PointPx enemyCenter = CellCenter(layout, world, enemy.x, enemy.y);
        AppendQuad(
            vertices,
            {
                enemyCenter.x - enemyGlowHalfSize,
                enemyCenter.y - enemyGlowHalfSize,
                enemyCenter.x + enemyGlowHalfSize,
                enemyCenter.y + enemyGlowHalfSize,
            },
            width,
            height,
            {0.95f, 0.26f, 0.31f, 0.18f});
        AppendCross(
            vertices,
            enemyCenter.x,
            enemyCenter.y,
            enemyOuterRadius,
            std::max(1.5f, cellSize * 0.05f),
            width,
            height,
            {1.0f, 0.88f, 0.90f, 0.92f});
        AppendDiamond(
            vertices,
            enemyCenter.x,
            enemyCenter.y,
            enemyInnerRadius,
            width,
            height,
            {0.78f, 0.08f, 0.12f, 1.0f});
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

void AppendLegendPanel(
    std::vector<Vertex>& vertices,
    const SceneLayout& layout,
    float width,
    float height) {
    /* 凡例カード背景 */
    AppendQuad(vertices, layout.legendCard, width, height, {0.09f, 0.12f, 0.19f, 0.97f});
    AppendFrame(vertices, layout.legendCard, 2.0f, width, height, {0.22f, 0.32f, 0.46f, 1.0f});

    const float left = layout.legendCard.left + 22.0f;
    const float top = layout.legendCard.top + 64.0f;
    const float rowGap = 42.0f;

    // 小見出し
    // 凡例のアイコンは盤面と同じ見た目で揃えています。

    /* スウォッチ枠 */
    for (int row = 0; row < 5; ++row) {
        const RectPx swatch = {
            left,
            top + rowGap * static_cast<float>(row),
            left + 54.0f,
            top + rowGap * static_cast<float>(row) + 34.0f,
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

    AppendDiamond(vertices, left + 27.0f, top + rowGap * 3.0f + 18.0f, 14.0f, width, height, {1.0f, 0.94f, 0.72f, 1.0f});
    AppendDiamond(vertices, left + 27.0f, top + rowGap * 3.0f + 18.0f, 9.0f, width, height, {0.95f, 0.73f, 0.18f, 1.0f});

    AppendCross(vertices, left + 27.0f, top + rowGap * 4.0f + 18.0f, 13.0f, 4.0f, width, height, {1.0f, 0.88f, 0.90f, 0.92f});
    AppendDiamond(vertices, left + 27.0f, top + rowGap * 4.0f + 18.0f, 7.0f, width, height, {0.78f, 0.08f, 0.12f, 1.0f});

    /* 経路サンプル */
    // 下部には最善経路ラインの見本も載せています。
    const RectPx routeBox = {
        layout.legendCard.left + 18.0f,
        layout.legendCard.bottom - 54.0f,
        layout.legendCard.right - 18.0f,
        layout.legendCard.bottom - 18.0f,
    };
    AppendFrame(vertices, routeBox, 1.0f, width, height, {0.22f, 0.30f, 0.41f, 1.0f});

    const PointPx a{routeBox.left + 16.0f, routeBox.bottom - 11.0f};
    const PointPx b{routeBox.left + 82.0f, routeBox.top + 16.0f};
    const PointPx c{routeBox.right - 20.0f, routeBox.top + 15.0f};
    AppendSegment(vertices, a, b, 6.0f, width, height, {0.98f, 0.80f, 0.24f, 0.60f});
    AppendSegment(vertices, b, c, 6.0f, width, height, {0.98f, 0.80f, 0.24f, 0.60f});
    AppendArrow(vertices, b.x, b.y, 9.0f, Action::Right, width, height, {0.03f, 0.05f, 0.09f, 0.95f});
}

//==================================
// 文字描画
//==================================

std::wstring FormatWFloat(float value, int precision) {
    /* 小数文字列化 */
    std::wostringstream stream;
    stream << std::fixed << std::setprecision(precision) << value;
    return stream.str();
}

std::wstring FormatPathText(int greedyPath) {
    /* 経路長文字列化 */
    if (greedyPath >= 0) {
        std::wostringstream stream;
        stream << greedyPath << L" 歩";
        return stream.str();
    }
    return L"まだ未発見";
}

std::wstring FormatEpisodeTargetInput(const EpisodeRunUiState& episodeRunUiState) {
    /* 入力欄表示文字列 */
    std::wstring text = episodeRunUiState.inputText;
    if (episodeRunUiState.editing) {
        text += L"|";
    }
    return text;
}

std::wstring FormatEpisodeTargetStatus(const EpisodeRunUiState& episodeRunUiState) {
    /* 入力状態メッセージ */
    if (episodeRunUiState.editing) {
        return L"数字を入力して Enter で開始";
    }

    if (episodeRunUiState.autoRunning && episodeRunUiState.targetEpisode >= 0) {
        std::wostringstream stream;
        stream << L"エピソード " << episodeRunUiState.targetEpisode << L" まで自動実行中";
        return stream.str();
    }

    if (episodeRunUiState.targetEpisode >= 0) {
        std::wostringstream stream;
        stream << L"クリックして数字入力 / Enter で EP " << episodeRunUiState.targetEpisode;
        return stream.str();
    }

    return L"白い欄をクリックして数字入力";
}

HFONT CreateUiFont(int height, int weight) {
    /* UI フォント作成 */
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
        OUT_OUTLINE_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        L"Yu Gothic UI");
}

void DrawTextLine(
    HDC dc,
    int x,
    int y,
    const std::wstring& text,
    COLORREF color,
    HFONT font) {
    // 小見出し
    // 影付き 2 回描画で文字を見やすくしています。
    const HFONT oldFont = static_cast<HFONT>(SelectObject(dc, font));
    SetTextColor(dc, RGB(10, 14, 22));
    TextOutW(dc, x + 1, y + 1, text.c_str(), static_cast<int>(text.size()));
    SetTextColor(dc, color);
    TextOutW(dc, x, y, text.c_str(), static_cast<int>(text.size()));
    SelectObject(dc, oldFont);
}

void DrawFlatTextLine(
    HDC dc,
    int x,
    int y,
    const std::wstring& text,
    COLORREF color,
    HFONT font) {
    // 小見出し
    // 影なしの単純な文字描画です。
    const HFONT oldFont = static_cast<HFONT>(SelectObject(dc, font));
    SetTextColor(dc, color);
    TextOutW(dc, x, y, text.c_str(), static_cast<int>(text.size()));
    SelectObject(dc, oldFont);
}

void DrawWrappedText(
    HDC dc,
    RECT rect,
    const std::wstring& text,
    COLORREF color,
    HFONT font) {
    // 小見出し
    // 説明文のような複数行テキストを折り返しながら描きます。
    const HFONT oldFont = static_cast<HFONT>(SelectObject(dc, font));
    RECT shadowRect = rect;
    OffsetRect(&shadowRect, 1, 1);
    SetTextColor(dc, RGB(10, 14, 22));
    DrawTextW(dc, text.c_str(), static_cast<int>(text.size()), &shadowRect, DT_WORDBREAK | DT_LEFT);
    SetTextColor(dc, color);
    DrawTextW(dc, text.c_str(), static_cast<int>(text.size()), &rect, DT_WORDBREAK | DT_LEFT);
    SelectObject(dc, oldFont);
}

void AppendTextBitmapGeometry(
    std::vector<Vertex>& vertices,
    const SceneLayout& layout,
    const QLearningGrid& world,
    const std::wstring& speedLabel,
    bool paused,
    const EpisodeRunUiState& episodeRunUiState,
    float width,
    float height) {
    /* テキスト用ビットマップ準備 */
    // GDI で文字を描いたあと、色付きピクセルだけを頂点化して重ねます。
    const int bitmapWidth = static_cast<int>(layout.panel.right - layout.panel.left);
    const int bitmapHeight = static_cast<int>(layout.panel.bottom - layout.panel.top);

    BITMAPINFO bitmapInfo = {};
    bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmapInfo.bmiHeader.biWidth = bitmapWidth;
    bitmapInfo.bmiHeader.biHeight = -bitmapHeight;
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;

    // 小見出し
    // まずメモリ上の 32bit ビットマップへ文字を描く準備をします。
    void* rawPixels = nullptr;
    HDC memoryDc = CreateCompatibleDC(nullptr);
    HBITMAP bitmap = CreateDIBSection(memoryDc, &bitmapInfo, DIB_RGB_COLORS, &rawPixels, nullptr, 0);
    HGDIOBJ oldBitmap = SelectObject(memoryDc, bitmap);

    RECT fillRect = {0, 0, bitmapWidth, bitmapHeight};
    FillRect(memoryDc, &fillRect, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
    SetBkMode(memoryDc, TRANSPARENT);

    /* フォント作成 */
    // タイトル、本文、小さめ説明の 3 種類を用意します。
    HFONT titleFont = CreateUiFont(26, FW_BOLD);
    HFONT bodyFont = CreateUiFont(18, FW_NORMAL);
    HFONT smallFont = CreateUiFont(16, FW_NORMAL);

    const float panelLeft = layout.panel.left;
    const float panelTop = layout.panel.top;
    auto localX = [panelLeft](float absoluteX) {
        return static_cast<int>(absoluteX - panelLeft);
    };
    auto localY = [panelTop](float absoluteY) {
        return static_cast<int>(absoluteY - panelTop);
    };

    // 小見出し
    // ここから右側パネルの文字列を順番に描画していきます。

    /* 表示文言の整形 */
    // 入力欄のプレースホルダーや状態文字列をここで決めます。
    const RectPx inputRect = EpisodeTargetInputRectPx(layout);
    const bool usePlaceholder = episodeRunUiState.inputText.empty() && !episodeRunUiState.editing;
    const std::wstring inputText = usePlaceholder ? L"50" : FormatEpisodeTargetInput(episodeRunUiState);

    DrawTextLine(memoryDc, localX(layout.metricsCard.left + 18.0f), localY(layout.metricsCard.top + 18.0f), L"学習状況", RGB(245, 250, 255), titleFont);
    DrawTextLine(memoryDc, localX(layout.metricsCard.left + 18.0f), localY(layout.metricsCard.top + 52.0f), L"エピソード : " + std::to_wstring(world.GetEpisodeCount()), RGB(255, 247, 210), bodyFont);
    DrawTextLine(memoryDc, localX(layout.metricsCard.left + 18.0f), localY(layout.metricsCard.top + 80.0f), L"総学習ステップ : " + std::to_wstring(world.GetTrainingStepCount()), RGB(255, 247, 210), bodyFont);
    DrawTextLine(memoryDc, localX(layout.metricsCard.left + 18.0f), localY(layout.metricsCard.top + 108.0f), L"現在速度 : " + std::wstring(paused ? L"一時停止" : speedLabel), RGB(255, 247, 210), bodyFont);

    const std::array<std::wstring, 4> labels = {
        L"ランダム行動率",
        L"平均報酬",
        L"直近成功率",
        L"最良経路",
    };
    const std::array<std::wstring, 4> values = {
        FormatWFloat(world.GetEpsilon(), 2),
        FormatWFloat(world.GetAverageReward(), 2),
        FormatWFloat(world.GetRecentSuccessRate(), 2),
        FormatPathText(world.MeasureGreedyPathLength()),
    };

    // 小見出し
    // 4 本のメトリクス名と値を縦に並べます。
    for (int row = 0; row < 4; ++row) {
        const float baseY = layout.metricsCard.top + 138.0f + static_cast<float>(row) * 46.0f;
        DrawTextLine(
            memoryDc,
            localX(layout.metricsCard.left + 18.0f),
            localY(baseY),
            labels[row] + L" : " + values[row],
            RGB(255, 247, 210),
            bodyFont);
    }

    DrawTextLine(memoryDc, localX(layout.metricsCard.left + 18.0f), localY(layout.metricsCard.bottom - 88.0f), L"目標エピソード", RGB(255, 247, 210), bodyFont);
    DrawFlatTextLine(
        memoryDc,
        localX(inputRect.left + 14.0f),
        localY(inputRect.top + 7.0f),
        inputText,
        usePlaceholder ? RGB(120, 126, 138) : RGB(28, 31, 38),
        bodyFont);
    DrawTextLine(memoryDc, localX(layout.metricsCard.left + 18.0f), localY(layout.metricsCard.bottom - 16.0f), FormatEpisodeTargetStatus(episodeRunUiState), RGB(215, 229, 246), smallFont);

    DrawTextLine(memoryDc, localX(layout.legendCard.left + 18.0f), localY(layout.legendCard.top + 18.0f), L"見方と操作", RGB(245, 250, 255), titleFont);

    const std::array<std::wstring, 5> legendLabels = {
        L"開始地点",
        L"ゴール",
        L"落とし穴",
        L"プレイヤー",
        L"敵 AI",
    };
    const float legendTextX = layout.legendCard.left + 94.0f;
    const float legendTop = layout.legendCard.top + 74.0f;
    // 小見出し
    // 凡例ラベルも縦方向に等間隔で並べます。
    for (int row = 0; row < 5; ++row) {
        DrawTextLine(
            memoryDc,
            localX(legendTextX),
            localY(legendTop + static_cast<float>(row) * 42.0f),
            legendLabels[row],
            RGB(220, 232, 248),
            bodyFont);
    }

    DrawTextLine(
        memoryDc,
        localX(layout.legendCard.left + 18.0f),
        localY(layout.legendCard.bottom - 104.0f),
        L"黄色線 : 現在の最良経路 / 赤印 : 敵 AI",
        RGB(215, 229, 246),
        smallFont);
    DrawTextLine(
        memoryDc,
        localX(layout.legendCard.left + 18.0f),
        localY(layout.legendCard.bottom - 80.0f),
        L"操作 : 1-4 速度 / Space 停止 / N 1歩 / Enter 目標 / Esc 終了",
        RGB(215, 229, 246),
        smallFont);

    /* 描いた文字の頂点化 */
    // 非黒色ピクセルだけを横方向ラン単位でまとめ、四角形として積みます。
    const auto* pixels = static_cast<const std::uint32_t*>(rawPixels);
    for (int y = 0; y < bitmapHeight; ++y) {
        int x = 0;
        while (x < bitmapWidth) {
            const std::uint32_t pixel = pixels[y * bitmapWidth + x];
            if ((pixel & 0x00FFFFFFu) == 0) {
                ++x;
                continue;
            }

            const std::uint32_t runColor = pixel & 0x00FFFFFFu;
            const int startX = x;
            // 小見出し
            // 同色が続く横方向ランを 1 本の細長い四角形へまとめます。
            while (x < bitmapWidth &&
                   (pixels[y * bitmapWidth + x] & 0x00FFFFFFu) == runColor) {
                ++x;
            }

            const float left = layout.panel.left + static_cast<float>(startX);
            const float top = layout.panel.top + static_cast<float>(y);
            const float right = layout.panel.left + static_cast<float>(x);
            const float bottom = top + 1.0f;

            const float red = static_cast<float>((runColor >> 16) & 0xFF) / 255.0f;
            const float green = static_cast<float>((runColor >> 8) & 0xFF) / 255.0f;
            const float blue = static_cast<float>(runColor & 0xFF) / 255.0f;
            AppendQuad(vertices, {left, top, right, bottom}, width, height, {red, green, blue, 1.0f});
        }
    }

    /* GDI リソース解放 */
    SelectObject(memoryDc, oldBitmap);
    DeleteObject(bitmap);
    DeleteObject(titleFont);
    DeleteObject(bodyFont);
    DeleteObject(smallFont);
    DeleteDC(memoryDc);
}

}  // namespace

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
           << L" | 成功率 " << std::setprecision(2) << world.GetRecentSuccessRate()
           << L" | 阻止率 " << std::setprecision(2) << world.GetRecentBlockedRate();

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
