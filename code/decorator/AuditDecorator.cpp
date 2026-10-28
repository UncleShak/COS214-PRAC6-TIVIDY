#include "AuditDecorator.h"

#include "History.h"

#include <exception>
#include <stdexcept>

AuditDecorator::AuditDecorator(WorkflowComponent* c, History* history)
    : TaskDecorator(c), history(history) {
    if (history == nullptr) {
        throw std::invalid_argument("AuditDecorator needs a history to write to");
    }
}

void AuditDecorator::request() {
    history->record("started: " + getName());
    try {
        TaskDecorator::request();
    } catch (const std::exception& e) {
        history->record("failed: " + getName() + " (" + e.what() + ")");
        throw;  // Auditing must never hide a failure from the engine.
    }
    history->record("completed: " + getName());
}
