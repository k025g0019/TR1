#include "Application.h"

#include <exception>
#include <iostream>

//========================================
// エントリーポイント
//========================================
// コンソール説明を出したあと、Application を起動します。

int main() {
    try {
        //========================================
        // 起動時ガイド表示
        //========================================

        /* 操作ヘルプ */
        // 起動直後に最低限のキー操作が分かるよう、コンソールへ短いガイドを先に出します。
        std::cout
            << "DirectX12 Q-Learning demo\n"
            << "1=slow 2=normal 3=fast 4=max "
            << "Space=pause N=step Enter=target episode "
            << "R=reload map E=export csv Esc=quit\n";

        //========================================
        // 本体実行
        //========================================

        /* ここから先は Win32 と DirectX12 の初期化、メインループ実行を Application へ委ねます。 */
        Application app;
        app.Run();
        return 0;
    } catch (const std::exception& exception) {
        //========================================
        // 例外通知
        //========================================

        /* 先に stderr へ流しておくと、ダイアログを閉じたあとでも原因を追いやすくなります。 */
        std::cerr << exception.what() << '\n';

        /* GUI アプリとして起動した場合でも見落とさないよう、ダイアログでも同じ内容を出します。 */
        MessageBoxA(
            nullptr,
            exception.what(),
            "DirectX12 Q-Learning Error",
            MB_OK | MB_ICONERROR);
        return 1;
    }
}
