#ifndef TIVIDY_STAGE_H
#define TIVIDY_STAGE_H
#include "WorkflowComponent.h"
#include <vector>

// Owns its children. add() takes ownership on success; remove() deletes a child.
// Never add a child already owned by another stage.
class Stage : public WorkflowComponent {
private:
    std::string name;
    std::vector<WorkflowComponent*> children;
    Stage(const Stage&);
    Stage& operator=(const Stage&);
public:
    explicit Stage(const std::string& name);
    ~Stage();
    std::string getName() const override;
    void request() override;
    void add(WorkflowComponent* child) override;
    void remove(WorkflowComponent* child) override;
    void accept(WorkflowVisitor* visitor) override;
    const std::vector<WorkflowComponent*>& getChildren() const;
};
#endif
