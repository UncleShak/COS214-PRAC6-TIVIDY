/**
 * @file EngineGateway.h
 * @brief The only view of the engine that the interactive interface has.
 *
 * PROPOSAL: needs Pride's approval (engine owner) before it is merged.
 * See docs/tui-plan.md section 2.
 *
 * The interface (InterfaceController, the ncurses and plain front ends) depends on
 * this abstract class and nothing else from the engine. Only plain value structs and
 * string IDs cross it, so the interface never holds a WorkItem* or WorkflowInstance*
 * and an engine refactor cannot leave dangling pointers in the UI.
 *
 * Error convention (team agreement, Wrath): every call either succeeds or throws a
 * std::exception whose what() is a message fit to show the user. The interface catches
 * std::exception once and shows what() unchanged, so the exact exception class
 * (InvalidTransition, WorkflowException, std::invalid_argument, ...) does not matter here.
 * A call that throws must leave engine state unchanged.
 *
 * @author Shakir Alexander (Lust)
 */
#ifndef TIVIDY_ENGINE_GATEWAY_H
#define TIVIDY_ENGINE_GATEWAY_H

#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace tividy {
namespace ui {

/** @brief One industry and the workflow definitions it offers. */
struct IndustryInfo {
    std::string id;                       ///< stable key, e.g. "arkham"
    std::string displayName;              ///< e.g. "ArkhamExpress"
    std::string description;              ///< one line shown on the industry picker
    std::vector<std::string> definitions; ///< definition names that can be started
};

/** @brief A value the user supplies when starting an instance. */
struct DataField {
    std::string key;          ///< KeyValuePayload key
    std::string label;        ///< prompt text
    bool numeric;             ///< must parse like KeyValuePayload::getNumber
    bool required;            ///< empty input refused
    std::string defaultValue; ///< used when the user leaves an optional field empty
};

/** @brief A participant who can act on or be assigned work. */
struct ParticipantView {
    std::string id;
    std::string name;
    std::vector<std::string> roles;
    int workload;
};

/** @brief Snapshot of one work item. Safe to keep after the engine changes. */
struct WorkItemView {
    std::string id;            ///< the item name until the engine has its own IDs
    std::string name;
    std::string state;         ///< exact WorkItemState::getStateName(): "Created", "Available", ...
    std::string trackerStatus; ///< ProgressionTracker set: "available", "waiting", "completed",
                               ///< "rejected", "cancelled", or "" if not reached yet
    std::string requiredRole;
    std::string assigneeName;  ///< "" when unassigned
    std::vector<std::string> waitingOn; ///< unmet prerequisites (empty unless waiting)
    int dueAt;                 ///< deadline on the engine clock, -1 if none

    WorkItemView() : dueAt(-1) {}
};

/** @brief Summary of one running or finished workflow instance. */
struct InstanceView {
    std::string id;
    std::string industryId;
    std::string definitionName;
    std::string definitionVersion;
    bool complete;  ///< ProgressionTracker::isComplete()
    bool stalled;   ///< ProgressionTracker::isStalled()
    std::map<std::string, std::string> data; ///< KeyValuePayload::entries()
};

/** @brief What the user can do to a work item. One per GoF Command in code/command. */
enum class WorkAction { Assign, Start, Complete, Reject, Cancel, Escalate };

/** @brief One user action, sent to the engine as a whole. */
struct ActionRequest {
    std::string instanceId;
    std::string workItemId;
    WorkAction action;
    std::string actorId;     ///< participant performing the action
    std::string assigneeId;  ///< Assign only. Empty: the industry's AssignmentRule chooses
    std::string reason;      ///< Reject only

    ActionRequest() : action(WorkAction::Assign) {}
};

/** @brief What happened as a result of an action: the state change and the routing. */
struct ActionOutcome {
    std::string workItemId;
    std::string previousState;
    std::string newState;
    std::string commandDescription;         ///< Command::describe()
    std::string routingDescription;         ///< RoutingDecision::description, "" if no routing ran
    std::vector<std::string> madeAvailable; ///< ProgressionResult::madeAvailable
    std::vector<std::string> nowWaiting;    ///< ProgressionResult::waiting
    std::vector<std::string> reopened;      ///< ProgressionResult::reopened
    bool instanceComplete;

    ActionOutcome() : instanceComplete(false) {}
};

/**
 * @brief One approval request in an instance's ApprovalDesk (Wrath's Chain of Responsibility).
 *
 * Shape is provisional until Wrath's ApprovalDesk API is published.
 */
struct ApprovalView {
    std::string id;           ///< e.g. "APR-1"; what the user types after approve/reject
    std::string workItemId;   ///< the work item waiting on this decision
    std::string summary;      ///< what is being approved, in words
    std::string approverRole; ///< role that may decide
    std::string status;       ///< "Pending", "Approved" or "Rejected"
    std::string decidedBy;    ///< participant name once decided
    std::string reason;       ///< rejection reason, if rejected
};

/**
 * @brief Abstract engine seam used by the interactive interface.
 *
 * Implemented by the real engine gateway (shipped) and by FakeEngine in tests/lust
 * (development and tests only).
 */
class EngineGateway {
public:
    virtual ~EngineGateway() {}

    /** @brief Every industry the engine has registered. */
    virtual std::vector<IndustryInfo> listIndustries() const = 0;

    /** @brief Values to ask for before starting @p definitionName. May be empty. */
    virtual std::vector<DataField> startFields(const std::string& industryId,
                                               const std::string& definitionName) const = 0;

    /**
     * @brief Creates and starts an instance of a definition.
     * @param initialData values for the fields from startFields(), already validated
     * @return the new instance's ID
     */
    virtual std::string startInstance(const std::string& industryId,
                                      const std::string& definitionName,
                                      const std::map<std::string, std::string>& initialData) = 0;

    /** @brief All instances of one industry, oldest first. */
    virtual std::vector<InstanceView> listInstances(const std::string& industryId) const = 0;
    virtual InstanceView getInstance(const std::string& instanceId) const = 0;

    /** @brief Every work item in the instance, in definition order. */
    virtual std::vector<WorkItemView> listWorkItems(const std::string& instanceId) const = 0;
    /** @brief Items in the tracker's available set, in the order they became available. */
    virtual std::vector<WorkItemView> listAvailableWork(const std::string& instanceId) const = 0;
    virtual WorkItemView getWorkItem(const std::string& instanceId,
                                     const std::string& workItemId) const = 0;

    virtual std::vector<ParticipantView> listParticipants(const std::string& industryId) const = 0;

    /** @brief Performs one action. Throws (and changes nothing) if the engine refuses it. */
    virtual ActionOutcome act(const ActionRequest& request) = 0;

    /** @brief History lines, oldest first: commands, routing decisions, state changes. */
    virtual std::vector<std::string> history(const std::string& instanceId) const = 0;

    /** @brief ProgressReportVisitor::summary(). */
    virtual std::string progressSummary(const std::string& instanceId) const = 0;
    /** @brief AuditReportVisitor::lines(). */
    virtual std::vector<std::string> auditReport(const std::string& instanceId) const = 0;
    /** @brief OverdueWorkVisitor::overdueNames(). */
    virtual std::vector<std::string> overdueWork(const std::string& instanceId) const = 0;

    /** @brief Every approval request of the instance (pending and decided), oldest first. */
    virtual std::vector<ApprovalView> listApprovals(const std::string& instanceId) const = 0;

    /** @brief The approval chain approves @p approvalId on behalf of @p actorId. */
    virtual ActionOutcome approve(const std::string& instanceId, const std::string& approvalId,
                                  const std::string& actorId) = 0;

    /** @brief The approval chain rejects @p approvalId. @p reason is required. */
    virtual ActionOutcome rejectApproval(const std::string& instanceId, const std::string& approvalId,
                                         const std::string& actorId, const std::string& reason) = 0;

    /**
     * @brief Moves the engine clock forward (ManualClock::advance). Work whose deadline has
     *        passed reacts as the engine decides (escalation).
     * @return one line per thing that changed, for display
     */
    virtual std::vector<std::string> advanceClock(int units) = 0;

    /** @brief The engine clock's current time, in the same units. */
    virtual int currentTime() const = 0;

    // ---- added for the real engine (Observer events and Memento undo) ----
    // Defaults refuse, so an engine without these (FakeEngine) still builds.

    /** @brief Domain events the industry reacts to, e.g. "WORM_SIGN". */
    virtual std::vector<std::string> eventTypes(const std::string& industryId) const {
        (void)industryId;
        return std::vector<std::string>();
    }
    /**
     * @brief Raises a domain event on an instance; the industry's interrupt rules react.
     * @return one line per thing that changed
     */
    virtual std::vector<std::string> raiseEvent(const std::string& instanceId, const std::string& type,
                                                const std::string& description) {
        (void)instanceId; (void)type; (void)description;
        throw std::logic_error("This engine does not support events");
    }
    /** @brief Label of the action undo() would reverse, or "" if there is none. */
    virtual std::string nextUndo(const std::string& instanceId) const {
        (void)instanceId;
        return "";
    }
    /** @brief Reverses the instance's most recent action. @return what was undone. */
    virtual std::string undo(const std::string& instanceId) {
        (void)instanceId;
        throw std::logic_error("This engine does not support undo");
    }
};

/** @brief Display name of an action ("Assign", "Start", ...). */
inline const char* actionName(WorkAction action) {
    switch (action) {
    case WorkAction::Assign:   return "Assign";
    case WorkAction::Start:    return "Start";
    case WorkAction::Complete: return "Complete";
    case WorkAction::Reject:   return "Reject";
    case WorkAction::Cancel:   return "Cancel";
    case WorkAction::Escalate: return "Escalate";
    }
    return "?";
}

} // namespace ui
} // namespace tividy

#endif
