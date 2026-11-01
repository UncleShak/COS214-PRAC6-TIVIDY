#ifndef ROLEBASEDASSIGNMENT_H
#define ROLEBASEDASSIGNMENT_H
#include "AssignmentRule.h"

// Picks the first available participant who has the role the item needs.
class RoleBasedAssignment : public AssignmentRule {
public:
    Participant* assignParticipant(WorkItem* item, const std::vector<Participant*>& available);
};
#endif
