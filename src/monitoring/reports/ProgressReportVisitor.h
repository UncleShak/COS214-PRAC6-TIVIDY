/**
 * @file ProgressReportVisitor.h
 * @brief ConcreteVisitor: how far an instance has got, counted per lifecycle state.
 *
 * Reads each WorkItem's current WorkItemState::getStateName() ("Available", "Started",
 * "Completed", ...), so it uses exactly the state names of the team's State pattern and
 * never hard-codes an industry's names.
 *
 * @author Ona Mojela (Envy)
 */
#ifndef TIVIDY_PROGRESS_REPORT_VISITOR_H
#define TIVIDY_PROGRESS_REPORT_VISITOR_H

#include <map>
#include <string>

#include "WorkflowVisitor.h"

/** @brief Counts work items by state and reports the share that is finished. */
class ProgressReportVisitor : public WorkflowVisitor {
public:
    void visitWorkItem(WorkItem* item) override;
    void visitStage(Stage* stage) override;

    /** @brief Work items visited. */
    int totalItems() const { return total; }
    /** @brief Stages visited. */
    int totalStages() const { return stages; }
    /** @brief Items currently in the named state, e.g. countIn("Completed"). 0 if none. */
    int countIn(const std::string& stateName) const;
    /** @brief Items per state name, in alphabetical order of the state. */
    const std::map<std::string, int>& countsByState() const { return byState; }

    /**
     * @brief Percentage of work that is finished, 0 to 100.
     *
     * Finished means Completed. Cancelled work is left out of the total, since it will
     * never be done. Returns 0 when there is nothing to do.
     */
    int percentComplete() const;

    /** @brief One line for the interface, e.g. "4 work items: 2 Completed, 1 Started, 1 Available (66% complete)". */
    std::string summary() const;

    /** @brief Clears the counts so the same visitor can report again. */
    void reset();

private:
    int total = 0;
    int stages = 0;
    std::map<std::string, int> byState;
};

#endif
