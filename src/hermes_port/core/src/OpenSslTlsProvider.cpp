#include "hermes/OpenSslTlsProvider.h"

#if HERMES_HAS_OPENSSL
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#endif

#include <memory>
#include <string>

namespace hermes {

#if HERMES_HAS_OPENSSL
namespace {

std::string LastOpenSslError() {
    const unsigned long code = ERR_get_error();
    if (code == 0) {
        return "Unknown OpenSSL error";
    }

    char buffer[256];
    ERR_error_string_n(code, buffer, sizeof(buffer));
    return buffer;
}

std::string SubjectFromCertificate(X509* certificate) {
    if (!certificate) {
        return {};
    }

    char buffer[512];
    if (X509_NAME_oneline(X509_get_subject_name(certificate), buffer, sizeof(buffer)) == nullptr) {
        return {};
    }
    return buffer;
}

class OpenSslClientSession final : public TlsClientSession {
public:
    OpenSslClientSession() {
        ctx_.reset(SSL_CTX_new(TLS_client_method()));
        if (ctx_) {
            SSL_CTX_set_min_proto_version(ctx_.get(), TLS1_2_VERSION);
            SSL_CTX_set_default_verify_paths(ctx_.get());
            SSL_CTX_set_verify(ctx_.get(), SSL_VERIFY_PEER, nullptr);
        }
    }

    bool Handshake(int socket_fd, std::string_view hostname, std::string* error_message) override {
        if (!ctx_) {
            if (error_message) {
                *error_message = "Unable to initialize OpenSSL client context.";
            }
            return false;
        }

        ssl_.reset(SSL_new(ctx_.get()));
        if (!ssl_) {
            if (error_message) {
                *error_message = LastOpenSslError();
            }
            return false;
        }

        SSL_set_tlsext_host_name(ssl_.get(), std::string(hostname).c_str());
#if defined(SSL_CTRL_SET_TLSEXT_HOSTNAME)
        SSL_set1_host(ssl_.get(), std::string(hostname).c_str());
#endif
        SSL_set_fd(ssl_.get(), socket_fd);

        if (SSL_connect(ssl_.get()) != 1) {
            if (error_message) {
                *error_message = LastOpenSslError();
            }
            peer_info_.last_error = error_message ? *error_message : LastOpenSslError();
            return false;
        }

        peer_info_.secure = true;
        peer_info_.protocol = SSL_get_version(ssl_.get());
        peer_info_.cipher = SSL_get_cipher_name(ssl_.get());

        const long verify_result = SSL_get_verify_result(ssl_.get());
        peer_info_.peer_verified = verify_result == X509_V_OK;
        peer_info_.hostname_verified = peer_info_.peer_verified;

        X509* certificate = SSL_get1_peer_certificate(ssl_.get());
        peer_info_.peer_subject = SubjectFromCertificate(certificate);
        if (certificate) {
            X509_free(certificate);
        }

        if (!peer_info_.peer_verified) {
            peer_info_.last_error = X509_verify_cert_error_string(verify_result);
            if (error_message) {
                *error_message = peer_info_.last_error;
            }
            return false;
        }
        return true;
    }

    std::ptrdiff_t Send(const void* data, std::size_t size, std::string* error_message) override {
        const int sent = SSL_write(ssl_.get(), data, static_cast<int>(size));
        if (sent <= 0 && error_message) {
            *error_message = LastOpenSslError();
        }
        return sent;
    }

    std::ptrdiff_t Receive(void* data, std::size_t size, std::string* error_message) override {
        const int received = SSL_read(ssl_.get(), data, static_cast<int>(size));
        if (received <= 0 && error_message) {
            *error_message = LastOpenSslError();
        }
        return received;
    }

    TlsPeerInfo PeerInfo() const override {
        return peer_info_;
    }

private:
    struct SslCtxDeleter {
        void operator()(SSL_CTX* ctx) const {
            SSL_CTX_free(ctx);
        }
    };

    struct SslDeleter {
        void operator()(SSL* ssl) const {
            SSL_free(ssl);
        }
    };

    std::unique_ptr<SSL_CTX, SslCtxDeleter> ctx_;
    std::unique_ptr<SSL, SslDeleter> ssl_;
    TlsPeerInfo peer_info_;
};

}  // namespace
#endif

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
    capabilities.system_trust_store = true;
#endif
    return capabilities;
}

std::unique_ptr<TlsClientSession> OpenSslTlsProvider::CreateClientSession(std::string* error_message) const {
#if HERMES_HAS_OPENSSL
    auto session = std::make_unique<OpenSslClientSession>();
    (void)error_message;
    return session;
#else
    if (error_message) {
        *error_message = "OpenSSL is unavailable.";
    }
    return nullptr;
#endif
}

}  // namespace hermes
