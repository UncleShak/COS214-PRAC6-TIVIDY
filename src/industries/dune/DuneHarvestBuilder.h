#ifndef TIVIDY_DUNE_HARVEST_BUILDER_H
#define TIVIDY_DUNE_HARVEST_BUILDER_H

#include "ScenarioBuilder.h"
#include "KeyValuePayload.h"

/**
 * @brief ConcreteBuilder: one spice harvest run on a field of Arrakis (industry: mining).
 *
 * SurveyField -> ApproveHarvestPermit (decided by the approval chain on estimatedYieldTonnes;
 *                                       sets permitGranted) -> (permitGranted = yes)
 *     -> DeployHarvester || DeployCarryall (parallel) -> Harvest (joins both) -> TransportToRefinery
 *   (otherwise) -> AbandonRun
 *
 * WORM_SIGN (Observer interrupt): cancels the deployment and the harvest if they are still open,
 * and opens Evacuate, which must be done within 3 clock units (DeadlineDecorator). Harvest is
 * audited (AuditDecorator). Only an OperationsManager may cancel work by hand (Proxy policy).
 */
class DuneHarvestBuilder : public ScenarioBuilder {
public:
    DuneHarvestBuilder();
    void buildStructure() override;
    void buildRules() override;
    void buildConnectors() override;

    /** @brief Starting data for one run: the field, its survey estimate and the permit decision. */
    static KeyValuePayload exampleRun(const std::string& runId);
};

#endif
