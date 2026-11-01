#ifndef WORKFLOW_INSTANCE_H
#define WORKFLOW_INSTANCE_H

// Independent execution with its own data and progress.
//
// Patterns it takes part in:
//  - Prototype: the constructor clones the definition's tree, so instances never share WorkItems.
//  - Command (Invoker): every action is a Command run through a CommandLog.
//  - Observer: Subject for the UI, history and interrupt listeners; it forwards every
//    WorkItem state change and every domain event raised on it.
//  - Memento (Originator): createSnapshot() / restore().
//  - Visitor (ObjectStructure): accept() walks the instance's own tree.
//  - Proxy: each participant's command acts through an AccessControlProxy around the item,
//    using the definition's ActionAccessPolicy. Automatic actions (interrupts) are not guarded.
//  - Decorator: items with a DecorationRule complete through AuditDecorator / DeadlineDecorator,
//    built when the item becomes available. Completion goes Proxy -> Audit -> Deadline -> item.
//  - Chain of Responsibility: items with an ApprovalRule are decided by an ApprovalDesk over the
//    definition's approval chain. Starting the item submits the request.

#include <map>
#include <memory>
#include <string>
#include <vector>
#include "WorkflowDefinition.h"
#include "InstanceSnapshot.h" // Memento
#include "DataPayload.h"
#include "EventSource.h"
#include "EventListener.h"
#include "CommandLog.h"
#include "InstanceHistory.h"
#include "Participant.h"
#include "ProgressionTracker.h"
#include "ApprovalDesk.h"
#include "ApprovalRequest.h"
#include "Clock.h"
#include "TaskDecorator.h"

class WorkItem;
class Command;
class WorkflowVisitor;
class InterruptListener;
class AccessControlProxy;

class DeadlineDecorator;

class WorkflowInstance : public EventSource, public EventListener {
    private:
        std::string instanceId;
        const WorkflowDefinition* definition;            // not owned; the engine keeps it alive
        WorkflowComponent* root;                         // owned: this instance's own copy of the tree
        std::map<std::string, WorkItem*> items;          // by name, into root
        std::vector<std::string> itemOrder;              // tree order, for display
        std::unique_ptr<DataPayload> payload;
        std::unique_ptr<ProgressionTracker> tracker;
        CommandLog commandLog;
        // Proxies the logged commands acted through; kept as long as the commands are.
        std::vector<std::unique_ptr<AccessControlProxy> > guards;
        InstanceHistory history;
        Participant system;                              // actor for automatic actions (interrupts)
        std::vector<std::unique_ptr<InterruptListener> > interruptListeners;
        bool started;

        const Clock* clock;                              // the engine's clock (or ownClock)
        std::unique_ptr<ManualClock> ownClock;
        std::unique_ptr<ApprovalDesk> approvalDesk;      // null if the definition has no approvals

        struct Decorated {
            std::vector<std::unique_ptr<TaskDecorator> > layers;
            DeadlineDecorator* deadline;
            WorkflowComponent* outer;
            Decorated() : deadline(nullptr), outer(nullptr) {}
        };
        std::map<std::string, std::unique_ptr<Decorated> > decorated;
        // Replaced decorators stay alive: logged commands and proxies may still point at them.
        std::vector<std::unique_ptr<Decorated> > retired;

        ProgressionResult lastProgression;
        bool routedLast;

        WorkflowInstance(const WorkflowInstance&);
        WorkflowInstance& operator=(const WorkflowInstance&);

        void requireStarted() const;
        void runCommand(Command* command);
        // A protection proxy around item for actor (owned by this instance).
        WorkflowComponent* guard(WorkItem* item, Participant* actor);
        void apply(const ProgressionResult& result);
        void beginAction();
        // makeAvailable() and (re)build the item's decorators.
        void open(const std::string& name);
        void decorate(const std::string& name);
        WorkflowComponent* workOf(WorkItem* item) const;   // outermost decorator, or the item
        void setField(const std::string& key, const std::string& value);
        void recordOutcome(const std::string& item, const std::string& outcome);
        void recordDropped(const ProgressionResult& result);
        void submitApproval(const std::string& name, Participant* requester);
        const ApprovalRequest* latestApproval(const std::string& name) const;
        void finishApproved(const std::string& name);
        void finishRejected(const std::string& name, Participant* decider, const std::string& reason);

    public:
        // @param def must have a routing rule (built by a ScenarioBuilder); not owned.
        // @param initialData copied; null means an empty KeyValuePayload.
        // @param engineClock not owned; null gives the instance its own ManualClock at 0.
        WorkflowInstance(const std::string& id, const WorkflowDefinition* def, const DataPayload* initialData,
                         const Clock* engineClock = nullptr);
        virtual ~WorkflowInstance();

        std::string getId() const;
        const WorkflowDefinition* getDefinition() const;
        const DataPayload* getPayload() const;
        // The payload's fields if it is a KeyValuePayload, otherwise empty.
        std::map<std::string, std::string> getDataFields() const;
        // @throws std::logic_error if the payload is not a KeyValuePayload.
        void setData(const std::string& key, const std::string& value);

        // State management
        virtual void start();
        bool isStarted() const;
        bool isComplete() const;
        bool isStalled() const;

        // Work items
        bool hasItem(const std::string& name) const;
        // @throws std::invalid_argument for an unknown name.
        WorkItem* getItem(const std::string& name) const;
        std::vector<WorkItem*> getItems() const;
        std::vector<std::string> getAvailableWork() const;
        const ProgressionTracker& getProgress() const;

        // Actions. Each runs a Command; the State pattern refuses illegal moves (std::logic_error).
        void assignWork(const std::string& item, Participant* actor, Participant* assignee);
        void startWork(const std::string& item, Participant* actor);
        // Calls the item's Connector (Adapter) first, if it has one; its response becomes the payload.
        void completeWork(const std::string& item, Participant* actor);
        void rejectWork(const std::string& item, Participant* actor, const std::string& reason);
        void cancelWork(const std::string& item, Participant* actor);
        void escalateWork(const std::string& item, Participant* actor);

        // Chain of Responsibility. Ids are the ApprovalDesk's request ids.
        // @throws std::invalid_argument for an unknown id, AccessDenied if actor may not decide.
        void approve(int requestId, Participant* actor, const std::string& note);
        void rejectApproval(int requestId, Participant* actor, const std::string& reason);
        std::vector<const ApprovalRequest*> getApprovals() const;
        bool needsApproval(const std::string& item) const;

        // Clock moved: escalates overdue approvals and overdue available work.
        // @return one line per change.
        std::vector<std::string> onClockAdvanced();
        // Deadline decorator due time on the clock, or -1.
        int getDueTime(const std::string& item) const;

        // What the most recent action routed, if it routed anything.
        bool routedLastAction() const { return routedLast; }
        const ProgressionResult& getLastProgression() const { return lastProgression; }

        // Observer: raise a domain event (e.g. WORM_SIGN) to every listener on this instance.
        void raise(const Event& event);
        // Called by InterruptListener.
        void interrupt(const WorkflowDefinition::InterruptRule& rule, const std::string& reason);
        // Forwards WorkItem state changes to this instance's listeners.
        void onEvent(const Event& event) override;

        const InstanceHistory& getHistory() const;
        std::vector<std::string> getCommandLog() const;

        // Visitor
        void accept(WorkflowVisitor* visitor);

        // Memento pattern
        virtual InstanceSnapshot* createSnapshot() const;
        virtual void restore(const InstanceSnapshot* snapshot);
};

#endif
