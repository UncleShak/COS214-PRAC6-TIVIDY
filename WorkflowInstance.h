#ifndef WORKFLOW_INSTANCE_H
#define WORKFLOW_INSTANCE_H

// Independent execution with its own data and progress

#include "WorkflowDefnition.h"
#include "InstanceSnapshot.h" // Memento

class WorkflowInstance {
    public:
        virtual ~WorkflowInstance() = default;

        // State management
        virtual void start() = 0;
        virtual bool isComplete() const = 0;

        // Memento pattern
        virtual InstanceSnapshot* createSnapshot() = 0;
        virtual void restore(InstanceSnapshot* snapshot) = 0;
};

#endif