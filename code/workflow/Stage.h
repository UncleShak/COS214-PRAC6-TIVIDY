#ifndef STAGE_H
#define STAGE_H

// Composite node: a named group of stages and work items

#include <string>
#include <vector>
#include "WorkflowComponent.h"

class Stage : public WorkflowComponent {
    private:
        std::string stageName;
        std::vector<WorkflowComponent*> children;

        Stage(const Stage&);
        Stage& operator=(const Stage&);

    public:
        explicit Stage(const std::string& name);
        // Children are not owned: WorkflowDefinition frees the tree it was built into.
        virtual ~Stage() = default;

        std::string getName() const override;

        // Forwards the request to every child, in order.
        void request() override;

        // Composite interface. remove() hands ownership back to the caller.
        void add(WorkflowComponent* component) override;
        void remove(WorkflowComponent* component) override;
        const std::vector<WorkflowComponent*>& getChildren() const;

        // Visitor: visitStage(this), then each child in order.
        void accept(WorkflowVisitor* visitor) override;

        // Prototype: copies this stage and every child (the copy's children are new objects).
        WorkflowComponent* clone() const override;
};

// Deletes a component and, if it is a Stage, everything under it (stages do not own children).
void deleteComponentTree(WorkflowComponent* component);

#endif
