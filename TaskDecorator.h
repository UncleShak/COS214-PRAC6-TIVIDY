#ifndef TASKDECORATOR_H
#define TASKDECORATOR_H

#include "WorkflowComponent.h"
#include <string>

class TaskDecorator : public WorkflowComponent{
    protected:
        WorkflowComponent* wrappedComponent;

    public:
        explicit TaskDecorator(WorkflowComponent* component) : wrappedComponent(component) {}

        virtual ~TaskDecorator() {

        }

        // Concrete Decorators will override specific methods below
        virtual std::string getName() const override{
            if(wrappedComponent){
                return wrappedComponent->getName();
            }
            return "Unknown";
        }

        virtual void add(WorkflowComponent* component) override {
        if (wrappedComponent) {
            wrappedComponent->add(component);
        }
    }
    
    virtual void remove(WorkflowComponent* component) override {
        if (wrappedComponent) {
            wrappedComponent->remove(component);
        }
    }
    
    virtual void accept(WorkflowVisitor* visitor) override {
        if (wrappedComponent) {
            // Forward the visitor to the wrapped component so it can be traversed
            wrappedComponent->accept(visitor);
        }
    }

};

#endif
