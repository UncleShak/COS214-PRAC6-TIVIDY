// The subway route model (no terminal needed): main line, exception branch, where the train is.
#include "doctest.h"

#include "tui/RouteLine.h"

using namespace tividy::ui;

namespace {

WorkItemView item(const std::string& name, const std::string& state, const std::string& tracker = "") {
    WorkItemView v;
    v.id = name;
    v.name = name;
    v.state = state;
    v.trackerStatus = tracker;
    return v;
}

RouteStop stop(const std::string& name, const std::string& splitsFrom = "") {
    RouteStop s;
    s.workItemId = name;
    s.exception = !splitsFrom.empty();
    s.splitsFrom = splitsFrom;
    return s;
}

std::vector<RouteStop> map() {
    std::vector<RouteStop> m;
    m.push_back(stop("ReceiveOrder"));
    m.push_back(stop("ValidateOrder"));
    m.push_back(stop("CheckStock"));
    m.push_back(stop("CloseOrder"));
    m.push_back(stop("CancelOrder", "ValidateOrder"));
    return m;
}

}  // namespace

TEST_SUITE("lust route line") {

TEST_CASE("labels: the engine's short label, else the first CamelCase word, at most 8 characters") {
    CHECK(stationLabel("SelectDeliveryType", "") == "Select");
    CHECK(stationLabel("Dispatch", "") == "Dispatch");
    CHECK(stationLabel("TransportToRefinery", "") == "Transpor");
    CHECK(stationLabel("ReceiveOrder", "Intake") == "Intake");
}

TEST_CASE("a fresh instance: first stop current, branch not taken and splitting where the map says") {
    std::vector<WorkItemView> items;
    items.push_back(item("ReceiveOrder", "Available", "available"));
    items.push_back(item("ValidateOrder", "Created"));
    items.push_back(item("CheckStock", "Created"));
    items.push_back(item("CloseOrder", "Created"));
    items.push_back(item("CancelOrder", "Created"));
    const RouteModel r = RouteModel::build(items, map());
    CHECK(r.main.size() == 4);
    CHECK(r.branch.size() == 1);
    CHECK(r.split == 1);
    CHECK_FALSE(r.branchTaken);
    CHECK(r.current == 0);
    REQUIRE(r.nextStation() != 0);
    CHECK(r.nextStation()->name == "ValidateOrder");
    CHECK(r.done == 0);
}

TEST_CASE("taking the exception route dims the main line after the split") {
    std::vector<WorkItemView> items;
    items.push_back(item("ReceiveOrder", "Completed"));
    items.push_back(item("ValidateOrder", "Completed"));
    items.push_back(item("CheckStock", "Created"));
    items.push_back(item("CloseOrder", "Created"));
    items.push_back(item("CancelOrder", "Available", "available"));
    const RouteModel r = RouteModel::build(items, map());
    CHECK(r.branchTaken);
    CHECK(r.branchCurrent == 0);
    CHECK(r.main[2].kind == Station::Kind::Skipped);
    REQUIRE(r.currentStation() != 0);
    CHECK(r.currentStation()->name == "CancelOrder");
    CHECK(r.nextStation() == 0);   // last stop
    CHECK(r.done == 2);
}

TEST_CASE("no route map: every item is a main stop; a passed-by stop is skipped") {
    std::vector<WorkItemView> items;
    items.push_back(item("A", "Completed"));
    items.push_back(item("B", "Created"));
    items.push_back(item("C", "Started"));
    const RouteModel r = RouteModel::build(items, std::vector<RouteStop>());
    CHECK(r.main.size() == 3);
    CHECK(r.branch.empty());
    CHECK(r.split == -1);
    CHECK(r.main[1].kind == Station::Kind::Skipped);
    CHECK(r.current == 2);
}

TEST_CASE("stations need 10 columns each, or the compact form is used") {
    CHECK(stationColumns(10, 100).size() == 10);
    CHECK(stationColumns(10, 60).empty());
    const std::vector<int> c = stationColumns(3, 100);
    REQUIRE(c.size() == 3);
    CHECK(c[1] - c[0] >= kStationSpacing);
}

TEST_CASE("the mini route shows the branch's current stop in red after the split") {
    std::vector<WorkItemView> items;
    items.push_back(item("ReceiveOrder", "Completed"));
    items.push_back(item("ValidateOrder", "Completed"));
    items.push_back(item("CheckStock", "Created"));
    items.push_back(item("CloseOrder", "Created"));
    items.push_back(item("CancelOrder", "Started"));
    const std::vector<std::pair<std::string, Tone> > cells = miniRoute(RouteModel::build(items, map()), 2);
    bool redCurrent = false;
    for (std::size_t i = 0; i < cells.size(); ++i)
        if (cells[i].first == "◉" && cells[i].second == Tone::Red) redCurrent = true;
    CHECK(redCurrent);
}

}  // TEST_SUITE
