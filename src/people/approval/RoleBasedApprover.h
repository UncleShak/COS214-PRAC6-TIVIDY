/**
 * @file RoleBasedApprover.h
 * @brief ConcreteHandler: hands work to a person who holds the role, and waits for a decision.
 *
 * The level that actually asks somebody. On receiving a request within its value limit,
 * it assigns the request to itself with a deadline (or NO_DEADLINE for a level that
 * never escalates on time) and leaves it PENDING, so a participant can decide it later.
 * If the value exceeds maxValue, it delegates to its successor.
 *
 * @author Leul Tamiru (Wrath)
 */
#ifndef TIVIDY_ROLE_BASED_APPROVER_H
#define TIVIDY_ROLE_BASED_APPROVER_H

#include <string>

#include "ApprovalHandler.h"

/** @brief Assigns work to a named role; the request stays PENDING until a participant decides. */
class RoleBasedApprover : public ApprovalHandler {
public:
    /**
     * @param levelName Shown in describe() and history ("Supervisor", "Manager", ...).
     * @param role The role a participant must hold to decide this request.
     * @param maxValue The largest value this level decides, or UNLIMITED.
     * @param timeLimit Clock units after which the request escalates; 0 means never.
     */
    RoleBasedApprover(const std::string& levelName, const std::string& role, double maxValue, int timeLimit);

    /**
     * @brief Assigns the request to this level if within the limit; otherwise delegates.
     *
     * On assignment: computes the deadline as now + timeLimit, or NO_DEADLINE when
     * timeLimit is 0, and calls request->assignTo(this, deadline). The request stays
     * PENDING, so a person decides it.
     */
    void handleRequest(ApprovalRequest* request, int now) override;

    /** @brief The role a participant must hold to decide this level's requests. */
    std::string getRole() const override;

private:
    std::string role;
    double maxValue;
    int timeLimit;
};

#endif