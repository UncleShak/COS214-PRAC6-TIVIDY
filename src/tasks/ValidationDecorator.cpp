#include "ValidationDecorator.h"

#include "WorkflowException.h"
#include "Constraint.h"

#include <stdexcept>

ValidationDecorator::ValidationDecorator(WorkflowComponent* c, const Constraint* constraint)
    : TaskDecorator(c), constraint(constraint) {
    if (constraint == nullptr) {
        throw std::invalid_argument("ValidationDecorator needs a constraint");
    }
}

void ValidationDecorator::request() {
    if (!constraint->isSatisfied()) {
        throw ValidationFailed("'" + getName() + "' blocked: " + constraint->describe());
    }
    TaskDecorator::request();
}
