#ifndef INSTANCE_SNAPSHOT_H
#define INSTANCE_SNAPSHOT_H

#include <map>
#include <memory>
#include <string>
#include "DataPayload.h"
#include "ProgressionTracker.h"
#include "ApprovalDesk.h"

class WorkflowInstance;
class Participant;

// Memento: everything needed to put a WorkflowInstance back exactly as it was.
// It stores state *names* (WorkItem::changeState deletes state objects), each item's assignee
// (participants are owned by the engine and outlive snapshots), a deep copy of the payload,
// and the ProgressionTracker's progress, so routing resumes from the right place.
class InstanceSnapshot {
public:
    struct ItemRecord {
        std::string state;
        Participant* assignee;
    };

    // Takes ownership of payload (may be null).
    InstanceSnapshot(const std::map<std::string, ItemRecord>& items, DataPayload* payload,
                     const ProgressionState& progress,
                     const ApprovalDesk::Snapshot& approvals,
                     const std::map<std::string, int>& deadlines,
                     std::size_t commandCount, const WorkflowInstance* owner)
        : savedApprovals(approvals), savedDeadlines(deadlines),
          savedCommandCount(commandCount), owner(owner), savedItems(items),
          savedPayload(payload), savedProgress(progress) {}

    const std::map<std::string, ItemRecord>& getSavedItems() const { return savedItems; }
    const DataPayload* getSavedPayload() const { return savedPayload.get(); }
    const ProgressionState& getSavedProgress() const { return savedProgress; }

    const ApprovalDesk::Snapshot& getSavedApprovals() const { return savedApprovals; }
    const std::map<std::string, int>& getSavedDeadlines() const { return savedDeadlines; }
    std::size_t getSavedCommandCount() const { return savedCommandCount; }
    const WorkflowInstance* getOwner() const { return owner; }
    void saveExternal(const std::string& item, std::unique_ptr<DataPayload> state) {
        externalStates[item] = std::move(state);
    }
    const std::map<std::string, std::unique_ptr<DataPayload> >& getExternalStates() const {
        return externalStates;
    }
    bool externalWasCancelled() const { return externalCancelled; }
    void saveExternalCancellation(bool value) { externalCancelled = value; }
private:

    std::map<std::string, std::unique_ptr<DataPayload> > externalStates;
    bool externalCancelled = false;
    ApprovalDesk::Snapshot savedApprovals;
    std::map<std::string, int> savedDeadlines;
    std::size_t savedCommandCount;
    const WorkflowInstance* owner; // Identity check only; not owned.
    std::map<std::string, ItemRecord> savedItems;
    std::unique_ptr<DataPayload> savedPayload;
    ProgressionState savedProgress;

    InstanceSnapshot(const InstanceSnapshot&);
    InstanceSnapshot& operator=(const InstanceSnapshot&);
};

#endif
