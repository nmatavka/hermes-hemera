#pragma once

#include <string>

namespace hermes {

struct TlsCapabilities {
    bool tls_1_2 = false;
    bool tls_1_3 = false;
    bool system_trust_store = false;
};

class TlsProvider {
public:
    virtual ~TlsProvider() = default;

    virtual bool IsAvailable() const = 0;
    virtual std::string ProviderName() const = 0;
    virtual std::string Version() const = 0;
    virtual TlsCapabilities Capabilities() const = 0;
};

}  // namespace hermes
