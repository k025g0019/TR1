#pragma once

#include <filesystem>

#include "QLearningGrid.h"

std::filesystem::path ExportEpisodeHistoryCsv(
    const QLearningGrid& world,
    const std::filesystem::path& outputDirectory);
