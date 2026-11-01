#ifndef STATE_FACTORY_H
#define STATE_FACTORY_H

#include <string>

class WorkItemState;

// Rebuilds a WorkItemState from its getStateName(). Snapshots store state names, never
// state objects, because WorkItem::changeState deletes the state it leaves.
// @throws std::invalid_argument for an unknown name.
WorkItemState* createWorkItemState(const std::string& stateName);

#endif
