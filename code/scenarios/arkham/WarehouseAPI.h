#ifndef TIVIDY_WAREHOUSE_API_H
#define TIVIDY_WAREHOUSE_API_H
#include <map>
#include <string>
/** @brief Adaptee contract: represents a vendor API, independent of TIVIDY payloads.
 * reserve is idempotent by order ID: repeating the same accepted request does not
 * consume stock twice; reusing that ID with different details is a conflict.
 */
class WarehouseAPI {
public:
    enum Status { Reserved, OutOfStock, Conflict };
    struct Reply { Status status; std::string reference; };
    virtual ~WarehouseAPI() = default;
    virtual bool ping() const = 0;
    virtual Reply reserve(const std::string& orderId, const std::string& sku, int units) = 0;
};
/** @brief Documented local external-service stub; no network or real orders.
 * Inventory is shared external state, intentionally separate from workflow progress.
 * Single-threaded, matching the project's interactive demonstration.
 */
class WarehouseStub : public WarehouseAPI {
public:
    WarehouseStub() : online(true), calls(0) {}
    bool ping() const override { return online; }
    void setOnline(bool value) { online = value; }
    void setStock(const std::string& sku, int units);
    int remaining(const std::string& sku) const;
    int reservationCalls() const { return calls; }
    Reply reserve(const std::string&, const std::string&, int) override;
private:
    struct Reservation { std::string sku; int units; std::string reference; };
    bool online;
    int calls;
    std::map<std::string, int> stock;
    std::map<std::string, Reservation> reservations;
};
#endif
