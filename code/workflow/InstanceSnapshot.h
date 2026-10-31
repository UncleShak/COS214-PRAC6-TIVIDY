#ifndef INSTANCE_SNAPSHOT_H
#define INSTANCE_SNAPSHOT_H

#include <map>
#include <memory>
#include <string>
#include "DataPayload.h"
#include "ProgressionTracker.h"

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
                     const ProgressionState& progress)
        : savedItems(items), savedPayload(payload), savedProgress(progress) {}

    const std::map<std::string, ItemRecord>& getSavedItems() const { return savedItems; }
    const DataPayload* getSavedPayload() const { return savedPayload.get(); }
    const ProgressionState& getSavedProgress() const { return savedProgress; }

private:
    std::map<std::string, ItemRecord> savedItems;
    std::unique_ptr<DataPayload> savedPayload;
    ProgressionState savedProgress;

    InstanceSnapshot(const InstanceSnapshot&);
    InstanceSnapshot& operator=(const InstanceSnapshot&);
};

#endif
