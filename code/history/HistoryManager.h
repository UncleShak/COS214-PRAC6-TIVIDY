#ifndef TIVIDY_HISTORY_MANAGER_H
#define TIVIDY_HISTORY_MANAGER_H

#include <memory>
#include <string>
#include <vector>

class InstanceSnapshot;
class WorkflowInstance;

// Memento Caretaker: keeps one instance's snapshots, newest last, without looking inside them.
// The engine saves a snapshot before every action; undo() hands the newest back to the instance.
class HistoryManager {
public:
    HistoryManager();
    ~HistoryManager();

    // Takes ownership of snapshot. label describes the action it precedes (shown on undo).
    void save(InstanceSnapshot* snapshot, const std::string& label);
    bool canUndo() const;
    std::size_t size() const;
    // Label of the action undo() would reverse. @throws std::logic_error if there is none.
    std::string nextUndoLabel() const;
    // Restores the newest snapshot into instance and forgets it; returns its label.
    // @throws std::logic_error if there is nothing to undo.
    std::string undo(WorkflowInstance& instance);

private:
    struct Entry {
        std::string label;
        std::unique_ptr<InstanceSnapshot> snapshot;
    };
    std::vector<Entry> entries;

    HistoryManager(const HistoryManager&);
    HistoryManager& operator=(const HistoryManager&);
};

#endif
