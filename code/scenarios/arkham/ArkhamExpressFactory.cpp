#include "ArkhamExpressFactory.h"
#include "ArkhamExpressBuilder.h"
#include "WarehouseConnector.h"

namespace {
std::unique_ptr<Participant> person(const std::string& name, const std::string& id,
                                    const std::vector<std::string>& roles) {
    std::unique_ptr<Participant> p(new Participant(name, id));
    for (std::size_t i = 0; i < roles.size(); ++i) p->addRole(Role(roles[i]));
    return p;
}
}

ArkhamExpressFactory::ArkhamExpressFactory(std::shared_ptr<WarehouseStub> service) : warehouse(service) {
    if (!warehouse) {
        warehouse.reset(new WarehouseStub());
        warehouse->setStock("AX-NOTEBOOK", 20);
    }
}

std::string ArkhamExpressFactory::getWorkflowName() const { return "ArkhamExpress"; }
std::string ArkhamExpressFactory::getDisplayName() const { return "Arkham Express"; }
std::string ArkhamExpressFactory::getDescription() const {
    return "Logistics: one customer order from intake to delivery, with a warehouse integration.";
}

std::string ArkhamExpressFactory::getIdPrefix() const { return "AX"; }

std::unique_ptr<WorkflowBuilder> ArkhamExpressFactory::createBuilder() const {
    return std::unique_ptr<WorkflowBuilder>(
        new ArkhamExpressBuilder(std::shared_ptr<Connector>(new WarehouseConnector(warehouse))));
}

std::vector<std::unique_ptr<Participant> > ArkhamExpressFactory::createParticipants() const {
    std::vector<std::unique_ptr<Participant> > team;
    team.push_back(person("Ada Clerkwell", "ax-clerk", {"Clerk"}));
    team.push_back(person("Wren Stockley", "ax-wh", {"Warehouse"}));
    team.push_back(person("Dorian Vale", "ax-dm", {"DeliveryManager", "Manager"}));
    team.push_back(person("Rook Calder", "ax-drv", {"Driver"}));
    return team;
}

std::unique_ptr<DataPayload> ArkhamExpressFactory::createInitialData(const std::string& instanceId) const {
    return std::unique_ptr<DataPayload>(new KeyValuePayload(ArkhamExpressBuilder::exampleOrder(instanceId)));
}

std::vector<std::string> ArkhamExpressFactory::getEventTypes() const {
    return std::vector<std::string>(1, "CUSTOMER_CANCELLED");
}

std::shared_ptr<WarehouseStub> ArkhamExpressFactory::getWarehouse() const { return warehouse; }
std::string ArkhamExpressFactory::getSector() const { return "Logistics"; }
