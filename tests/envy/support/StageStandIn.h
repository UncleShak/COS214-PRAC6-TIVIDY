/**
 * @file StageStandIn.h
 * @brief Test-only Stage (Composite), following the team's Prac 6 design (see support/README.md).
 *
 * accept() calls visitStage(this) and then passes the visitor to each child, so a whole
 * tree of stages and work items is visited. Children are not owned.
 */
#ifndef TIVIDY_TEST_STAGE_STAND_IN_H
#define TIVIDY_TEST_STAGE_STAND_IN_H

#include <algorithm>
#include <string>
#include <vector>

#include "WorkflowComponent.h"
#include "WorkflowVisitor.h"

class Stage : public WorkflowComponent {
public:
    explicit Stage(const std::string& stageName) : name(stageName) {}

    std::string getName() const override { return name; }
    void request() override {
        for (std::size_t i = 0; i < children.size(); ++i) children[i]->request();
    }
    void add(WorkflowComponent* c) override { children.push_back(c); }
    void remove(WorkflowComponent* c) override {
        children.erase(std::remove(children.begin(), children.end(), c), children.end());
    }
    void accept(WorkflowVisitor* visitor) override {
        visitor->visitStage(this);
        for (std::size_t i = 0; i < children.size(); ++i) children[i]->accept(visitor);
    }

private:
    std::string name;
    std::vector<WorkflowComponent*> children;
};

#endif
