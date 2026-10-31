#include "Stage.h"
#include "../visitor/WorkflowVisitor.h"
#include <stdexcept>
Stage::Stage(const std::string& n) : name(n) {
    if (n.empty()) throw std::invalid_argument("Stage needs a name");
}
namespace {
bool contains(const Stage& tree, const WorkflowComponent* target) {
    if (&tree == target) return true;
    for (const auto& en#include "Stage.h"
#include "../visitor/WorkflowVisitor.h"
#include <stdexcept>

Stage::Stage(const std::string& value) : name(value) {
    if (name.empty()) throw std::invalid_argument("Stage needs a name");
}
Stage::~Stage() {
    for (std::size_t i = 0; i < children.size(); i++) delete children[i];
}
std::string Stage::getName() const { return name; }
const std::vector<WorkflowComponent*>& Stage::getChildren() const { return children; }

static bool contains(Stage* tree, WorkflowComponent* target) {
    if (tree == target) return true;
    const std::vector<WorkflowComponent*>& children = tree->getChildren();
    for (std::size_t i = 0; i < children.size(); i++) {
        if (children[i] == target) return true;
        Stage* nested = dynamic_cast<Stage*>(children[i]);
        if (nested != nullptr && contains(nested, target)) return true;
    }
    return false;
}
void Stage::add(WorkflowComponent* child) {
    if (child == nullptr || child == this) throw std::invalid_argument("Invalid child");
    if (contains(this, child)) throw std::invalid_argument("Child already belongs to this tree");
    Stage* nested = dynamic_cast<Stage*>(child);
    if (nested != nullptr && contains(nested, this)) throw std::invalid_argument("Stage cycle");
    children.push_back(child);
}
void Stage::remove(WorkflowComponent* child) {
    for (std::size_t i = 0; i < children.size(); i++) {
        if (children[i] == child) {
            children.erase(children.begin() + i);
            delete child;
            return;
        }
    }
    throw std::invalid_argument("Not a direct child");
}
void Stage::request() {
    for (std::size_t i = 0; i < children.size(); i++) children[i]->request();
}
void Stage::accept(WorkflowVisitor* visitor) {
    if (visitor == nullptr) throw std::invalid_argument("Visitor cannot be null");
    visitor->visitStage(this);
    for (std::size_t i = 0; i < children.size(); i++) children[i]->accept(visitor);
}
try : tree.getChildren()) {
        if (entry.get() == target) return true;
        const Stage* nested = dynamic_cast<const Stage*>(entry.get());
        if (nested && contains(*nested, target)) return true;
    }
    return false;
}
}
void Stage::add(WorkflowComponent* child) {
    if (!child || child == this) throw std::invalid_argument("Invalid stage child");
    const Stage* stage = dynamic_cast<const Stage*>(child);
    if (stage && contains(*stage, this)) throw std::invalid_argument("Stage cycle");
    for (const auto& existing : children)
        if (existing.get() == child) throw std::invalid_argument("Child already belongs to stage");
    // Allocate the vector slot before accepting ownership.
    children.emplace_back();
    children.back().reset(child);
}
void Stage::remove(WorkflowComponent* child) {
    for (auto it = children.begin(); it != children.end(); ++it)
        if (it->get() == child) { children.erase(it); return; }
    throw std::invalid_argument("Not a direct child of this stage");
}
void Stage::request() { for (const auto& child : children) child->request(); }
void Stage::accept(WorkflowVisitor* visitor) {
    if (!visitor) throw std::invalid_argument("Visitor cannot be null");
    visitor->visitStage(this);
    for (const auto& child : children) child->accept(visitor);
}
