#include "TestRegistry.h"

#include "hermes/OpenSslTlsProvider.h"
#include "hermes/TransportService.h"

#if !defined(_WIN32)
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <thread>
#endif

HERMES_TEST(SocketTransportServiceAdvertisesOnlyPlaintextSupport) {
    hermes::SocketTransportService service;
    HERMES_CHECK(service.Supports(hermes::TransportSecurity::kPlaintext));
    HERMES_CHECK(!service.Supports(hermes::TransportSecurity::kImplicitTls));
}

HERMES_TEST(SocketTransportServiceRejectsImplicitTlsWithoutTlsProvider) {
    hermes::SocketTransportService service;
    std::string error_message;
    const auto connection =
        service.Connect({"localhost", 443, hermes::TransportSecurity::kImplicitTls, 1000}, &error_message);
    HERMES_CHECK(!connection);
    HERMES_CHECK(!error_message.empty());
}

HERMES_TEST(SocketTransportServiceAdvertisesTlsWhenOpenSslIsAvailable) {
    hermes::OpenSslTlsProvider provider;
    hermes::SocketTransportService service(&provider);
    HERMES_CHECK(service.Supports(hermes::TransportSecurity::kPlaintext));
    HERMES_CHECK_EQ(service.Supports(hermes::TransportSecurity::kImplicitTls), provider.IsAvailable());
    HERMES_CHECK_EQ(service.Supports(hermes::TransportSecurity::kStartTls), provider.IsAvailable());
}

#if !defined(_WIN32)
HERMES_TEST(SocketTransportServiceConnectsToLocalPlaintextServer) {
    const int listener = ::socket(AF_INET, SOCK_STREAM, 0);
    HERMES_CHECK(listener >= 0);

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    HERMES_CHECK(::bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0);
    HERMES_CHECK(::listen(listener, 1) == 0);

    socklen_t address_size = sizeof(address);
    HERMES_CHECK(::getsockname(listener, reinterpret_cast<sockaddr*>(&address), &address_size) == 0);
    const std::uint16_t port = ntohs(address.sin_port);

    std::thread server([listener]() {
        const int client = ::accept(listener, nullptr, nullptr);
        if (client >= 0) {
            char buffer[64];
            (void)::recv(client, buffer, sizeof(buffer), 0);
            const char response[] = "+OK Hermes test server\r\n";
            (void)::send(client, response, sizeof(response) - 1, 0);
            ::close(client);
        }
        ::close(listener);
    });

    hermes::SocketTransportService service;
    std::string error_message;
    auto connection =
        service.Connect({"127.0.0.1", port, hermes::TransportSecurity::kPlaintext, 2000}, &error_message);
    HERMES_CHECK(static_cast<bool>(connection));
    HERMES_CHECK(connection->Send("NOOP\r\n", &error_message));
    std::string received;
    HERMES_CHECK(connection->ReceiveLine(&received, &error_message));
    HERMES_CHECK(received.find("+OK Hermes test server") != std::string::npos);
    connection->Close();

    server.join();
}
#endif
