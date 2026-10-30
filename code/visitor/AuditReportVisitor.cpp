#include "AuditReportVisitor.h"

#include <stdexcept>

#include "Participant.h"
#include "WorkItem.h"
#include "WorkItemState.h"

void AuditReportVisitor::visitWorkItem(WorkItem* item) {
    if (item == nullptr) {
        throw std::invalid_argument("AuditReportVisitor: work item cannot be null");
    }
    const Participant* holder = item->getAssignedParticipant();
    report.push_back(item->getName() + " | " +
                     item->getCurrentState()->getStateName() + " | " +
                     (holder != nullptr ? holder->getName() : std::string("unassigned")) +
                     " | role: " + item->getRequiredRole());
}

void AuditReportVisitor::visitStage(Stage*) {
    // Stage has no public header yet (Pride), so only its position is marked; its
    // children are visited next and follow this line.
    report.push_back("-- stage --");
}

std::string AuditReportVisitor::text() const {
    std::string out;
    for (std::size_t i = 0; i < report.size(); ++i) {
        out += report[i] + "\n";
    }
    return out;
}
