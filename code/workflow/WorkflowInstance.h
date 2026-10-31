#ifndef WORKFLOW_INSTANCE_H
#define WORKFLOW_INSTANCE_H

// Independent execution with its own data and progress

#include "WorkflowDefinition.h"
#include "InstanceSnapshot.h" // Memento
#include "../progression/ProgressionTracker.h"

class WorkflowInstance {
    private:
        WorkflowDefinition* definition;
        WorkflowComponent* root;
        DataPayload* payload;
        ProgressionTracker* progress;

         // Points to tasks owned by the root tree.
    std::map<std::string, WorkItem*> items;

    // Prevent copying owned pointers.
    WorkflowInstance(const WorkflowInstance&);
    WorkflowInstance& operator=(const WorkflowInstance&);

    public:
        WorkflowInstance(WorkflowDefinition* def, DataPayload* data);
        virtual ~WorkflowInstance();

        // State management
        virtual void start();
        virtual bool isComplete() const;
        bool isStalled() const;

    WorkItem* getWorkItem(const std::string& name) const;
    const DataPayload* getPayload() const;
    const std::vector<std::string>& availableItems() const;

    void assignWork(
        const std::string& name,
        Participant* person
    );

    void startWork(
        const std::string& name,
        Participant* actor
    );

    void completeWork(
        const std::string& name,
        Participant* actor
    );

        // Memento pattern
        virtual InstanceSnapshot* createSnapshot() = 0;
        virtual void restore(InstanceSnapshot* snapshot) = 0;
};

#endif