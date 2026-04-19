#pragma once

#include <filesystem>
#include <string>
#include <vector>

struct LdtkEntityData {
    std::string identifier;
    int gridX = 0;
    int gridY = 0;
};

struct LdtkLevelData {
    std::string identifier;
    int gridWidth = 0;
    int gridHeight = 0;
    std::vector<int> intGridCsv;
    std::vector<LdtkEntityData> entities;
};

struct LdtkProjectData {
    std::vector<LdtkLevelData> levels;
};

LdtkProjectData LoadLdtkProject(const std::filesystem::path& filePath);
