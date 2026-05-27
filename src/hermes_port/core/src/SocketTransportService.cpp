#include "hermes/TransportService.h"

#include <cerrno>
#include <cstring>
#include <utility>

#if !defined(_WIN32)
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

namespace hermes {

namespace {

class SocketTransportConnection final : public TransportConnection {
public:
    explicit SocketTransportConnection(int socket_fd) : socket_fd_(socket_fd) {}

    ~SocketTransportConnection() override {
        Close();
    }

    bool Send(std::string_view data, std::string* error_message) override {
#if defined(_WIN32)
        (void)data;
        if (error_message) {
            *error_message = "Socket transport is only implemented for POSIX platforms.";
        }
        return false;
#else
        const char* buffer = data.data();
        std::size_t remaining = data.size();
        while (remaining != 0) {
            const auto sent = SendRaw(buffer, remaining, error_message);
            if (sent <= 0) {
                if (error_message && error_message->empty()) {
                    *error_message = "Socket closed while sending data.";
                }
                return false;
            }
            remaining -= static_cast<std::size_t>(sent);
            buffer += sent;
        }
        return true;
#endif
    }

    std::string Receive(std::size_t max_bytes, std::string* error_message) override {
#if defined(_WIN32)
        if (error_message) {
            *error_message = "Socket transport is only implemented for POSIX platforms.";
        }
        return {};
#else
        std::string output;
        if (!buffered_data_.empty()) {
            const std::size_t take = std::min(max_bytes, buffered_data_.size());
            output = buffered_data_.substr(0, take);
            buffered_data_.erase(0, take);
            return output;
        }

        output.resize(max_bytes);
        const auto received = ReceiveRaw(output.data(), output.size(), error_message);
        if (received <= 0) {
            output.clear();
            return output;
        }
        output.resize(static_cast<std::size_t>(received));
        return output;
#endif
    }

    bool ReceiveLine(std::string* line, std::string* error_message) override {
#if defined(_WIN32)
        if (error_message) {
            *error_message = "Socket transport is only implemented for POSIX platforms.";
        }
        return false;
#else
        if (!line) {
            if (error_message) {
                *error_message = "ReceiveLine requires an output buffer.";
            }
            return false;
        }

        for (;;) {
            const std::size_t newline = buffered_data_.find('\n');
            if (newline != std::string::npos) {
                *line = buffered_data_.substr(0, newline);
                if (!line->empty() && line->back() == '\r') {
                    line->pop_back();
                }
                buffered_data_.erase(0, newline + 1);
                return true;
            }

            char scratch[1024];
            const auto received = ReceiveRaw(scratch, sizeof(scratch), error_message);
            if (received <= 0) {
                if (error_message && error_message->empty()) {
                    *error_message = "Socket closed while receiving a line.";
                }
                return false;
            }
            buffered_data_.append(scratch, static_cast<std::size_t>(received));
        }
#endif
    }

    bool UpgradeToTls(const TlsProvider& provider,
                      std::string_view hostname,
                      std::string* error_message) override {
#if defined(_WIN32)
        if (error_message) {
            *error_message = "Socket transport is only implemented for POSIX platforms.";
        }
        return false;
#else
        if (tls_session_) {
            return true;
        }
        auto session = provider.CreateClientSession(error_message);
        if (!session) {
            return false;
        }
        if (!session->Handshake(socket_fd_, hostname, error_message)) {
            return false;
        }
        const TlsPeerInfo peer = session->PeerInfo();
        tls_status_.secure = peer.secure;
        tls_status_.peer_verified = peer.peer_verified;
        tls_status_.hostname_verified = peer.hostname_verified;
        tls_status_.protocol = peer.protocol;
        tls_status_.cipher = peer.cipher;
        tls_status_.peer_subject = peer.peer_subject;
        tls_session_ = std::move(session);
        return true;
#endif
    }

    bool IsSecure() const override {
        return tls_session_ && tls_status_.secure;
    }

    TransportTlsStatus TlsStatus() const override {
        return tls_status_;
    }

    void Close() override {
#if !defined(_WIN32)
        tls_session_.reset();
        if (socket_fd_ >= 0) {
            ::close(socket_fd_);
            socket_fd_ = -1;
        }
#endif
    }

private:
#if !defined(_WIN32)
    std::ptrdiff_t SendRaw(const void* data, std::size_t size, std::string* error_message) {
        if (tls_session_) {
            return tls_session_->Send(data, size, error_message);
        }
        const ssize_t sent = ::send(socket_fd_, data, size, 0);
        if (sent < 0 && error_message) {
            *error_message = std::strerror(errno);
        }
        return sent;
    }

    std::ptrdiff_t ReceiveRaw(void* data, std::size_t size, std::string* error_message) {
        if (tls_session_) {
            return tls_session_->Receive(data, size, error_message);
        }
        const ssize_t received = ::recv(socket_fd_, data, size, 0);
        if (received < 0 && error_message) {
            *error_message = std::strerror(errno);
        }
        return received;
    }
#endif

    int socket_fd_ = -1;
    std::string buffered_data_;
    std::unique_ptr<TlsClientSession> tls_session_;
    TransportTlsStatus tls_status_;
};

#if !defined(_WIN32)
bool ApplySocketTimeout(int socket_fd, int timeout_ms) {
    if (timeout_ms <= 0) {
        return true;
    }

    timeval timeout{};
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;
    return setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == 0 &&
           setsockopt(socket_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) == 0;
}
#endif

}  // namespace

SocketTransportService::SocketTransportService(const TlsProvider* tls_provider) : tls_provider_(tls_provider) {}

std::unique_ptr<TransportConnection> SocketTransportService::Connect(const TransportEndpoint& endpoint,
                                                                     std::string* error_message) {
#if defined(_WIN32)
    if (error_message) {
        *error_message = "Socket transport is only implemented for POSIX platforms.";
    }
    return nullptr;
#else
    if ((endpoint.security == TransportSecurity::kImplicitTls || endpoint.security == TransportSecurity::kStartTls) &&
        (!tls_provider_ || !tls_provider_->IsAvailable())) {
        if (error_message) {
            *error_message = "TLS support is unavailable for this transport service.";
        }
        return nullptr;
    }

    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo* result = nullptr;
    const std::string port = std::to_string(endpoint.port);
    const int status = ::getaddrinfo(endpoint.host.c_str(), port.c_str(), &hints, &result);
    if (status != 0) {
        if (error_message) {
            *error_message = gai_strerror(status);
        }
        return nullptr;
    }

    std::unique_ptr<TransportConnection> connection;
    for (addrinfo* candidate = result; candidate != nullptr; candidate = candidate->ai_next) {
        const int socket_fd = ::socket(candidate->ai_family, candidate->ai_socktype, candidate->ai_protocol);
        if (socket_fd < 0) {
            continue;
        }

        ApplySocketTimeout(socket_fd, endpoint.connect_timeout_ms);
        if (::connect(socket_fd, candidate->ai_addr, candidate->ai_addrlen) == 0) {
            auto socket_connection = std::make_unique<SocketTransportConnection>(socket_fd);
            if (endpoint.security == TransportSecurity::kImplicitTls &&
                !socket_connection->UpgradeToTls(*tls_provider_, endpoint.host, error_message)) {
                socket_connection->Close();
                continue;
            }
            connection = std::move(socket_connection);
            break;
        }

        ::close(socket_fd);
    }

    ::freeaddrinfo(result);
    if (!connection && error_message) {
        *error_message = "Unable to connect to " + endpoint.host + ':' + port;
    }
    return connection;
#endif
}

bool SocketTransportService::Supports(TransportSecurity security) const {
    if (security == TransportSecurity::kPlaintext) {
        return true;
    }
    return tls_provider_ && tls_provider_->IsAvailable();
}

}  // namespace hermes
