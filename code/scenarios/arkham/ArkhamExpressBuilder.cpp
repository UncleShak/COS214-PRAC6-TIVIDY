#include "ArkhamExpressBuilder.h"
#include "../../assignment/WorkloadBasedAssignment.h"
#include <stdexcept>
ArkhamExpressBuilder::ArkhamExpressBuilder(std::shared_ptr<Connector> connector)
    : ScenarioBuilder("ArkhamExpress", "1.0"), warehouse(connector) {
    if (!warehouse) throw std::invalid_argument("ArkhamExpress needs a warehouse connector");
}
void ArkhamExpressBuilder::buildStructure() {
    buildStage("OrderIntake");
    buildStage("Fulfilment");
    buildStage("Delivery");
    buildStage("Exceptions");
    buildWorkItem("ReceiveOrder", "Clerk", "OrderIntake");
    buildWorkItem("ValidateOrder", "Clerk", "OrderIntake");
    buildWorkItem("CheckStock", "Warehouse", "Fulfilment");
    buildWorkItem("RequestStock", "Warehouse", "Fulfilment");
    buildWorkItem("PreparePackage", "Warehouse", "Fulfilment");
    buildWorkItem("SelectDeliveryType", "DeliveryManager", "Delivery");
    buildWorkItem("AssignDriver", "DeliveryManager", "Delivery");
    buildWorkItem("Dispatch", "Driver", "Delivery");
    buildWorkItem("Deliver", "Driver", "Delivery");
    buildWorkItem("CloseOrder", "Clerk", "Delivery");
    buildWorkItem("CancelOrder", "DeliveryManager", "Exceptions");
    buildWorkItem("HandleStockFailure", "DeliveryManager", "Exceptions");
    buildWorkItem("ReturnToWarehouse", "Driver", "Exceptions");
}
void ArkhamExpressBuilder::buildRules() {
    buildEntry("ReceiveOrder");
    buildIdentityField("orderId");
    buildCancellationItem("CancelOrder");
    buildAssignmentRule(std::unique_ptr<AssignmentRule>(new WorkloadBasedAssignment()));
    buildPath("ReceiveOrder", "ValidateOrder");
    buildBranch("ValidateOrder", RoutingCondition::equals("orderValid", "yes"), {"CheckStock"});
    buildDefaultBranch("ValidateOrder", {"CancelOrder"});
    buildBranch("CheckStock", RoutingCondition::equals("inStock", "yes"), {"PreparePackage"});
    buildDefaultBranch("CheckStock", {"RequestStock"});
    buildBranch("RequestStock", RoutingCondition::equals("stockReceived", "yes"), {"PreparePackage"});
    buildDefaultBranch("RequestStock", {"HandleStockFailure"});
    buildPath("HandleStockFailure", "CancelOrder");
    buildPath("PreparePackage", "SelectDeliveryType");
    buildPath("PreparePackage", "AssignDriver");
    buildPath("SelectDeliveryType", "Dispatch");
    buildPath("AssignDriver", "Dispatch");
    buildPath("Dispatch", "Deliver");
    buildBranch("Deliver", RoutingCondition::equals("delivered", "yes"), {"CloseOrder"});
    buildDefaultBranch("Deliver", {"ReturnToWarehouse"});
    // A rejected step cancels the order (Bug 1). Without these, a rejection followed the
    // success path into a step that waits on the rejected one, and the order stalled.
    const char* const rejectable[] = {"ReceiveOrder", "ValidateOrder", "CheckStock", "PreparePackage",
                                      "SelectDeliveryType", "AssignDriver", "Dispatch", "Deliver"};
    for (const char* item : rejectable) buildRejectionPath(item, {"CancelOrder"});
    buildDependency("ValidateOrder", "ReceiveOrder");
    buildDependency("CheckStock", "ValidateOrder");
    buildDependency("RequestStock", "CheckStock");
    // CheckStock completes even when its result is 'no'. RequestStock is an
    // alternative path, so making it a mandatory prerequisite would deadlock local stock.
    buildDependency("PreparePackage", "CheckStock");
    buildDependency("SelectDeliveryType", "PreparePackage");
    buildDependency("AssignDriver", "PreparePackage");
    buildDependency("Dispatch", "PreparePackage");
    buildDependency("Dispatch", "SelectDeliveryType");
    buildDependency("Dispatch", "AssignDriver");
    buildDependency("Deliver", "Dispatch");
    buildDependency("CloseOrder", "Deliver");
    // Decorators: delivery has a deadline; the external stock request is audited.
    buildDeadline("Deliver", 10);
    buildAudit("RequestStock");
    // Proxy policy: only delivery management may cancel work in an order.
    buildAccessRule("cancel", {"DeliveryManager"});
    // Observer: the customer cancels mid-order. Open fulfilment work stops; CancelOrder opens.
    buildInterrupt("CUSTOMER_CANCELLED",
                   {"ReceiveOrder", "ValidateOrder", "CheckStock", "RequestStock", "PreparePackage",
                    "SelectDeliveryType", "AssignDriver", "Dispatch", "Deliver", "CloseOrder",
                    "HandleStockFailure", "ReturnToWarehouse"},
                   {"CancelOrder"});
}
void ArkhamExpressBuilder::buildConnectors() {
    buildConnector("RequestStock", warehouse, "requestStock");
}
KeyValuePayload ArkhamExpressBuilder::exampleOrder(const std::string& orderId) {
    if (orderId.empty()) throw std::invalid_argument("Order ID cannot be empty");
    KeyValuePayload data;
    data.set("orderId", orderId);
    data.set("sku", "AX-NOTEBOOK");
    data.set("quantity", "2");
    data.set("orderValid", "yes");
    data.set("inStock", "yes");
    data.set("stockReceived", "no");
    data.set("urgency", "standard");
    data.set("distanceKm", "25");
    data.set("delivered", "yes");
    return data;
}
