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
