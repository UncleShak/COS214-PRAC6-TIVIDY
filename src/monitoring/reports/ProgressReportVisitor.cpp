#include "ProgressReportVisitor.h"

#include <sstream>
#include <stdexcept>

#include "WorkItem.h"
#include "WorkItemState.h"

void ProgressReportVisitor::visitWorkItem(WorkItem* item) {
    if (item == nullptr) {
        throw std::invalid_argument("ProgressReportVisitor: work item cannot be null");
    }
    ++total;
    ++byState[item->getCurrentState()->getStateName()];
}

void ProgressReportVisitor::visitStage(Stage*) {
    // The stage passes this visitor on to its children itself; here we only count it.
    ++stages;
}

int ProgressReportVisitor::countIn(const std::string& stateName) const {
    std::map<std::string, int>::const_iterator it = byState.find(stateName);
    return it == byState.end() ? 0 : it->second;
}

int ProgressReportVisitor::percentComplete() const {
    const int toDo = total - countIn("Cancelled");
    if (toDo <= 0) return 0;
    return (countIn("Completed") * 100) / toDo;
}

std::string ProgressReportVisitor::summary() const {
    std::ostringstream out;
    out << total << (total == 1 ? " work item" : " work items");
    if (!byState.empty()) {
        out << ": ";
        bool first = true;
        for (std::map<std::string, int>::const_iterator it = byState.begin(); it != byState.end(); ++it) {
            if (!first) out << ", ";
            out << it->second << " " << it->first;
            first = false;
        }
    }
    out << " (" << percentComplete() << "% complete)";
    return out.str();
}

void ProgressReportVisitor::reset() {
    total = 0;
    stages = 0;
    byState.clear();
}
