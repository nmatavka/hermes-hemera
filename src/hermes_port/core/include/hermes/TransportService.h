#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace hermes {

enum class TransportSecurity {
    kPlaintext,
    kTls,
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
    std::unique_ptr<TransportConnection> Connect(const TransportEndpoint& endpoint,
                                                 std::string* error_message = nullptr) override;
    bool Supports(TransportSecurity security) const override;
};

}  // namespace hermes
