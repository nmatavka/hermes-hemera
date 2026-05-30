#include "HaikuShellHost.h"

int main() {
    hemera::haiku::HaikuShellHost shell_host;
    return shell_host.Run();
}
