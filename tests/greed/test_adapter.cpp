#include "doctest.h"
#include "scenarios/arkham/ArkhamExpressBuilder.h"
#include "scenarios/arkham/WarehouseConnector.h"
#include <limits>
#include <stdexcept>
namespace {
struct Fixture {
    std::shared_ptr<WarehouseStub> api{new WarehouseStub()};
    WarehouseConnector adapter{api};
    KeyValuePayload data = ArkhamExpressBuilder::exampleOrder("AX-10482");
    Fixture() { api->setStock("AX-NOTEBOOK", 5); }
    KeyValuePayload run() {
        const auto response = adapter.invoke("requestStock", data);
        return dynamic_cast<const KeyValuePayload&>(*response);
    }
};
class Opaque : public DataPayload {
    std::string getPayloadType() const override { return "Opaque"; }
    DataPayload* clone() const override { return new Opaque(); }
};
}
TEST_CASE("Adapter translates fields and vendor success; preserves request without mutating it") {
    Fixture f;
    Connector& target = f.adapter;
    const auto response = target.invoke("requestStock", f.data);
    const auto& fields = dynamic_cast<const KeyValuePayload&>(*response);
    CHECK(fields.get("stockReceived") == "yes");
    CHECK(fields.get("stockRequestStatus") == "reserved");
    CHECK(fields.get("reservationReference") == "WH-AX-10482");
    CHECK(fields.get("orderId") == "AX-10482");
    CHECK(fields.get("delivered") == "yes");
    CHECK(f.data.get("stockReceived") == "no");
    CHECK(f.api->remaining("AX-NOTEBOOK") == 3);
}
TEST_CASE("Business refusal is data, and a later restock permits retry") {
    Fixture f; f.api->setStock("AX-NOTEBOOK", 1);
    CHECK(f.run().get("stockRequestStatus") == "outOfStock");
    CHECK(f.api->remaining("AX-NOTEBOOK") == 1);
    f.api->setStock("AX-NOTEBOOK", 3);
    CHECK(f.run().get("stockReceived") == "yes");
    CHECK(f.api->remaining("AX-NOTEBOOK") == 1);
}
TEST_CASE("Duplicate accepted requests are idempotent; changed details conflict") {
    Fixture f;
    const auto first = f.run();
    CHECK(f.run().get("reservationReference") == first.get("reservationReference"));
    CHECK(f.api->remaining("AX-NOTEBOOK") == 3);
    f.data.set("quantity", "1");
    CHECK(f.run().get("stockRequestStatus") == "conflict");
    CHECK(f.run().get("stockReceived") == "no");
    CHECK(f.api->remaining("AX-NOTEBOOK") == 3);
}
TEST_CASE("Unavailable service throws without changing payload or inventory") {
    Fixture f; f.api->setOnline(false);
    CHECK_FALSE(f.adapter.isAvailable());
    CHECK_THROWS_AS(f.run(), std::runtime_error);
    CHECK(f.api->reservationCalls() == 0);
    CHECK(f.api->remaining("AX-NOTEBOOK") == 5);
    CHECK(f.data.get("stockReceived") == "no");
}
TEST_CASE("Invalid operation, payload type and missing fields never reach vendor") {
    Fixture f; Opaque other;
    CHECK_THROWS_AS(f.adapter.invoke("deleteAll", f.data), std::invalid_argument);
    CHECK_THROWS_AS(f.adapter.invoke("requestStock", other), std::invalid_argument);
    KeyValuePayload missing;
    CHECK_THROWS_AS(f.adapter.invoke("requestStock", missing), std::invalid_argument);
    f.data.set("orderId", "");
    CHECK_THROWS_AS(f.run(), std::invalid_argument);
    CHECK(f.api->reservationCalls() == 0);
}
TEST_CASE("Quantity must be a bounded positive integer, never fractional or nonfinite") {
    Fixture f;
    for (const std::string bad : {"", "0", "-1", "1.5", "nan", "inf", " 2", "2x", "999999999999999999999"}) {
        f.data.set("quantity", bad);
        CHECK_THROWS_AS(f.run(), std::invalid_argument);
    }
    CHECK(f.api->reservationCalls() == 0);
    f.data.set("quantity", std::to_string(std::numeric_limits<int>::max()));
    CHECK(f.run().get("stockRequestStatus") == "outOfStock");
}
TEST_CASE("Warehouse constructor rejects missing adaptee and stub rejects invalid stock") {
    CHECK_THROWS_AS(WarehouseConnector(nullptr), std::invalid_argument);
    WarehouseStub stub;
    CHECK_THROWS_AS(stub.setStock("", 1), std::invalid_argument);
    CHECK_THROWS_AS(stub.setStock("SKU", -1), std::invalid_argument);
}
TEST_CASE("Different order IDs reserve independently and responses do not alias") {
    Fixture f;
    auto first = f.run();
    f.data.set("orderId", "AX-10483");
    auto second = f.run();
    second.set("stockReceived", "changed");
    CHECK(first.get("stockReceived") == "yes");
    CHECK(first.get("reservationReference") != second.get("reservationReference"));
    CHECK(f.api->remaining("AX-NOTEBOOK") == 1);
}
