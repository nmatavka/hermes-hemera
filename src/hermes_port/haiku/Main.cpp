#include "HaikuShellHost.h"

int main() {
    hermes::haiku_port::HaikuShellHost shell_host;
    return shell_host.Run();
}
