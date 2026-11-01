#ifndef WORKFLOW_ENGINE_H
#define WORKFLOW_ENGINE_H

// Facade: the one place a client (the interface, a test, a demo script) drives the engine.
// Behind it: Builder products (definitions), instances (Composite + State + Command),
// ProgressionTracker (routing), AssignmentRule (Strategy), Connector (Adapter),
// observers, and the Memento caretaker that makes every action undoable.
//
// Everything crosses this boundary by name or id. Actions throw std::exception subclasses
// whose what() is readable by a user: std::invalid_argument for unknown names,
// std::logic_error (incl. WorkflowException) for illegal moves, std::runtime_error for
// external-system failures. A failed action changes nothing.

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "WorkflowInstance.h"
#include "HistoryManager.h"
#include "Clock.h"

class Participant;
class IndustryFactory;
class WorkflowDefinition;
class DataPayload;

class WorkflowEngine {
private:
    std::map<std::string, std::unique_ptr<WorkflowDefinition> > definitions;
    std::vector<std::unique_ptr<Participant> > participants;
    std::map<std::string, std::unique_ptr<WorkflowInstance> > activeInstances;
    std::vector<std::string> instanceOrder;
    std::map<std::string, std::unique_ptr<HistoryManager> > caretakers;
    int nextInstanceNumber;
    ManualClock clock;   // engine time: deadlines (Decorator) and approval time limits

    WorkflowEngine(const WorkflowEngine&);
    WorkflowEngine& operator=(const WorkflowEngine&);

    WorkflowInstance& requireInstance(const std::string& instanceId) const;
    Participant* requireParticipant(const std::string& participantId) const;
    // Snapshot, act, and keep the snapshot for undo; on failure roll back and rethrow.
    void perform(const std::string& instanceId, const std::string& label,
                 const std::function<void(WorkflowInstance&)>& action);
    // Workload = work a participant holds in Assigned or Started state, across all instances.
    void recountWorkloads();

public:
    WorkflowEngine();
    ~WorkflowEngine();

    // ---- definitions (Builder products) ----
    // Takes ownership. Its name is its root stage's name. @throws std::invalid_argument on duplicates.
    std::string deployWorkflow(std::unique_ptr<WorkflowDefinition> definition);
    std::vector<std::string> listDefinitions() const;
    const WorkflowDefinition* getDefinition(const std::string& name) const;

    // Abstract Factory client: deploys the industry's workflow (via EngineConfigurator) and adds
    // its participants. @return the workflow name.
    std::string installIndustry(const IndustryFactory& industry);

    // ---- participants ----
    Participant* addParticipant(const std::string& name, const std::string& id, const std::vector<std::string>& roles);
    Participant* addParticipant(std::unique_ptr<Participant> participant);
    Participant* getParticipant(const std::string& id) const;   // null if unknown
    std::vector<Participant*> getParticipants() const;

    // ---- instances ----
    // An empty id generates "<definition>-<n>". data is copied (null = no data).
    WorkflowInstance* startInstance(const std::string& instanceId, const std::string& definitionName, const DataPayload* data);
    WorkflowInstance* getInstance(const std::string& instanceId) const;   // null if unknown
    std::vector<std::string> listInstances() const;
    std::vector<std::string> getAvailableWork(const std::string& instanceId) const;

    // ---- actions (each one is undoable) ----
    // The assignee must hold the item's required role.
    void assignWork(const std::string& instanceId, const std::string& workItem,
                    const std::string& actorId, const std::string& assigneeId);
    // Uses the definition's AssignmentRule (Strategy). @return the chosen participant's id.
    std::string autoAssign(const std::string& instanceId, const std::string& workItem, const std::string& actorId);
    void startWork(const std::string& instanceId, const std::string& workItem, const std::string& actorId);
    void completeWorkItem(const std::string& instanceId, const std::string& workItem, const std::string& actorId);
    void rejectWork(const std::string& instanceId, const std::string& workItem,
                    const std::string& actorId, const std::string& reason);
    void cancelWork(const std::string& instanceId, const std::string& workItem, const std::string& actorId);
    void escalateWork(const std::string& instanceId, const std::string& workItem, const std::string& actorId);
    void setData(const std::string& instanceId, const std::string& key, const std::string& value);
    // Observer: raise a domain event on an instance (e.g. "WORM_SIGN"); interrupt rules react.
    void raiseEvent(const std::string& instanceId, const std::string& type, const std::string& description);

    // ---- approvals (Chain of Responsibility) ----
    void approve(const std::string& instanceId, int requestId, const std::string& actorId, const std::string& note = "");
    void rejectApproval(const std::string& instanceId, int requestId, const std::string& actorId, const std::string& reason);

    // ---- time ----
    // Moves the clock and lets every instance react (escalations). @return one line per change.
    std::vector<std::string> advanceClock(int units);
    int currentTime() const;

    // ---- undo (Memento) ----
    bool canUndo(const std::string& instanceId) const;
    std::string nextUndoLabel(const std::string& instanceId) const;
    // @return the label of the action that was undone.
    std::string undo(const std::string& instanceId);
};

#endif
