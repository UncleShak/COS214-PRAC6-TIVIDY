/**
 * @file OverdueWorkVisitor.h
 * @brief ConcreteVisitor: lists work nobody acted on in time.
 *
 * In the team's State pattern, escalation is how overdue work is marked:
 * AvailableState::escalate() is "nobody acted on the available item in time", and
 * DeadlineDecorator's DeadlineExceeded leads to an EscalateCommand. So overdue work is
 * every WorkItem whose state is "Escalated". The escalation chain (Chain of Responsibility)
 * and the interface use this list to see what needs attention.
 *
 * @author Ona Mojela (Envy)
 */
#ifndef TIVIDY_OVERDUE_WORK_VISITOR_H
#define TIVIDY_OVERDUE_WORK_VISITOR_H

#include <string>
#include <vector>

#include "WorkflowVisitor.h"

/** @brief Collects escalated (overdue) work items in the order they are visited. */
class OverdueWorkVisitor : public WorkflowVisitor {
public:
    void visitWorkItem(WorkItem* item) override;
    void visitStage(Stage* stage) override;

    /** @brief The overdue items found, in visit order. Not owned. */
    const std::vector<WorkItem*>& overdueItems() const { return overdue; }
    /** @brief Names of the overdue items, in visit order. */
    std::vector<std::string> overdueNames() const;
    /** @brief True if any visited work is overdue. */
    bool hasOverdueWork() const { return !overdue.empty(); }

    /** @brief Clears the list so the same visitor can check again. */
    void reset() { overdue.clear(); }

private:
    std::vector<WorkItem*> overdue;
};

#endif
