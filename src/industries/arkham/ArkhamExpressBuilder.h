#ifndef TIVIDY_ARKHAM_EXPRESS_BUILDER_H
#define TIVIDY_ARKHAM_EXPRESS_BUILDER_H
#include "ScenarioBuilder.h"
#include "KeyValuePayload.h"
/** @brief ConcreteBuilder: ArkhamExpress order fulfilment and delivery.
 * Inject the warehouse adapter; the Director itself depends only on WorkflowBuilder.
 * Routing outcomes are data; business names never enter the generic engine.
 */
class ArkhamExpressBuilder : public ScenarioBuilder {
public:
    explicit ArkhamExpressBuilder(std::shared_ptr<Connector> warehouse);
    void buildStructure() override;
    void buildRules() override;
    void buildConnectors() override;
    /** @brief Independent realistic initial data for demonstrations/tests.
     * orderValid/inStock/delivered are demo inputs; real activities must set them.
     */
    static KeyValuePayload exampleOrder(const std::string& orderId);
private:
    std::shared_ptr<Connector> warehouse;
};
#endif
