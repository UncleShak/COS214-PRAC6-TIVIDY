/**
 * @file FakeEngine.h
 * @brief DEVELOPMENT AND TESTS ONLY. A small stand-in for the real engine.
 *
 * Never linked into the shipped `tividy` program and never used in the demo. It lets the
 * interface be built and unit-tested before the real engine runs.
 *
 * It copies the real engine's observable rules so a swap does not surprise the UI:
 * - state names and transitions of src/workflow/lifecycle (Created, Available, Assigned, Started,
 *   Completed, Rejected, Cancelled, Escalated; illegal ones throw std::logic_error);
 * - Command::requireAssignee (only the assignee may start or complete);
 * - routing descriptions in SequentialRouting's format, and a decision branch like
 *   DataBasedRouting (std::runtime_error when nothing matches and there is no default);
 * - a ProgressionTracker-style AND-join (an item reached early waits for its prerequisites);
 * - std::invalid_argument for unknown IDs; a refused call changes nothing.
 *
 * Industries are deliberately named "Fake Industry A/B" so they cannot pass for demo data.
 *
 * Workflow "Fake Order Flow":
 * @code
 *   Receive -> { Validate, Check Stock }          parallel split
 *   Validate: amount > 1000 -> Approve            decision
 *             default        -> Pack
 *   Approve -> Pack                               needs a Manager's approval (APR-n)
 *   Pack waits for Check Stock                    dependency (AND-join)
 *   Pack -> Ship
 * @endcode
 *
 * Approvals and deadlines are a GUESS at Wrath's ApprovalDesk and the DeadlineDecorator
 * until their APIs are published: reaching Approve opens a pending approval that only a
 * Manager may decide; approving completes Approve and routes on, rejecting rejects it.
 * Every item gets a deadline kDeadline units after it becomes available; advancing the
 * clock past it escalates the item if it is still Available.
 *
 * @author Shakir Alexander (Lust)
 */
#ifndef TIVIDY_FAKE_ENGINE_H
#define TIVIDY_FAKE_ENGINE_H

#include <map>
#include <set>
#include <string>
#include <vector>

#include "EngineGateway.h"

namespace tividy {
namespace ui {

/** @brief In-memory EngineGateway with one hard-coded workflow. Tests only. */
class FakeEngine : public EngineGateway {
public:
    FakeEngine();

    /** @brief Test hook: removes Validate's default branch, so a low amount has no route. */
    void removeDecisionDefault() { decisionHasDefault_ = false; }

    std::vector<IndustryInfo> listIndustries() const override;
    std::vector<DataField> startFields(const std::string& industryId,
                                       const std::string& definitionName) const override;
    std::string startInstance(const std::string& industryId, const std::string& definitionName,
                              const std::map<std::string, std::string>& initialData) override;
    std::vector<InstanceView> listInstances(const std::string& industryId) const override;
    InstanceView getInstance(const std::string& instanceId) const override;
    std::vector<WorkItemView> listWorkItems(const std::string& instanceId) const override;
    std::vector<WorkItemView> listAvailableWork(const std::string& instanceId) const override;
    WorkItemView getWorkItem(const std::string& instanceId,
                             const std::string& workItemId) const override;
    std::vector<ParticipantView> listParticipants(const std::string& industryId) const override;
    ActionOutcome act(const ActionRequest& request) override;
    std::vector<std::string> history(const std::string& instanceId) const override;
    std::string progressSummary(const std::string& instanceId) const override;
    std::vector<std::string> auditReport(const std::string& instanceId) const override;
    std::vector<std::string> overdueWork(const std::string& instanceId) const override;
    std::vector<ApprovalView> listApprovals(const std::string& instanceId) const override;
    ActionOutcome approve(const std::string& instanceId, const std::string& approvalId,
                          const std::string& actorId) override;
    ActionOutcome rejectApproval(const std::string& instanceId, const std::string& approvalId,
                                 const std::string& actorId, const std::string& reason) override;
    std::vector<std::string> advanceClock(int units) override;
    int currentTime() const override { return clock_; }

    /** @brief Units of time an item may stay Available before it is escalated. */
    static const int kDeadline = 5;

private:
    struct Item {
        std::string name;
        std::string role;
        std::string state;
        std::string trackerStatus;
        std::string assigneeId;
        int dueAt;

        Item() : dueAt(-1) {}
    };

    struct Instance {
        std::string id;
        std::string industryId;
        std::map<std::string, std::string> data;
        std::vector<Item> items;              // definition order
        std::vector<std::string> available;   // order they became available
        std::vector<std::string> history;
        std::vector<ApprovalView> approvals;
        std::vector<std::string> notes;       // history lines waiting for the current action's line
    };

    struct Routing {
        std::string description;
        std::vector<std::string> next;
    };

    const Instance& instance(const std::string& instanceId) const;
    Instance& instance(const std::string& instanceId);
    static Item* findItem(Instance& inst, const std::string& name);
    static const Item* findItem(const Instance& inst, const std::string& name);
    std::vector<ParticipantView>& participantsOf(const std::string& industryId);
    const std::vector<ParticipantView>& participantsOf(const std::string& industryId) const;
    ParticipantView* findParticipant(const std::string& industryId, const std::string& id);
    std::string nameOf(const std::string& industryId, const std::string& participantId) const;

    Routing route(const Instance& inst, const std::string& completed) const;
    std::vector<std::string> unmetPrerequisites(const Instance& inst, const std::string& item) const;
    void reach(Instance& inst, const std::string& item, ActionOutcome& outcome);
    void releaseWaiting(Instance& inst, ActionOutcome& outcome);
    void makeAvailable(Instance& inst, Item& item, ActionOutcome& outcome);
    /** Completes @p item, routes onward and releases waiting work. Routing may throw first. */
    void completeAndRoute(Instance& inst, Item& item, ActionOutcome& outcome);
    ActionOutcome decide(const std::string& instanceId, const std::string& approvalId,
                         const std::string& actorId, bool approved, const std::string& reason);
    /** Writes the outcome to the history, then commits the copies back. */
    void commit(Instance& inst, const std::vector<ParticipantView>& people, ActionOutcome& outcome);
    static void releaseAssignee(const Item& item, std::vector<ParticipantView>& people);
    static void removeAvailable(Instance& inst, const std::string& item);
    static bool isComplete(const Instance& inst);
    static bool isStalled(const Instance& inst);
    WorkItemView toView(const Instance& inst, const Item& item) const;

    std::map<std::string, std::vector<ParticipantView> > participants_;
    std::vector<Instance> instances_;
    std::map<std::string, std::set<std::string> > prerequisites_;
    int nextId_;
    int nextApproval_;
    int clock_;
    bool decisionHasDefault_;
};

} // namespace ui
} // namespace tividy

#endif
