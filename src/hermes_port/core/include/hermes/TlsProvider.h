#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>

namespace hermes {

struct TlsCapabilities {
    bool tls_1_2 = false;
    bool tls_1_3 = false;
    bool system_trust_store = false;
};

struct TlsPeerInfo {
    bool secure = false;
    bool peer_verified = false;
    bool hostname_verified = false;
    std::string protocol;
    std::string cipher;
    std::string peer_subject;
    std::string last_error;
};

class TlsClientSession {
public:
    virtual ~TlsClientSession() = default;

    virtual bool Handshake(int socket_fd, std::string_view hostname, std::string* error_message = nullptr) = 0;
    virtual std::ptrdiff_t Send(const void* data, std::size_t size, std::string* error_message = nullptr) = 0;
    virtual std::ptrdiff_t Receive(void* data, std::size_t size, std::string* error_message = nullptr) = 0;
    virtual TlsPeerInfo PeerInfo() const = 0;
};

class TlsProvider {
public:
    virtual ~TlsProvider() = default;

    virtual bool IsAvailable() const = 0;
    virtual std::string ProviderName() const = 0;
    virtual std::string Version() const = 0;
    virtual TlsCapabilities Capabilities() const = 0;
    virtual std::unique_ptr<TlsClientSession> CreateClientSession(
        std::string* error_message = nullptr) const = 0;
};

}  // namespace hermes
