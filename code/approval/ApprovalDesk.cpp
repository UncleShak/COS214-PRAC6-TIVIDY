#include "ApprovalDesk.h"
#include "ApprovalHandler.h"
#include "Clock.h"
#include "History.h"
#include "Participant.h"
#include "WorkflowException.h"

#include <stdexcept>
#include <string>

ApprovalDesk::ApprovalDesk(ApprovalHandler* chain, const Clock* clock, History* history)
    : chain(chain), clock(clock), history(history), nextId(1) {

    if(chain == nullptr){

        throw std::invalid_argument("ApprovalDesk: chain cannot be null");
    }
    if(clock == nullptr){

        throw std::invalid_argument("ApprovalDesk: clock cannot be null");
    }
    if(history == nullptr){

        throw std::invalid_argument("ApprovalDesk: history cannot be null");
    }
}

ApprovalDesk::~ApprovalDesk() = default;

const ApprovalRequest* ApprovalDesk::submit(const std::string& itemName, const Participant* requestedBy, double value, const std::string& reason) {

    if(requestedBy == nullptr){
      
        throw std::invalid_argument("ApprovalDesk::submit: requestedBy cannot be null");
    }

    if(hasPending(itemName)){

        throw InvalidTransition("ApprovalDesk::submit: '" + itemName + "' already has a PENDING request");
    }

    const int now = clock->now();

    std::unique_ptr<ApprovalRequest> request(new ApprovalRequest(nextId, itemName, requestedBy->getName(), value, reason, now));

    chain->handleRequest(request.get(), now);

    ApprovalRequest* stored = request.get();
    requests.push_back(std::move(request));
    ++nextId;

    history->record("approval " + stored->describe());
    return stored;
}

ApprovalStatus ApprovalDesk::decide(int requestId,
                                    const Participant& actor,
                                    bool approve,
                                    const std::string& note) {

    ApprovalRequest* request = nullptr;
    for (std::size_t i = 0; i < requests.size(); ++i) {
        if (requests[i]->getId() == requestId) {
            request = requests[i].get();
            break;
        }
    }
    if (request == nullptr) {
        throw std::out_of_range("ApprovalDesk::decide: unknown request id " +
                                std::to_string(requestId));
    }
    if (request->getStatus() != PENDING) {
        throw InvalidTransition("ApprovalDesk::decide: request #" +
                                std::to_string(requestId) + " is already " +
                                statusName(request->getStatus()));
    }

    ApprovalHandler* handler = request->getHandler();
    if (handler == nullptr || !handler->mayDecide(actor)) {
        throw AccessDenied("ApprovalDesk::decide: " + actor.getName() +
                           " does not hold the role required at '" +
                           (handler != nullptr ? handler->getName() : std::string("(unassigned)")) + "'");
    }

    const ApprovalStatus newStatus = approve ? APPROVED : REJECTED;
    request->resolve(newStatus, actor.getName(), note);
    history->record("approval " + request->describe());
    return newStatus;
}

std::vector<const ApprovalRequest*> ApprovalDesk::checkDeadlines() {

    std::vector<const ApprovalRequest*> moved;
    const int now = clock->now();

    for (std::size_t i = 0; i < requests.size(); ++i) {
        ApprovalRequest* request = requests[i].get();
        if (request->getStatus() != PENDING || !request->isOverdue(now)) {
            continue;
        }
        ApprovalHandler* handler = request->getHandler();
        if (handler != nullptr) {
            handler->escalate(request, now);
        }
        history->record("approval escalated: " + request->describe());
        moved.push_back(request);
    }
    return moved;
}

std::vector<const ApprovalRequest*> ApprovalDesk::pending() const {

    std::vector<const ApprovalRequest*> result;
    for (std::size_t i = 0; i < requests.size(); ++i) {
        if (requests[i]->getStatus() == PENDING) {
            result.push_back(requests[i].get());
        }
    }
    return result;
}

std::vector<const ApprovalRequest*> ApprovalDesk::all() const {
    std::vector<const ApprovalRequest*> result;
    for (std::size_t i = 0; i < requests.size(); ++i) result.push_back(requests[i].get());
    return result;
}

const ApprovalRequest* ApprovalDesk::get(int requestId) const {

    for (std::size_t i = 0; i < requests.size(); ++i) {
        if (requests[i]->getId() == requestId) {
            return requests[i].get();
        }
    }
    throw std::out_of_range("ApprovalDesk::get: unknown request id " +
                            std::to_string(requestId));
}

bool ApprovalDesk::hasPending(const std::string& itemName) const {

    for (std::size_t i = 0; i < requests.size(); ++i) {
        if (requests[i]->getStatus() == PENDING &&
            requests[i]->getItemName() == itemName) {
            return true;
        }
    }
    return false;
}

ApprovalDesk::Snapshot ApprovalDesk::save() const {
    Snapshot snapshot;
    snapshot.nextId = nextId;
    for (std::size_t i = 0; i < requests.size(); ++i) {
        const ApprovalRequest& request = *requests[i];
        RequestState saved;
        saved.id = request.getId();
        saved.itemName = request.getItemName();
        saved.requesterName = request.getRequesterName();
        saved.value = request.getValue();
        saved.reason = request.getReason();
        saved.raisedAt = request.getRaisedAt();
        saved.status = request.getStatus();
        saved.handler = request.getHandler();
        saved.deadline = request.getDeadline();
        saved.decidedBy = request.getDecidedBy();
        saved.decisionNote = request.getDecisionNote();
        snapshot.requests.push_back(saved);
    }
    return snapshot;
}

void ApprovalDesk::restore(const Snapshot& snapshot) {
    // Build the replacement first so an allocation failure leaves this desk intact.
    std::vector<std::unique_ptr<ApprovalRequest> > restored;
    for (std::size_t i = 0; i < snapshot.requests.size(); ++i) {
        const RequestState& saved = snapshot.requests[i];
        std::unique_ptr<ApprovalRequest> request(new ApprovalRequest(
            saved.id, saved.itemName, saved.requesterName, saved.value,
            saved.reason, saved.raisedAt));
        request->assignTo(saved.handler, saved.deadline);
        if (saved.status != PENDING)
            request->resolve(saved.status, saved.decidedBy, saved.decisionNote);
        restored.push_back(std::move(request));
    }
    requests.swap(restored);
    nextId = snapshot.nextId;
}

void ApprovalDesk::cancelPending(const std::string& itemName, const std::string& reason) {
    for (std::size_t i = 0; i < requests.size(); ++i) {
        ApprovalRequest* request = requests[i].get();
        if (request->getItemName() == itemName && request->getStatus() == PENDING) {
            // The existing status enum has no Cancelled value. EXPIRED means
            // this request is no longer actionable; the note states the cause.
            request->resolve(EXPIRED, "System", reason);
            history->record("approval cancelled: " + request->describe());
        }
    }
}
