/**
 * @file ApprovalRequest.h
 * @brief One request for a decision, from submission to final outcome.
 *
 * A request is created by ApprovalDesk when work needs approval, passed down the
 * ApprovalHandler chain, and finally resolved as APPROVED, REJECTED or EXPIRED.
 *
 * All per-request state lives here, so the handlers themselves can be stateless
 * and shared across instances
 *
 * @author Leul Tamiru (Wrath)
 */
#ifndef TIVIDY_APPROVAL_REQUEST_H
#define TIVIDY_APPROVAL_REQUEST_H

#include <string>

#include "WorkflowException.h"

class ApprovalHandler;

/** @brief Where a request is in its life. PENDING is the only non-final value. */
enum ApprovalStatus { PENDING, APPROVED, REJECTED, EXPIRED };

/**
 * @brief Human-readable name for a status ("Pending", "Approved", "Rejected", "Expired").
 *
 * Free function, not a member.
 */
inline const char* statusName(ApprovalStatus s) {
    
    switch (s){
        case PENDING:  return "Pending";
        case APPROVED: return "Approved";
        case REJECTED: return "Rejected";
        case EXPIRED:  return "Expired";
    }
    return "Unknown";
}

/** @brief Sentinel meaning "no deadline configured". */
const int NO_DEADLINE = -1;

/**
 * @brief One approval request: what was asked, by whom, by when, and the outcome.
 *
 * Not copyable: each request has one identity (its id) and one lifetime, owned by
 * the desk.
 */
class ApprovalRequest {
public:
    /**
     * @param id Unique request id, assigned by the desk.
     * @param itemName Name of the work item the decision is about.
     * @param requesterName Who submitted the request.
     * @param value Numeric value used by AutoApprover (e.g. monetary).
     * @param reason Why a decision is needed (e.g. "special" in the plan example).
     * @param raisedAt The current clock time when the request was submitted.
     */
    ApprovalRequest(int id, const std::string& itemName, const std::string& requesterName,
                    double value, const std::string& reason, int raisedAt);

    int getId() const { return id; }
    const std::string& getItemName() const { return itemName; }
    const std::string& getRequesterName() const { return requesterName; }
    double getValue() const { return value; }
    const std::string& getReason() const { return reason; }
    int getRaisedAt() const { return raisedAt; }
    ApprovalStatus getStatus() const { return status; }
    ApprovalHandler* getHandler() const { return handler; }
    int getDeadline() const { return deadline; }
    const std::string& getDecidedBy() const { return decidedBy; }
    const std::string& getDecisionNote() const { return decisionNote; }

    /**
     * @brief Records who is responsible for the request, and by when.
     * @param handler The handler that just received the request.
     * @param deadline Absolute clock time by which a decision is due, or NO_DEADLINE.
     * @throws InvalidTransition if the request is not PENDING.
     */
    void assignTo(ApprovalHandler* handler, int deadline);

    /**
     * @brief Records the decision and moves the request to a final state.
     * @param status APPROVED, REJECTED or EXPIRED (never PENDING).
     * @param decidedBy Who made the decision (empty string for EXPIRED-by-system).
     * @param note Free-text note for history and the TUI.
     * @throws std::invalid_argument if status == PENDING (programmer error).
     * @throws InvalidTransition if the request is already decided; the message
     *         names the request id and its current status.
     */
    void resolve(ApprovalStatus status, const std::string& decidedBy, const std::string& note);

    /**
     * @brief True only when PENDING, a deadline exists, and now >= deadline.
     */
    bool isOverdue(int now) const;

    /**
     * @brief One readable line for the TUI and history.
     * Example: "#1 'Dispatch' (special, value 5000) Pending at Supervisor"
     * Example: "#1 'Dispatch' (special, value 5000) Approved by Sam"
     */
    std::string describe() const;

    ApprovalRequest(const ApprovalRequest&) = delete;
    ApprovalRequest& operator=(const ApprovalRequest&) = delete;

private:
    int id;
    std::string itemName;
    std::string requesterName;
    double value;
    std::string reason;
    int raisedAt;

    ApprovalStatus status;
    ApprovalHandler* handler;
    int deadline;
    std::string decidedBy;
    std::string decisionNote;
};

#endif