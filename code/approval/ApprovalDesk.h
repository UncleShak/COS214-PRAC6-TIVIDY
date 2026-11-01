/**
 * @file ApprovalDesk.h
 * @brief Client of the approval chain, owner of the requests.
 *
 * Submits requests, records decisions, escalates overdue ones, and keeps every
 * request it created so history and the interface can inspect past outcomes.
 * One desk per workflow instance.
 *
 * @author Leul Tamiru (Wrath)
 */
#ifndef TIVIDY_APPROVAL_DESK_H
#define TIVIDY_APPROVAL_DESK_H

#include <memory>
#include <string>
#include <vector>

#include "ApprovalRequest.h"

class ApprovalHandler;
class Clock;
class History;
class Participant;

/** @brief Owns the requests and drives the approval chain. */
class ApprovalDesk {
public:
    /**
     * @param chain Head of the approval chain (not owned).
     * @param clock Source of the current time (not owned).
     * @param history Where approvals and escalations are recorded (not owned).
     * @throws std::invalid_argument if any argument is null.
     */
    ApprovalDesk(ApprovalHandler* chain, const Clock* clock, History* history);

    /** @brief Deletes every stored request. The chain, clock and history are not deleted. */
    ~ApprovalDesk();

    /**
     * @brief Creates a request, runs it through the chain, and stores it.
     *
     * The request is built on the heap. If the chain throws, it is deleted.
     * Otherwise it is stored here, even when the chain auto-approves it, so
     * history stays complete. The history records "approval " followed by the
     * request's describe().
     *
     * @param itemName The work item the decision is about.
     * @param requestedBy Who is asking (not owned).
     * @param value Numeric value used by the chain's value checks.
     * @param reason Why a decision is needed.
     * @return A pointer to the stored request (owned by the desk).
     * @throws InvalidTransition if itemName already has a PENDING request.
     * @throws std::invalid_argument if requestedBy is null.
     */
    const ApprovalRequest* submit(const std::string& itemName, const Participant* requestedBy, double value, const std::string& reason);

    /**
     * @brief Records a decision on a pending request.
     *
     * Checks, in order: the request exists (std::out_of_range), the request is
     * PENDING (InvalidTransition), the actor's role satisfies the current
     * level's mayDecide (AccessDenied). Only then is the decision recorded.
     *
     * @param requestId Which request.
     * @param actor Who is deciding (not owned).
     * @param approve true approves, false rejects.
     * @param note Free-text note for history and the interface.
     * @return The new status (APPROVED or REJECTED).
     * @throws std::out_of_range if requestId is unknown.
     * @throws InvalidTransition if the request is already decided.
     * @throws AccessDenied if actor does not hold the current level's role.
     */
    ApprovalStatus decide(int requestId, const Participant& actor, bool approve, const std::string& note);

    /**
     * @brief Escalates every pending request whose deadline has passed.
     *
     * Each overdue request moves to the next level, or is resolved EXPIRED at
     * the end of the chain. The history records the movement. New deadlines
     * are computed from the current clock, so a single jump cannot cascade
     * through every level in one call.
     *
     * @return The requests that moved (still PENDING at a new level) or expired.
     */
    std::vector<const ApprovalRequest*> checkDeadlines();

    /** @brief Read-only view of the pending requests. */
    std::vector<const ApprovalRequest*> pending() const;
    /** @brief Every request, pending and decided, oldest first. */
    std::vector<const ApprovalRequest*> all() const;

    /**
     * @brief Finds a request by id.
     * @throws std::out_of_range if requestId is unknown.
     */
    const ApprovalRequest* get(int requestId) const;

    /** @brief True if itemName has a PENDING request. */
    bool hasPending(const std::string& itemName) const;
     // Close a stopped task's pending requests without routing a rejection path.
    void cancelPending(const std::string& itemName, const std::string& reason);
    
// Plain values saved by an instance snapshot. Handler pointers are borrowed
    // from the definition, which must outlive its instances and snapshots.
    struct RequestState {
        int id;
        std::string itemName, requesterName;
        double value;
        std::string reason;
        int raisedAt;
        ApprovalStatus status;
        ApprovalHandler* handler;
        int deadline;
        std::string decidedBy, decisionNote;
    };
    struct Snapshot {
        std::vector<RequestState> requests;
        int nextId;
        Snapshot() : nextId(1) {}
    };
    Snapshot save() const;
    void restore(const Snapshot& snapshot);

    ApprovalDesk(const ApprovalDesk&) = delete;
    ApprovalDesk& operator=(const ApprovalDesk&) = delete;

private:
    ApprovalHandler* chain;
    const Clock* clock;
    History* history;
    std::vector<std::unique_ptr<ApprovalRequest>> requests;
    int nextId;
};

#endif