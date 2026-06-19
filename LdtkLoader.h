#pragma once

#include <filesystem>
#include <string>
#include <vector>

//========================================
// LDtk 読み込み用データ定義
//========================================
// `.ldtk` から取り出したレベル情報を、学習側が使いやすい形へ整理した構造体群です。

/* エンティティ 1 件分 */
struct LdtkEntityData {
    // 小見出し
    // Start / Goal / Wall など、LDtk 上で付けたエンティティ名です。
    std::string identifier;

    // 小見出し
    // 読み込んだグリッド上でどの列に置かれているかを表します。
    int gridX = 0;

    // 小見出し
    // 読み込んだグリッド上でどの行に置かれているかを表します。
    int gridY = 0;

    // 小見出し
    // LDtk の Count フィールドで指定した部隊人数です。未設定なら -1 です。
    int count = -1;

    // 小見出し
    // この部隊が属する将軍の GeneralId です。-1 なら配下なし（または将軍自身）です。
    int generalId = -1;
};

/* レベル 1 件分 */
struct LdtkLevelData {
    /* UI 表示やデバッグ出力に使うレベル名です。 */
    std::string identifier;
    /* このデモへ読み込めるか確認するための横マス数です。 */
    int gridWidth = 0;
    /* このデモへ読み込めるか確認するための縦マス数です。 */
    int gridHeight = 0;
    /* IntGrid レイヤーの値を、左上からの一次元配列として保持します。 */
    std::vector<int> intGridCsv;
    /* Start / Goal など IntGrid 以外で補うエンティティ群です。 */
    std::vector<LdtkEntityData> entities;
};

/* プロジェクト全体 */
struct LdtkProjectData {
    /* `.ldtk` 内に入っていた全レベルを読み込み順で保持します。 */
    std::vector<LdtkLevelData> levels;
};

//========================================
// 公開読み込み関数
//========================================

/* LDtk プロジェクト読み込み */
// JSON を解析して LDtk 専用情報を抜き出し、利用可能な全レベルを 1 つの構造体へまとめます。
LdtkProjectData LoadLdtkProject(const std::filesystem::path& filePath);
