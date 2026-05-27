#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace hermes {

enum class MailTaskKind {
    kUnknown,
    kSending,
    kReceiving,
    kImapSync,
    kMailboxDiscovery,
    kImapMutation,
    kAttachmentFetch,
    kAuthentication,
    kTls,
};

enum class MailTaskState {
    kCreated,
    kWaiting,
    kQueued,
    kRunning,
    kComplete,
    kFailed,
};

enum class MailTaskErrorKind {
    kUnknown,
    kUnsupportedMechanism,
    kCredentialRejected,
    kCredentialAcquisitionFailed,
    kHandshakeFailed,
    kServerRejected,
    kTlsRequired,
    kKerberosUnavailable,
    kServicePrincipalFailure,
};

struct MailTaskRecord {
    std::string id;
    std::string persona;
    std::string title;
    std::string status;
    std::string details;
    MailTaskKind kind = MailTaskKind::kUnknown;
    MailTaskState state = MailTaskState::kCreated;
    int total = 0;
    int so_far = 0;
    bool allow_bring_to_front = true;
};

struct MailTaskError {
    std::string task_id;
    MailTaskErrorKind kind = MailTaskErrorKind::kUnknown;
    std::string mechanism;
    std::string message;
};

std::string ToString(MailTaskErrorKind kind);

class MailTaskModel {
public:
    virtual ~MailTaskModel() = default;

    virtual void UpsertTask(const MailTaskRecord& task) = 0;
    virtual bool CompleteTask(std::string_view task_id, std::string_view status) = 0;
    virtual bool FailTask(std::string_view task_id,
                          std::string_view status,
                          std::string_view error_message,
                          MailTaskErrorKind kind = MailTaskErrorKind::kUnknown,
                          std::string_view mechanism = {}) = 0;
    virtual std::vector<MailTaskRecord> Tasks() const = 0;
    virtual std::vector<MailTaskError> Errors() const = 0;
};

class InMemoryMailTaskModel final : public MailTaskModel {
public:
    void UpsertTask(const MailTaskRecord& task) override;
    bool CompleteTask(std::string_view task_id, std::string_view status) override;
    bool FailTask(std::string_view task_id,
                  std::string_view status,
                  std::string_view error_message,
                  MailTaskErrorKind kind = MailTaskErrorKind::kUnknown,
                  std::string_view mechanism = {}) override;
    std::vector<MailTaskRecord> Tasks() const override;
    std::vector<MailTaskError> Errors() const override;

private:
    std::vector<MailTaskRecord> tasks_;
    std::vector<MailTaskError> errors_;
};

}  // namespace hermes
