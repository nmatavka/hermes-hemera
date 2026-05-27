#include <Application.h>

#include "HaikuShellHost.h"
#include "hermes/ComposeMessage.h"

int main() {
    BApplication app("application/x-vnd.hermes-hemera-smoke");

    hermes::haiku_port::HaikuShellHost shell_host;
    shell_host.ShowMainWindow();

    hermes::ComposeMessage message;
    message.id = "haiku-smoke-compose";
    message.headers.subject = "Haiku compose smoke";
    shell_host.OpenComposer(message);
    shell_host.ReloadWorkspace();
    return 0;
}
