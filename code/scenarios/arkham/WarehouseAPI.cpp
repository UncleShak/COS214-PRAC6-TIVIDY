#include "WarehouseAPI.h"
#include <stdexcept>
#include <limits>
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
    Reservation saved;
    saved.active = true;
    saved.sku = sku;
    saved.units = units;
    saved.reference = "WH-" + orderId;
    // Store first, so allocation failure cannot consume inventory without a record.
    reservations.emplace(orderId, saved);
    stock.find(sku)->second -= units;
    return Reply{Reserved, saved.reference};
}

WarehouseAPI::Reservation WarehouseStub::getReservation(const std::string& orderId) const {
    const auto found = reservations.find(orderId);
    return found == reservations.end() ? Reservation() : found->second;
}

void WarehouseStub::restoreReservation(const std::string& orderId, const Reservation& saved) {
    if (orderId.empty()) throw std::invalid_argument("Reservation needs an order ID");
    const Reservation current = getReservation(orderId);
    if (current.active == saved.active && (!saved.active ||
        (current.sku == saved.sku && current.units == saved.units && current.reference == saved.reference)))
        return; // No external change, also safe during an outage.
    if (!online) throw std::runtime_error("Warehouse unavailable: reservation was not changed");
    if (saved.active && (saved.sku.empty() || saved.units <= 0 || saved.reference != "WH-" + orderId))
        throw std::invalid_argument("Invalid saved reservation");

    // Prepare on CURRENT inventory, never on an old whole-warehouse snapshot.
    // Copy-and-swap keeps this local stub atomic even if an allocation fails.
    auto nextStock = stock;
    auto nextReservations = reservations;
    if (current.active) {
        const long long returned = static_cast<long long>(nextStock[current.sku]) + current.units;
        if (returned > std::numeric_limits<int>::max())
            throw std::overflow_error("Cannot release reservation: inventory would overflow");
        nextStock[current.sku] = static_cast<int>(returned);
        nextReservations.erase(orderId);
    }
    if (saved.active) {
        if (nextStock[saved.sku] < saved.units)
            throw std::runtime_error("Cannot restore reservation: insufficient stock");
        nextStock[saved.sku] -= saved.units;
        nextReservations[orderId] = saved;
    }
    stock.swap(nextStock);
    reservations.swap(nextReservations);
}

bool WarehouseStub::release(const std::string& orderId) {
    if (orderId.empty()) throw std::invalid_argument("Reservation needs an order ID");
    if (!getReservation(orderId).active) return false;
    restoreReservation(orderId, Reservation());
    return true;
}
