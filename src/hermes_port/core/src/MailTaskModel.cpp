#include "hermes/MailTaskModel.h"

#include <algorithm>

namespace hermes {

std::string ToString(MailTaskErrorKind kind) {
    switch (kind) {
        case MailTaskErrorKind::kUnknown:
            return "unknown";
        case MailTaskErrorKind::kUnsupportedMechanism:
            return "unsupported-mechanism";
        case MailTaskErrorKind::kCredentialRejected:
            return "credential-rejected";
        case MailTaskErrorKind::kCredentialAcquisitionFailed:
            return "credential-acquisition-failed";
        case MailTaskErrorKind::kHandshakeFailed:
            return "handshake-failed";
        case MailTaskErrorKind::kServerRejected:
            return "server-rejected";
        case MailTaskErrorKind::kTlsRequired:
            return "tls-required";
        case MailTaskErrorKind::kKerberosUnavailable:
            return "kerberos-unavailable";
        case MailTaskErrorKind::kServicePrincipalFailure:
            return "service-principal-failure";
        case MailTaskErrorKind::kProviderConfiguration:
            return "provider-configuration";
        case MailTaskErrorKind::kAuthorizationPending:
            return "authorization-pending";
        case MailTaskErrorKind::kAuthorizationDenied:
            return "authorization-denied";
        case MailTaskErrorKind::kTokenRefreshFailed:
            return "token-refresh-failed";
        case MailTaskErrorKind::kOAuthMechanismRejected:
            return "oauth-mechanism-rejected";
    }
    return "unknown";
}

void InMemoryMailTaskModel::UpsertTask(const MailTaskRecord& task) {
    const auto it =
        std::find_if(tasks_.begin(), tasks_.end(), [&](const MailTaskRecord& candidate) { return candidate.id == task.id; });
    if (it == tasks_.end()) {
        tasks_.push_back(task);
    } else {
        *it = task;
    }
}

bool InMemoryMailTaskModel::CompleteTask(std::string_view task_id, std::string_view status) {
    const auto it =
        std::find_if(tasks_.begin(), tasks_.end(), [&](const MailTaskRecord& task) { return task.id == task_id; });
    if (it == tasks_.end()) {
        return false;
    }
    it->state = MailTaskState::kComplete;
    it->status = std::string(status);
    it->so_far = it->total > 0 ? it->total : it->so_far;
    return true;
}

bool InMemoryMailTaskModel::FailTask(std::string_view task_id,
                                     std::string_view status,
                                     std::string_view error_message,
                                     MailTaskErrorKind kind,
                                     std::string_view mechanism) {
    const auto it =
        std::find_if(tasks_.begin(), tasks_.end(), [&](const MailTaskRecord& task) { return task.id == task_id; });
    if (it == tasks_.end()) {
        return false;
    }
    it->state = MailTaskState::kFailed;
    it->status = std::string(status);
    errors_.push_back({std::string(task_id), kind, std::string(mechanism), std::string(error_message)});
    return true;
}

std::vector<MailTaskRecord> InMemoryMailTaskModel::Tasks() const {
    return tasks_;
}

std::vector<MailTaskError> InMemoryMailTaskModel::Errors() const {
    return errors_;
}

bool InMemoryMailTaskModel::RemoveError(std::size_t index) {
    if (index >= errors_.size()) {
        return false;
    }
    errors_.erase(errors_.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

void InMemoryMailTaskModel::ClearErrors() {
    errors_.clear();
}

}  // namespace hermes
