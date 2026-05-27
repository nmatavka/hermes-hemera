#include "hermes/MailTaskModel.h"

#include <algorithm>

namespace hermes {

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
                                     std::string_view error_message) {
    const auto it =
        std::find_if(tasks_.begin(), tasks_.end(), [&](const MailTaskRecord& task) { return task.id == task_id; });
    if (it == tasks_.end()) {
        return false;
    }
    it->state = MailTaskState::kFailed;
    it->status = std::string(status);
    errors_.push_back({std::string(task_id), std::string(error_message)});
    return true;
}

std::vector<MailTaskRecord> InMemoryMailTaskModel::Tasks() const {
    return tasks_;
}

std::vector<MailTaskError> InMemoryMailTaskModel::Errors() const {
    return errors_;
}

}  // namespace hermes
