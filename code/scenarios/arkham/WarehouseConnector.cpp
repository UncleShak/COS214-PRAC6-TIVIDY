#include "WarehouseConnector.h"
#include "../../data/KeyValuePayload.h"
#include <limits>
#include <stdexcept>
namespace {
std::string required(const KeyValuePayload& p, const std::string& key) {
    if (!p.has(key) || p.get(key).empty())
        throw std::invalid_argument("Warehouse request needs " + key);
    return p.get(key);
}
int quantity(const std::string& text) {
    int n = 0;
    for (char c : text) {
        if (c < '0' || c > '9' || n > (std::numeric_limits<int>::max() - (c - '0')) / 10)
            throw std::invalid_argument("quantity must be a positive integer within int range");
        n = n * 10 + (c - '0');
    }
    if (n == 0) throw std::invalid_argument("quantity must be positive");
    return n;
}
}
WarehouseConnector::WarehouseConnector(std::shared_ptr<WarehouseAPI> api) : service(api) {
    if (!service) throw std::invalid_argument("WarehouseConnector needs a service");
}
bool WarehouseConnector::isAvailable() const { return service->ping(); }
std::unique_ptr<DataPayload> WarehouseConnector::invoke(const std::string& operation,
                                                       const DataPayload& data) {
    if (operation != "requestStock") throw std::invalid_argument("Unsupported warehouse operation: " + operation);
    const auto* fields = dynamic_cast<const KeyValuePayload*>(&data);
    if (!fields) throw std::invalid_argument("WarehouseConnector needs KeyValuePayload");
    const std::string order = required(*fields, "orderId");
    const std::string sku = required(*fields, "sku");
    const int units = quantity(required(*fields, "quantity"));
    if (!isAvailable()) throw std::runtime_error("Warehouse unavailable");
    const WarehouseAPI::Reply reply = service->reserve(order, sku, units);
    std::unique_ptr<KeyValuePayload> result(new KeyValuePayload(*fields));
    switch (reply.status) {
    case WarehouseAPI::Reserved:
        result->set("stockRequestStatus", "reserved");
        result->set("stockReceived", "yes");
        break;
    case WarehouseAPI::OutOfStock:
        result->set("stockRequestStatus", "outOfStock");
        result->set("stockReceived", "no");
        break;
    case WarehouseAPI::Conflict:
        result->set("stockRequestStatus", "conflict");
        result->set("stockReceived", "no");
        break;
    default: throw std::runtime_error("Unknown warehouse response");
    }
    result->set("reservationReference", reply.reference);
    return std::unique_ptr<DataPayload>(result.release());
}
