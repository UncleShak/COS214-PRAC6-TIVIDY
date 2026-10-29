#include "TaskDecorator.h"

#include <stdexcept>

TaskDecorator::TaskDecorator(WorkflowComponent* c) : component(c) {
    if (component == nullptr) {
        throw std::invalid_argument("TaskDecorator needs a component to wrap");
    }
}

void TaskDecorator::request() { component->request(); }

void TaskDecorator::accept(WorkflowVisitor* v) { component->accept(v); }

std::string TaskDecorator::getName() const { return component->getName(); }

void TaskDecorator::add(WorkflowComponent* c) { component->add(c); }

void TaskDecorator::remove(WorkflowComponent* c) { component->remove(c); }
