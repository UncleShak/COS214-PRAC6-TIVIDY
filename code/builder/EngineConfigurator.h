#ifndef TIVIDY_ENGINE_CONFIGURATOR_H
#define TIVIDY_ENGINE_CONFIGURATOR_H
#include "WorkflowBuilder.h"
#include "../workflow/WorkflowDefinition.h"
/** @brief Director: knows construction order, never industry names or rules. */
class EngineConfigurator {
public:
    std::unique_ptr<WorkflowDefinition> constructScenario(WorkflowBuilder& builder) const {
        builder.reset();
        builder.buildStructure();
        builder.buildRules();
        builder.buildConnectors();
        return builder.getResult();
    }
};
#endif
