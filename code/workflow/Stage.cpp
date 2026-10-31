#include "Stage.h"
#include <algorithm>
#include <stdexcept>
#include "WorkflowVisitor.h"

Stage::Stage(const std::string& name) : stageName(name) {
    if (name.empty()) throw std::invalid_argument("Stage needs a name");
}

std::string Stage::getName() const { return stageName; }

void Stage::request() {
    for (std::size_t i = 0; i < children.size(); ++i) children[i]->request();
}

void Stage::add(WorkflowComponent* component) {
    if (component == nullptr) throw std::invalid_argument("Cannot add a null component");
    if (component == this) throw std::invalid_argument("A stage cannot contain itself");
    if (std::find(children.begin(), children.end(), component) != children.end())
        throw std::invalid_argument("Component is already in this stage");
    children.push_back(component);
}

void Stage::remove(WorkflowComponent* component) {
    children.erase(std::remove(children.begin(), children.end(), component), children.end());
}

const std::vector<WorkflowComponent*>& Stage::getChildren() const { return children; }

void Stage::accept(WorkflowVisitor* visitor) {
    if (visitor == nullptr) throw std::invalid_argument("Visitor cannot be null");
    visitor->visitStage(this);
    for (std::size_t i = 0; i < children.size(); ++i) children[i]->accept(visitor);
}

WorkflowComponent* Stage::clone() const {
    Stage* copy = new Stage(stageName);
    try {
        for (std::size_t i = 0; i < children.size(); ++i) copy->add(children[i]->clone());
    } catch (...) {
        deleteComponentTree(copy);
        throw;
    }
    return copy;
}

void deleteComponentTree(WorkflowComponent* component) {
    Stage* stage = dynamic_cast<Stage*>(component);
    if (stage != nullptr) {
        const std::vector<WorkflowComponent*> children = stage->getChildren();
        for (std::size_t i = 0; i < children.size(); ++i) deleteComponentTree(children[i]);
    }
    delete component;
}
