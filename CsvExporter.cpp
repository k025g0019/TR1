#include "CsvExporter.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

//========================================
// ローカル補助関数
//========================================

/* UTF-8 BOM 書き込み */
// Excel などで日本語ヘッダーが崩れにくいよう、先頭へ BOM を付けます。
void WriteUtf8Bom(std::ofstream& stream) {
    const unsigned char bom[] = {0xEF, 0xBB, 0xBF};
    stream.write(reinterpret_cast<const char*>(bom), sizeof(bom));
}

/* 小数点書式統一 */
// 比較しやすいよう、小数の出力桁数を固定します。
std::string FormatFloat(float value) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(4) << value;
    return stream.str();
}

}  // namespace

//========================================
// 公開 CSV 出力
//========================================

std::filesystem::path ExportEpisodeHistoryCsv(
    const QLearningGrid& world,
    const std::filesystem::path& outputDirectory) {
    //========================================
    // 出力先準備
    //========================================

    /* フォルダ作成 */
    std::filesystem::create_directories(outputDirectory);

    /* 保存先パス確定 */
    const std::filesystem::path outputPath = outputDirectory / "episode_history.csv";

    //========================================
    // ファイルオープン
    //========================================

    /* 上書き保存で開く */
    std::ofstream output(outputPath, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("failed to open CSV output file");
    }

    //========================================
    // ヘッダー出力
    //========================================

    /* BOM 付与 */
    WriteUtf8Bom(output);

    /* 列名行 */
    output << "合戦回数,指揮ゆらぎ,平均報酬,味方勝率,残兵差,ターン数\n";

    //========================================
    // 本文出力
    //========================================

    /* 画面で蓄積してきた履歴スナップショットを、そのまま CSV の元データとして使います。 */
    const auto& history = world.GetEpisodeHistory();

    /* 各レコードを「数値列 + 特殊文字列表現」の順で 1 行ずつ組み立てます。 */
    for (const auto& record : history) {

        // まず毎回必ず存在する数値項目を、列順を崩さずに並べます。
        output << record.episode << ','
               << FormatFloat(record.epsilon) << ','
               << FormatFloat(record.averageReward) << ','
               << FormatFloat(record.recentSuccessRate) << ',';

        output << record.bestPathLength;


        // 最後にそのエピソードの手数を付けて 1 行を閉じます。
        output << ',' << record.steps << '\n';
    }

    //========================================
    // 保存先通知
    //========================================

    /* 保存ダイアログを使わないので、書き込んだ実パスを呼び出し元が通知表示に使えるよう渡します。 */
    return outputPath;
}
