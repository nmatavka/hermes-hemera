#include "hermes/OpenSslTlsProvider.h"

#if HERMES_HAS_OPENSSL
#include <openssl/opensslv.h>
#endif

namespace hermes {

bool OpenSslTlsProvider::IsAvailable() const {
#if HERMES_HAS_OPENSSL
    return true;
#else
    return false;
#endif
}

std::string OpenSslTlsProvider::ProviderName() const {
    return "OpenSSL";
}

std::string OpenSslTlsProvider::Version() const {
#if HERMES_HAS_OPENSSL
    return OPENSSL_VERSION_TEXT;
#else
    return "OpenSSL unavailable";
#endif
}

TlsCapabilities OpenSslTlsProvider::Capabilities() const {
    TlsCapabilities capabilities;
#if HERMES_HAS_OPENSSL
    capabilities.tls_1_2 = true;
#if defined(OPENSSL_VERSION_MAJOR) && OPENSSL_VERSION_MAJOR >= 3
    capabilities.tls_1_3 = true;
#endif
    capabilities.system_trust_store = false;
#endif
    return capabilities;
}

}  // namespace hermes
