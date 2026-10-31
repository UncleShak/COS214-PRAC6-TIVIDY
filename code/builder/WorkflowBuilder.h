#ifndef TIVIDY_WORKFLOW_BUILDER_H
#define TIVIDY_WORKFLOW_BUILDER_H
#include <memory>
class WorkflowDefinition;
/** @brief GoF Builder: ordered construction phases shared by every industry. */
class WorkflowBuilder {
public:
    virtual ~WorkflowBuilder() = default;
    virtual void reset() = 0;
    virtual void buildStructure() = 0;
    virtual void buildRules() = 0;
    virtual void buildConnectors() = 0;
    /** @brief Validates and transfers ownership; repeat calls without reset are refused. */
    virtual std::unique_ptr<WorkflowDefinition> getResult() = 0;
};
#endif
