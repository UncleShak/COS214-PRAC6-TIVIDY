/**
 * @file test_routing.cpp
 * @brief Envy: routing takes the right branch (SequentialRouting, DataBasedRouting).
 */
#include "doctest.h"

#include <stdexcept>
#include <string>
#include <vector>

#include "DataBasedRouting.h"
#include "KeyValuePayload.h"
#include "RoutingCondition.h"
#include "SequentialRouting.h"

namespace {
std::vector<std::string> names(const char* a) { return std::vector<std::string>(1, a); }
std::vector<std::string> names(const char* a, const char* b) {
    std::vector<std::string> v; v.push_back(a); v.push_back(b); return v;
}

/// The ArkhamExpress validation decision from Practical 6.
DataBasedRouting validationRouting() {
    DataBasedRouting routing;
    routing.addPath("ReceiveOrder", "ValidateOrder");
    routing.addBranch("ValidateOrder", RoutingCondition::equals("orderValid", "yes"), names("CheckStock"));
    routing.setDefaultBranch("ValidateOrder", names("CancelOrder"));
    return routing;
}
}

TEST_SUITE("routing") {

TEST_CASE("sequential routing follows the configured path") {
    SequentialRouting routing;
    routing.addPath("ReceiveOrder", "ValidateOrder");
    KeyValuePayload data;

    RoutingDecision decision = routing.determineNextWork("ReceiveOrder", data);
    CHECK(decision.completedItem == "ReceiveOrder");
    CHECK(decision.nextItems == names("ValidateOrder"));
    CHECK(decision.description == "Sequence: ReceiveOrder -> ValidateOrder");
}

TEST_CASE("several successors form a parallel split") {
    SequentialRouting routing;
    routing.addPath("PreparePackage", "SelectDeliveryType");
    routing.addPath("PreparePackage", "AssignDriver");
    routing.addPath("PreparePackage", "AssignDriver"); // duplicate is ignored
    KeyValuePayload data;

    RoutingDecision decision = routing.determineNextWork("PreparePackage", data);
    CHECK(decision.nextItems == names("SelectDeliveryType", "AssignDriver"));
    CHECK(decision.description.find("Parallel split") == 0);
}

TEST_CASE("an item with no successor ends its path") {
    SequentialRouting routing;
    KeyValuePayload data;
    CHECK(routing.determineNextWork("Deliver", data).nextItems.empty());
}

TEST_CASE("invalid paths are refused") {
    SequentialRouting routing;
    CHECK_THROWS_AS(routing.addPath("A", "A"), std::invalid_argument);
    CHECK_THROWS_AS(routing.addPath("", "B"), std::invalid_argument);
}

TEST_CASE("a decision takes the branch that matches the instance data") {
    DataBasedRouting routing = validationRouting();

    KeyValuePayload valid;
    valid.set("orderValid", "yes");
    RoutingDecision yes = routing.determineNextWork("ValidateOrder", valid);
    CHECK(yes.nextItems == names("CheckStock"));
    CHECK(yes.description == "Decision at ValidateOrder: [orderValid == yes] -> CheckStock");

    KeyValuePayload invalid;
    invalid.set("orderValid", "no");
    RoutingDecision no = routing.determineNextWork("ValidateOrder", invalid);
    CHECK(no.nextItems == names("CancelOrder"));
    CHECK(no.description == "Decision at ValidateOrder: [otherwise] -> CancelOrder");
}

TEST_CASE("missing data never matches a guard, so the default branch is taken") {
    DataBasedRouting routing = validationRouting();
    KeyValuePayload empty;
    CHECK(routing.determineNextWork("ValidateOrder", empty).nextItems == names("CancelOrder"));
}

TEST_CASE("branches are checked in order and the first match wins") {
    DataBasedRouting routing;
    routing.addBranch("SelectDeliveryType", RoutingCondition::equals("urgency", "same-day"), names("ExpressCourier"));
    routing.addBranch("SelectDeliveryType", RoutingCondition::greaterThan("distanceKm", 50), names("LinehaulTruck"));
    routing.addBranch("SelectDeliveryType", RoutingCondition::atMost("distanceKm", 50), names("LocalDriver"));

    KeyValuePayload urgentAndFar;
    urgentAndFar.set("urgency", "same-day");
    urgentAndFar.set("distanceKm", "120");
    CHECK(routing.determineNextWork("SelectDeliveryType", urgentAndFar).nextItems == names("ExpressCourier"));

    KeyValuePayload far;
    far.set("urgency", "standard");
    far.set("distanceKm", "120");
    CHECK(routing.determineNextWork("SelectDeliveryType", far).nextItems == names("LinehaulTruck"));

    KeyValuePayload near;
    near.set("urgency", "standard");
    near.set("distanceKm", "50");
    CHECK(routing.determineNextWork("SelectDeliveryType", near).nextItems == names("LocalDriver"));
}

TEST_CASE("a decision with no matching branch and no default is an error") {
    DataBasedRouting routing;
    routing.addBranch("SelectDeliveryType", RoutingCondition::greaterThan("distanceKm", 50), names("LinehaulTruck"));
    KeyValuePayload notANumber;
    notANumber.set("distanceKm", "unknown");
    CHECK_THROWS_AS(routing.determineNextWork("SelectDeliveryType", notANumber), std::runtime_error);
}

TEST_CASE("items without a decision point follow the fixed path") {
    DataBasedRouting routing = validationRouting();
    KeyValuePayload data;
    CHECK_FALSE(routing.hasDecision("ReceiveOrder"));
    CHECK(routing.hasDecision("ValidateOrder"));
    CHECK(routing.determineNextWork("ReceiveOrder", data).nextItems == names("ValidateOrder"));
}

TEST_CASE("industries can supply their own guard without changing the engine") {
    DataBasedRouting routing;
    routing.addBranch("AssessClaim",
        RoutingCondition("[claim needs a site visit]", [](const DataPayload& payload) {
            const KeyValuePayload& d = dynamic_cast<const KeyValuePayload&>(payload);
            return d.has("claimType") && d.get("claimType") == "property" && d.getNumber("amount") > 20000;
        }),
        names("SiteVisit"));
    routing.setDefaultBranch("AssessClaim", names("ApproveClaim"));

    KeyValuePayload big;
    big.set("claimType", "property");
    big.set("amount", "45000");
    CHECK(routing.determineNextWork("AssessClaim", big).nextItems == names("SiteVisit"));

    KeyValuePayload small;
    small.set("claimType", "property");
    small.set("amount", "800");
    CHECK(routing.determineNextWork("AssessClaim", small).nextItems == names("ApproveClaim"));
}

TEST_CASE("one shared rule routes two instances by their own data") {
    // The rule belongs to the definition; each instance passes its own payload.
    const DataBasedRouting routing = validationRouting();
    KeyValuePayload first, second;
    first.set("orderValid", "yes");
    second.set("orderValid", "no");

    CHECK(routing.determineNextWork("ValidateOrder", first).nextItems == names("CheckStock"));
    CHECK(routing.determineNextWork("ValidateOrder", second).nextItems == names("CancelOrder"));
    CHECK(routing.determineNextWork("ValidateOrder", first).nextItems == names("CheckStock"));
}

TEST_CASE("branches must lead somewhere") {
    DataBasedRouting routing;
    CHECK_THROWS_AS(routing.addBranch("X", RoutingCondition::equals("a", "b"), std::vector<std::string>()),
                    std::invalid_argument);
    CHECK_THROWS_AS(routing.setDefaultBranch("X", std::vector<std::string>()), std::invalid_argument);
}

TEST_CASE("KeyValuePayload reports missing and non-numeric values") {
    KeyValuePayload data;
    data.set("distanceKm", "12.5");
    data.set("address", "12 Main Rd");
    CHECK(data.getNumber("distanceKm") == doctest::Approx(12.5));
    CHECK_THROWS_AS(data.get("missing"), std::out_of_range);
    CHECK_THROWS_AS(data.getNumber("address"), std::invalid_argument);
}

}

// ---- Integration with the team's abstract DataPayload (integration-Code) ----

namespace {
// Stand-in for some other team payload type that has no named fields.
class OpaquePayload : public DataPayload {
public:
    std::string getPayloadType() const { return "Opaque"; }
    DataPayload* clone() const { return new OpaquePayload(*this); }
};
}

TEST_CASE("routing accepts any DataPayload; a payload without fields takes the default branch") {
    DataBasedRouting routing = validationRouting();
    OpaquePayload opaque;
    RoutingDecision decision = routing.determineNextWork("ValidateOrder", opaque);
    CHECK(decision.nextItems == names("CancelOrder"));
}

TEST_CASE("KeyValuePayload clones through DataPayload* so each instance keeps its own data") {
    KeyValuePayload original;
    original.set("orderValid", "yes");

    DataPayload* asBase = &original;
    CHECK(asBase->getPayloadType() == "KeyValue");

    DataPayload* copy = asBase->clone();           // what WorkflowInstance's constructor does
    KeyValuePayload* copyFields = dynamic_cast<KeyValuePayload*>(copy);
    REQUIRE(copyFields != 0);
    copyFields->set("orderValid", "no");

    DataBasedRouting routing = validationRouting();
    CHECK(routing.determineNextWork("ValidateOrder", original).nextItems == names("CheckStock"));
    CHECK(routing.determineNextWork("ValidateOrder", *copy).nextItems == names("CancelOrder"));
    delete copy;
}
