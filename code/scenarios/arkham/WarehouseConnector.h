#ifndef TIVIDY_WAREHOUSE_CONNECTOR_H
#define TIVIDY_WAREHOUSE_CONNECTOR_H
#include "../../integration/Connector.h"
#include "WarehouseAPI.h"
/** @brief Object Adapter: translates KeyValuePayload to WarehouseAPI and back.
 * Owns a shared reference to the service; service lifetime outlasts each invocation.
 * Supported operation: requestStock. Required fields: orderId, sku, quantity.
 * Response copies all input fields and updates: stockRequestStatus, stockReceived, reservationReference.
 */
class WarehouseConnector : public Connector {
public:
    explicit WarehouseConnector(std::shared_ptr<WarehouseAPI> service);
    bool isAvailable() const override;
    std::unique_ptr<DataPayload> invoke(const std::string&, const DataPayload&) override;
private:
    std::shared_ptr<WarehouseAPI> service;
};
#endif
