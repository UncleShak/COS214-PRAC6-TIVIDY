#include "HistoryManager.h"
#include <stdexcept>
#include "InstanceSnapshot.h"
#include "WorkflowInstance.h"

HistoryManager::HistoryManager() {}
HistoryManager::~HistoryManager() {}

void HistoryManager::save(InstanceSnapshot* snapshot, const std::string& label) {
    if (snapshot == nullptr) throw std::invalid_argument("HistoryManager: snapshot cannot be null");
    Entry entry;
    entry.label = label;
    entry.snapshot.reset(snapshot);
    entries.push_back(std::move(entry));
}

bool HistoryManager::canUndo() const { return !entries.empty(); }
std::size_t HistoryManager::size() const { return entries.size(); }

std::string HistoryManager::nextUndoLabel() const {
    if (entries.empty()) throw std::logic_error("Nothing to undo");
    return entries.back().label;
}

std::string HistoryManager::undo(WorkflowInstance& instance) {
    if (entries.empty()) throw std::logic_error("Nothing to undo");
    instance.restore(entries.back().snapshot.get());
    const std::string label = entries.back().label;
    entries.pop_back();
    return label;
}
