#include "TaskDecorator.h"

#include <stdexcept>

TaskDecorator::TaskDecorator(WorkflowComponent* c) : component(c) {
    if (component == nullptr) {
        throw std::invalid_argument("TaskDecorator needs a component to wrap");
    }
}

void TaskDecorator::request() { component->request(); }

void TaskDecorator::proxyRequest() { component->proxyRequest(); }

void TaskDecorator::accept(WorkflowVisitor* v) { component->accept(v); }

WorkflowIterator* TaskDecorator::createIterator() { return component->createIterator(); }

std::string TaskDecorator::getName() const { return component->getName(); }
