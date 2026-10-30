#include "OverdueWorkVisitor.h"

#include <stdexcept>

#include "WorkItem.h"
#include "WorkItemState.h"

void OverdueWorkVisitor::visitWorkItem(WorkItem* item) {
    if (item == nullptr) {
        throw std::invalid_argument("OverdueWorkVisitor: work item cannot be null");
    }
    if (item->getCurrentState()->getStateName() == "Escalated") {
        overdue.push_back(item);
    }
}

void OverdueWorkVisitor::visitStage(Stage*) {
    // Nothing to check on the stage itself; its children are visited next.
}

std::vector<std::string> OverdueWorkVisitor::overdueNames() const {
    std::vector<std::string> names;
    for (std::size_t i = 0; i < overdue.size(); ++i) {
        names.push_back(overdue[i]->getName());
    }
    return names;
}
