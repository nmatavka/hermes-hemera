#pragma once

#include "hermes/TlsProvider.h"

namespace hermes {

class OpenSslTlsProvider final : public TlsProvider {
public:
    bool IsAvailable() const override;
    std::string ProviderName() const override;
    std::string Version() const override;
    TlsCapabilities Capabilities() const override;
    std::unique_ptr<TlsClientSession> CreateClientSession(
        std::string* error_message = nullptr) const override;
};

}  // namespace hermes
