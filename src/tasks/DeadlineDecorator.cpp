#include "DeadlineDecorator.h"

#include "WorkflowException.h"
#include "Clock.h"

#include <climits>
#include <stdexcept>
#include <string>

DeadlineDecorator::DeadlineDecorator(WorkflowComponent* c, int duration, const Clock* clock)
    : TaskDecorator(c), duration(duration), dueTime(0), clock(clock) {
    if (clock == nullptr) {
        throw std::invalid_argument("DeadlineDecorator needs a clock");
    }
    if (duration < 0) {
        throw std::invalid_argument("DeadlineDecorator duration cannot be negative");
    }
    // Bug 20: a due time past INT_MAX would wrap negative; such a deadline never arrives.
    dueTime = clock->now() > INT_MAX - duration ? INT_MAX : clock->now() + duration;
}

void DeadlineDecorator::request() {
    if (isOverdue()) {
        throw DeadlineExceeded("'" + getName() + "' was due at " + std::to_string(dueTime) +
                               " and it is now " + std::to_string(clock->now()));
    }
    TaskDecorator::request();
}

bool DeadlineDecorator::isOverdue() const { return clock->now() > dueTime; }

// Declared in the header but never defined: a deadline adds nothing to structure, so these
// forward to the wrapped component like any decorator.
void DeadlineDecorator::add(WorkflowComponent* c) { TaskDecorator::add(c); }

void DeadlineDecorator::remove(WorkflowComponent* c) { TaskDecorator::remove(c); }
