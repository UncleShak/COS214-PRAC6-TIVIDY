/**
 * @file AuditReportVisitor.h
 * @brief ConcreteVisitor: one audit line per work item: what it is, its state and who holds it.
 *
 * Gives a marker or supervisor a snapshot of who is responsible for what, for example
 * "Verify documents | Started | Thandi | role: Clerk". The order follows the work tree,
 * so the report reads in the same order as the workflow.
 *
 * @author Ona Mojela (Envy)
 */
#ifndef TIVIDY_AUDIT_REPORT_VISITOR_H
#define TIVIDY_AUDIT_REPORT_VISITOR_H

#include <string>
#include <vector>

#include "WorkflowVisitor.h"

/** @brief Builds one line per visited work item, plus a marker line per stage. */
class AuditReportVisitor : public WorkflowVisitor {
public:
    void visitWorkItem(WorkItem* item) override;
    void visitStage(Stage* stage) override;

    /** @brief The report lines, in visit order. */
    const std::vector<std::string>& lines() const { return report; }
    /** @brief The whole report, one line per entry, each ending in a newline. */
    std::string text() const;

    /** @brief Clears the report so the same visitor can run again. */
    void reset() { report.clear(); }

private:
    std::vector<std::string> report;
};

#endif
