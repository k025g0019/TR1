#include "Application.h"

#include <exception>
#include <iostream>

int main() {
    try {
        std::cout
            << "DirectX12 Q-Learning demo\n"
            << "1=slow 2=normal 3=fast 4=max "
            << "Space=pause N=step Enter=target episode "
            << "R=reload map E=export csv Esc=quit\n";

        Application app;
        app.Run();
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        MessageBoxA(
            nullptr,
            exception.what(),
            "DirectX12 Q-Learning Error",
            MB_OK | MB_ICONERROR);
        return 1;
    }
}
