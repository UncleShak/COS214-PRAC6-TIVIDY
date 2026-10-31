#include "WarehouseAPI.h"
#include <stdexcept>
void WarehouseStub::setStock(const std::string& sku, int units) {
    if (sku.empty() || units < 0) throw std::invalid_argument("Invalid stock entry");
    stock[sku] = units;
}
int WarehouseStub::remaining(const std::string& sku) const {
    const auto it = stock.find(sku);
    return it == stock.end() ? 0 : it->second;
}
WarehouseAPI::Reply WarehouseStub::reserve(const std::string& orderId,
                                          const std::string& sku, int units) {
    if (!online) throw std::runtime_error("Warehouse unavailable");
    if (orderId.empty() || sku.empty() || units <= 0)
        throw std::invalid_argument("Invalid reservation");
    ++calls;
    const auto found = reservations.find(orderId);
    if (found != reservations.end()) {
        if (found->second.sku != sku || found->second.units != units)
            return Reply{Conflict, ""};
        return Reply{Reserved, found->second.reference};
    }
    if (remaining(sku) < units) return Reply{OutOfStock, ""};
    Reservation saved{sku, units, "WH-" + orderId};
    // Store first, so allocation failure cannot consume inventory without a record.
    reservations.emplace(orderId, saved);
    stock.find(sku)->second -= units;
    return Reply{Reserved, saved.reference};
}
