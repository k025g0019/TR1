#include "CsvExporter.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

void WriteUtf8Bom(std::ofstream& stream) {
    const unsigned char bom[] = {0xEF, 0xBB, 0xBF};
    stream.write(reinterpret_cast<const char*>(bom), sizeof(bom));
}

std::string FormatFloat(float value) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(4) << value;
    return stream.str();
}

}  // namespace

std::filesystem::path ExportEpisodeHistoryCsv(
    const QLearningGrid& world,
    const std::filesystem::path& outputDirectory) {
    std::filesystem::create_directories(outputDirectory);

    const std::filesystem::path outputPath = outputDirectory / "episode_history.csv";
    std::ofstream output(outputPath, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("failed to open CSV output file");
    }

    WriteUtf8Bom(output);
    output << "episode,epsilon,average_reward,recent_success_rate,best_path_length,steps\n";

    const auto& history = world.GetEpisodeHistory();
    for (const auto& record : history) {
        output << record.episode << ','
               << FormatFloat(record.epsilon) << ','
               << FormatFloat(record.averageReward) << ','
               << FormatFloat(record.recentSuccessRate) << ',';

        if (record.bestPathLength >= 0) {
            output << record.bestPathLength;
        } else {
            output << "not-found";
        }

        output << ',' << record.steps << '\n';
    }

    return outputPath;
}
