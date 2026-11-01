#ifndef TIVIDY_DUNE_HARVEST_FACTORY_H
#define TIVIDY_DUNE_HARVEST_FACTORY_H

#include "../IndustryFactory.h"

/**
 * @brief ConcreteFactory: Dune spice harvesting (mining).
 * Products: DuneHarvestBuilder, the surveyor/crew chief/carryall pilot/operations manager crew,
 * and an example harvest run.
 */
class DuneHarvestFactory : public IndustryFactory {
public:
    std::string getWorkflowName() const override;
    std::string getDisplayName() const override;
    std::string getDescription() const override;
    std::string getSector() const override;
    std::string getIdPrefix() const override;
    std::unique_ptr<WorkflowBuilder> createBuilder() const override;
    std::vector<std::unique_ptr<Participant> > createParticipants() const override;
    std::unique_ptr<DataPayload> createInitialData(const std::string& instanceId) const override;
    std::vector<std::string> getEventTypes() const override;
};

#endif
