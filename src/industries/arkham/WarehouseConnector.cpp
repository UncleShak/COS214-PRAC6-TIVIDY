#include "WarehouseConnector.h"
#include "KeyValuePayload.h"
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
    if (operation != "requestStock" && operation != "releaseStock") throw std::invalid_argument("Unsupported warehouse operation: " + operation);
    const auto* fields = dynamic_cast<const KeyValuePayload*>(&data);
    if (!fields) throw std::invalid_argument("WarehouseConnector needs KeyValuePayload");
    const std::string order = required(*fields, "orderId");
    if (operation == "releaseStock") {
        service->release(order);
        std::unique_ptr<KeyValuePayload> result(new KeyValuePayload(*fields));
        result->set("stockRequestStatus", "released");
        result->set("stockReceived", "no");
        result->set("reservationReference", "");
        return std::unique_ptr<DataPayload>(result.release());
    }
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

std::unique_ptr<DataPayload> WarehouseConnector::saveExternalState(const DataPayload& data) const {
    const KeyValuePayload* fields = dynamic_cast<const KeyValuePayload*>(&data);
    if (!fields) throw std::invalid_argument("Warehouse snapshot needs KeyValuePayload");
    const std::string owner = required(*fields, "orderId");
    const WarehouseAPI::Reservation saved = service->getReservation(owner);
    std::unique_ptr<KeyValuePayload> state(new KeyValuePayload());
    state->set("owner", owner);
    state->set("active", saved.active ? "yes" : "no");
    state->set("sku", saved.sku);
    state->set("quantity", std::to_string(saved.units));
    state->set("reference", saved.reference);
    return std::unique_ptr<DataPayload>(state.release());
}

void WarehouseConnector::restoreExternalState(const DataPayload& data, const DataPayload& state) {
    const KeyValuePayload* fields = dynamic_cast<const KeyValuePayload*>(&data);
    const KeyValuePayload* saved = dynamic_cast<const KeyValuePayload*>(&state);
    if (!fields || !saved) throw std::invalid_argument("Invalid warehouse snapshot type");
    const std::string owner = required(*fields, "orderId");
    if (owner != required(*saved, "owner"))
        throw std::invalid_argument("Reservation snapshot belongs to a different order");
    WarehouseAPI::Reservation target;
    const std::string active = required(*saved, "active");
    if (active != "yes" && active != "no") throw std::invalid_argument("Invalid reservation state");
    target.active = active == "yes";
    if (target.active) {
        target.sku = required(*saved, "sku");
        target.units = quantity(required(*saved, "quantity"));
        target.reference = required(*saved, "reference");
    }
    service->restoreReservation(owner, target);
}

std::unique_ptr<DataPayload> WarehouseConnector::cancelExternal(const DataPayload& data) {
    return invoke("releaseStock", data);
}
