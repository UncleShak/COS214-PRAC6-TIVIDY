/**
 * @file ApprovalHandler.h
 * @brief Handler participant: one level in the chain that decides an ApprovalRequest.
 *
 * Each concrete handler either handles a request itself (via mayDecide and withinLimit)
 * or delegates to its successor through handleRequest. Chains are built with setNext,
 * so callers can write a->setNext(b)->setNext(c). If a request reaches the end of the
 * chain without a decision, the base class resolves it, so no request goes unhandled
 *
 * @author Leul Tamiru (Wrath)
 */
#ifndef TIVIDY_APPROVAL_HANDLER_H
#define TIVIDY_APPROVAL_HANDLER_H

#include <string>

class ApprovalRequest;
class Participant;

/** @brief Sentinel meaning "this handler accepts any value" in withinLimit. */
const double UNLIMITED = -1.0;

/** @brief One level in the approval chain. Owns its successor. */
class ApprovalHandler {
public:
    virtual ~ApprovalHandler();

    /**
     * @brief Links the successor and returns it, so chains can be built in one line.
     * @param next The next handler in the chain (may be null). Ownership transfers here.
     * @return next, so a->setNext(b)->setNext(c) works.
     * @throws std::invalid_argument if next is this handler.
     */
    ApprovalHandler* setNext(ApprovalHandler* next);

    /**
     * @brief Handles the request at this level, or delegates to the successor.
     *
     * Default: if a successor exists, call successor_->handleRequest(request, now).
     * Otherwise resolve the request REJECTED, decidedBy "(chain)", with the note
     * "no approver has sufficient authority".
     */
    virtual void handleRequest(ApprovalRequest* request, int now);

    /**
     * @brief Hands the request to the next level after a deadline or refusal.
     *
     * Default: if a successor exists, call successor_->handleRequest(request, now).
     * Otherwise resolve the request EXPIRED, decidedBy "(chain)", with the note
     * "escalation exhausted: nobody responded".
     */
    virtual void escalate(ApprovalRequest* request, int now);

    /** @brief The role this handler checks. Default empty (no role restriction). */
    virtual std::string getRole() const { return ""; }

    /** @brief True only when this handler's role is non-empty and the actor holds it. */
    bool mayDecide(const Participant& actor) const;

    /** @brief The level name, as used in describe() and history. */
    const std::string& getName() const { return levelName; }

    ApprovalHandler(const ApprovalHandler&) = delete;
    ApprovalHandler& operator=(const ApprovalHandler&) = delete;

protected:
    /** @param levelName Shown in describe() and history ("Supervisor", "Manager", ...). */
    explicit ApprovalHandler(const std::string& levelName);

    /** @brief True when maxValue is UNLIMITED or value <= maxValue. */
    static bool withinLimit(double value, double maxValue) {
        return maxValue == UNLIMITED || value <= maxValue;
    }

private:
    std::string levelName;
    ApprovalHandler* successor;
};

#endif