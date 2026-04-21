#pragma once

#include <filesystem>

#include "QLearningGrid.h"

//========================================
// CSV 出力関数宣言
//========================================
// 学習履歴を外部の表計算ソフトで見やすい形へ整えるための出力関数です。

/* エピソード履歴を書き出す */
// 出力先フォルダを準備し、履歴を 1 行ずつ CSV へ整形したうえで保存先パスを呼び出し元へ渡します。
std::filesystem::path ExportEpisodeHistoryCsv(
    const QLearningGrid& world,
    const std::filesystem::path& outputDirectory);
