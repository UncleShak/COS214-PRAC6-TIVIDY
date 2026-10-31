#ifndef TIVIDY_ARKHAM_EXPRESS_FACTORY_H
#define TIVIDY_ARKHAM_EXPRESS_FACTORY_H

#include <memory>
#include "../IndustryFactory.h"
#include "WarehouseAPI.h"

/**
 * @brief ConcreteFactory: Arkham Express order fulfilment and delivery.
 * Products: ArkhamExpressBuilder wired to a WarehouseConnector (Adapter) over this factory's
 * warehouse, the clerk/warehouse/delivery/driver team, and an example order.
 */
class ArkhamExpressFactory : public IndustryFactory {
public:
    /** @param warehouse the external service; null makes a stub stocked with 20 AX-NOTEBOOK. */
    explicit ArkhamExpressFactory(std::shared_ptr<WarehouseStub> warehouse = std::shared_ptr<WarehouseStub>());

    std::string getWorkflowName() const override;
    std::string getDisplayName() const override;
    std::string getDescription() const override;
    std::string getIdPrefix() const override;
    std::unique_ptr<WorkflowBuilder> createBuilder() const override;
    std::vector<std::unique_ptr<Participant> > createParticipants() const override;
    std::unique_ptr<DataPayload> createInitialData(const std::string& instanceId) const override;
    std::vector<std::string> getEventTypes() const override;

    /** @brief The warehouse the connector talks to (for demos: restock, take offline). */
    std::shared_ptr<WarehouseStub> getWarehouse() const;

private:
    std::shared_ptr<WarehouseStub> warehouse;
};

#endif
