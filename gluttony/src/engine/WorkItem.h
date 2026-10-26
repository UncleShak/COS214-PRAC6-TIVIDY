/**
 * @file WorkItem.h
 * @brief ConcreteComponent (Decorator) and Receiver (Command): one unit of work.
 *
 * STAND-IN for the engine's WorkItem (owned by Pride). In the real engine each receiver
 * operation delegates to the current WorkItemState (State pattern). Here a small status
 * enum enforces the same lifecycle so the commands and decorators can be built and tested
 * now. The receiver operations (assign, start, complete, reject, cancel) are the contract
 * the real WorkItem must keep (see docs/decisions-gluttony.md, D-G3 and D-G4).
 *
 * @author Tanatswa Masiiwa (Gluttony)
 * @note Implemented with assistance from Claude (Anthropic AI), from the team's Practical 6 UML.
 */
#ifndef TIVIDY_WORK_ITEM_H
#define TIVIDY_WORK_ITEM_H

#include "WorkflowComponent.h"

#include <functional>
#include <string>

class Participant;

/** @brief Lifecycle states of a work item (stand-in for the WorkItemState classes). */
enum class WorkItemStatus { Created, Available, Assigned, Started, Completed, Rejected, Cancelled };

/** @brief Returns the status as text, for messages and the interface. */
std::string statusName(WorkItemStatus status);

/** @brief A single unit of work in a workflow instance. */
class WorkItem : public WorkflowComponent {
public:
    /**
     * @param name Display name, for example "Verify documents".
     * @param work The action performed by request(). Defaults to doing nothing.
     */
    explicit WorkItem(const std::string& name,
                      std::function<void()> work = std::function<void()>());

    // ---- WorkflowComponent (Decorator: ConcreteComponent) ----
    void request() override;
    void proxyRequest() override;
    void accept(WorkflowVisitor* v) override;
    WorkflowIterator* createIterator() override;
    std::string getName() const override;

    // ---- Receiver operations (Command) ----
    /** @brief Created to Available: the work can now be assigned. */
    void makeAvailable();
    /** @brief Available to Assigned. @throws InvalidTransition if not Available or assignee is null. */
    void assign(Participant* assignee);
    /** @brief Assigned to Started. @throws InvalidTransition if the actor is not the assignee. */
    void start(Participant* actor);
    /** @brief Started to Completed. @throws InvalidTransition if the actor is not the assignee. */
    void complete(Participant* actor);
    /** @brief Assigned or Started to Rejected, recording the reason. */
    void reject(Participant* actor, const std::string& reason);
    /** @brief Any unfinished state to Cancelled. @throws InvalidTransition once finished. */
    void cancel(Participant* actor);

    WorkItemStatus getStatus() const { return status; }
    Participant* getAssignee() const { return assignee; }
    const std::string& getRejectionReason() const { return rejectionReason; }
    /** @brief How many times request() has run the work. */
    int getRequestCount() const { return requestCount; }

private:
    /** @brief Throws InvalidTransition naming the action and the current status. */
    void refuse(const std::string& action) const;
    /** @brief True once the item is Completed, Rejected or Cancelled. */
    bool isFinished() const;

    std::string name;
    std::function<void()> work;
    WorkItemStatus status;
    Participant* assignee;
    std::string rejectionReason;
    int requestCount;
};

#endif
