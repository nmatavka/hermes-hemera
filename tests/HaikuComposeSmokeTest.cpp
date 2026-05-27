#include <Application.h>

#include "HaikuShellHost.h"
#include "hermes/ComposeMessage.h"

int main() {
    BApplication app("application/x-vnd.hermes-hemera-smoke");

    hermes::haiku_port::HaikuShellHost shell_host;
    shell_host.Settings().SetString("Settings", "HaikuComposeUtilityPaneHeight", "240");
    shell_host.Settings().SetString("Settings", "HaikuComposeUtilityPaneSelectedTab", "1");
    shell_host.ShowMainWindow();

    hermes::ComposeMessage message;
    message.id = "haiku-smoke-compose";
    message.headers.subject = "Haiku compose smoke";
    message.headers.to = "haiku@example.com";
    message.body.plain_text = "Compose window smoke body";
    shell_host.OpenComposer(message);
    shell_host.ReloadWorkspace();
    return 0;
}
