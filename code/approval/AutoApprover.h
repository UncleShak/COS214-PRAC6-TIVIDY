/**
 * @file AutoApprover.h
 * @brief ConcreteHandler: approves anything under a value limit without asking a person.
 *
 * The first level in most chains. It reads only the value on the request, so it never
 * waits for a human. If the value exceeds maxValue, it delegates to its successor
 * (typically a RoleBasedApprover). See Section 7.3 of the plan.
 *
 * @author Leul Tamiru (Wrath)
 */
#ifndef TIVIDY_AUTO_APPROVER_H
#define TIVIDY_AUTO_APPROVER_H

#include "ApprovalHandler.h"

/** @brief Auto-approves requests whose value is within maxValue. */
class AutoApprover : public ApprovalHandler {
public:
    /**
     * @param levelName Shown in describe() and history.
     * @param maxValue The largest value this level approves automatically,
     *                 or UNLIMITED to approve everything.
     */
    AutoApprover(const std::string& levelName, double maxValue);

    /**
     * @brief Approves the request if its value is within the limit; otherwise delegates.
     *
     * On approval: resolves the request APPROVED, decidedBy "auto:" + getName(),
     * note "within automatic limit". Never assigns the request, so nothing waits
     * at this level.
     */
    void handleRequest(ApprovalRequest* request, int now) override;

private:
    double maxValue;
};

#endif