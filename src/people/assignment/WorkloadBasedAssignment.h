#ifndef WORKLOADBASEDASSIGNMENT_H
#define WORKLOADBASEDASSIGNMENT_H
#include "AssignmentRule.h"

// Among participants with the needed role, picks the one with the least work.
class WorkloadBasedAssignment : public AssignmentRule {
public:
    Participant* assignParticipant(WorkItem* item, const std::vector<Participant*>& available);
};
#endif
