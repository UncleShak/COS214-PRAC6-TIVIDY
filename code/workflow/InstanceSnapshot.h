#ifndef INSTANCE_SNAPSHOT_H
#define INSTANCE_SNAPSHOT_H

#include <map>
#include <string>
#include "../state/WorkItemState.h"
#include "DataPayload.h"

class InstanceSnapshot {
private:
    std::map<std::string, WorkItemState*> savedStates;
    DataPayload* savedPayload;

public:
    // The Memento locks in the states and a deep copy of the payload
    InstanceSnapshot(const std::map<std::string, WorkItemState*>& states, DataPayload* payload)
        : savedStates(states), savedPayload(payload) {}

    ~InstanceSnapshot() {
        delete savedPayload;
    }

    std::map<std::string, WorkItemState*> getSavedStates() const { 
        return savedStates; 
    }
    
    DataPayload* getSavedPayload() const { 
        return savedPayload; 
    }
};

#endif