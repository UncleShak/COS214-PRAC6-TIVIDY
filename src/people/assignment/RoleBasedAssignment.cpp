#include "RoleBasedAssignment.h"
#include "WorkItem.h"

Participant* RoleBasedAssignment::assignParticipant(WorkItem* item, const std::vector<Participant*>& available) {
    if (item == nullptr) {
        return nullptr;
    }
    
    for (size_t i = 0; i < available.size(); i++) {
        if (available[i]->hasRole(item->getRequiredRole())) {
            return available[i];
        }
    }
    return nullptr;
}
