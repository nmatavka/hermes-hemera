#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "hermes/TlsProvider.h"

namespace hermes {

enum class TransportSecurity {
    kPlaintext,
    kImplicitTls,
    kStartTls,
};

struct TransportTlsStatus {
    bool secure = false;
    bool peer_verified = false;
    bool hostname_verified = false;
    std::string protocol;
    std::string cipher;
    std::string peer_subject;
};

struct TransportEndpoint {
    std::string host;
    std::uint16_t port = 0;
    TransportSecurity security = TransportSecurity::kPlaintext;
    int connect_timeout_ms = 5000;
};

class TransportConnection {
public:
    virtual ~TransportConnection() = default;

    virtual bool Send(std::string_view data, std::string* error_message = nullptr) = 0;
    virtual std::string Receive(std::size_t max_bytes, std::string* error_message = nullptr) = 0;
    virtual bool ReceiveLine(std::string* line, std::string* error_message = nullptr) = 0;
    virtual bool UpgradeToTls(const TlsProvider& provider,
                              std::string_view hostname,
                              std::string* error_message = nullptr) = 0;
    virtual bool IsSecure() const = 0;
    virtual TransportTlsStatus TlsStatus() const = 0;
    virtual void Close() = 0;
};

class TransportService {
public:
    virtual ~TransportService() = default;

    virtual std::unique_ptr<TransportConnection> Connect(const TransportEndpoint& endpoint,
                                                         std::string* error_message = nullptr) = 0;
    virtual bool Supports(TransportSecurity security) const = 0;
};

class SocketTransportService final : public TransportService {
public:
    explicit SocketTransportService(const TlsProvider* tls_provider = nullptr);

    std::unique_ptr<TransportConnection> Connect(const TransportEndpoint& endpoint,
                                                 std::string* error_message = nullptr) override;
    bool Supports(TransportSecurity security) const override;

private:
    const TlsProvider* tls_provider_ = nullptr;
};

}  // namespace hermes
