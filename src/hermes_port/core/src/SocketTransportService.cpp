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
            const ssize_t sent = ::send(socket_fd_, buffer, remaining, 0);
            if (sent <= 0) {
                if (error_message) {
                    *error_message = sent == 0 ? "Socket closed while sending data." : std::strerror(errno);
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
        std::string buffer(max_bytes, '\0');
        const ssize_t received = ::recv(socket_fd_, buffer.data(), buffer.size(), 0);
        if (received < 0) {
            if (error_message) {
                *error_message = std::strerror(errno);
            }
            return {};
        }
        buffer.resize(static_cast<std::size_t>(received));
        return buffer;
#endif
    }

    void Close() override {
#if !defined(_WIN32)
        if (socket_fd_ >= 0) {
            ::close(socket_fd_);
            socket_fd_ = -1;
        }
#endif
    }

private:
    int socket_fd_ = -1;
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

std::unique_ptr<TransportConnection> SocketTransportService::Connect(const TransportEndpoint& endpoint,
                                                                     std::string* error_message) {
    if (endpoint.security == TransportSecurity::kTls) {
        if (error_message) {
            *error_message =
                "TLS transport is not yet wired through the socket transport adapter. Use TlsProvider separately.";
        }
        return nullptr;
    }

#if defined(_WIN32)
    if (error_message) {
        *error_message = "Socket transport is only implemented for POSIX platforms.";
    }
    return nullptr;
#else
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
            connection = std::make_unique<SocketTransportConnection>(socket_fd);
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
    return security == TransportSecurity::kPlaintext;
}

}  // namespace hermes
