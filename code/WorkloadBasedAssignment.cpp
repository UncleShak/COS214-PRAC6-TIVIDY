#include "WorkloadBasedAssignment.h"
#include "WorkItem.h"

Participant* WorkloadBasedAssignment::assignParticipant(WorkItem* item, const std::vector<Participant*>& available) {
    if (item == nullptr) {
        return nullptr;
    }

    Participant* best = nullptr;
    
    for (size_t i = 0; i < available.size(); i++) {
        Participant* p = available[i];
    
        if (!p->hasRole(item->getRequiredRole())) {
             continue;
        }
        
        if (best == nullptr || p->getWorkload() < best->getWorkload()) {
            best = p;
        }
    }
    return best;
}
