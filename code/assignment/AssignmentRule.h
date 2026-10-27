#ifndef ASSIGNMENTRULE_H
#define ASSIGNMENTRULE_H
#include <vector>
#include "Participant.h"

class WorkItem;

// Strategy interface: decides WHO gets an available work item.
// Returns nullptr if nobody is eligible.
class AssignmentRule {
public:
    virtual ~AssignmentRule() {}
    virtual Participant* assignParticipant(WorkItem* item, const std::vector<Participant*>& available) = 0;
};
#endif
