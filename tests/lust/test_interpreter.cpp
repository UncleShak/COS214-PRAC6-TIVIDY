/**
 * @file test_interpreter.cpp
 * @brief Typed commands (LineInterpreter) and the plain front end, against FakeEngine.
 * @author Shakir Alexander (Lust)
 */
#include "doctest.h"

#include <sstream>
#include <string>

#include "FakeEngine.h"
#include "InterfaceController.h"
#include "LineInterpreter.h"
#include "PlainFrontEnd.h"

using namespace tividy::ui;

namespace {

struct Console {
    FakeEngine engine;
    InterfaceController ui;
    LineInterpreter cli;

    Console() : ui(engine), cli(ui) { ui.refresh(); }

    Reply operator()(const std::string& line) { return cli.run(line); }
};

bool mentions(const Reply& reply, const std::string& text) {
    if (reply.status.text.find(text) != std::string::npos) {
        return true;
    }
    for (std::size_t i = 0; i < reply.lines.size(); ++i) {
        if (reply.lines[i].find(text) != std::string::npos) {
            return true;
        }
    }
    return false;
}

} // namespace

TEST_CASE("blank lines do nothing; unknown commands are reported") {
    Console c;
    Reply blank = c("   ");
    CHECK(blank.lines.empty());
    CHECK(blank.status.text.empty());

    Reply unknown = c("fly away");
    CHECK(unknown.status.text == "Invalid input: unknown command 'fly'. Type 'help' for the list.");
    CHECK(c("help").lines.size() > 10);
    CHECK(c("QUIT").quit);
}

TEST_CASE("industry and start: guided usage, then an instance") {
    Console c;
    CHECK(mentions(c("start"), "choose an industry first"));
    CHECK(mentions(c("industries"), "Fake Industry A"));
    CHECK(c("industry 3").status.text == "Invalid input: there is no industry 3");
    CHECK_FALSE(c("industry 1").status.isError());
    CHECK(c.ui.view().industryId == "fake-a");

    Reply needs = c("start");
    CHECK(mentions(needs, "Usage: start amount=<number> destination=<text> [urgent=<text>]"));

    Reply bad = c("start amount=lots destination=Gotham");
    CHECK(bad.status.text == "Invalid input: Order amount must be a number");
    CHECK(mentions(c("start colour=red"), "unknown field 'colour'"));

    Reply ok = c("start amount=1500 destination=New York City");
    CHECK(ok.status.text == "Started Fake Order Flow as FAKE-0001");
    CHECK(c.ui.view().instance.data.at("destination") == "New York City");
    CHECK(mentions(ok, "Receive"));
    CHECK(c.cli.prompt() == "tividy fake-a/FAKE-0001 (Alex)> ");
}

TEST_CASE("work-item commands take a number or a name with spaces") {
    Console c;
    c("industry fake-a");
    c("start amount=10 destination=Gotham");

    Reply assigned = c("assign 1");
    CHECK(assigned.status.text == "Alex assigned 'Receive' to Alex (Available -> Assigned)");
    CHECK(mentions(assigned, "state:     'Receive' Available -> Assigned"));
    c("begin receive");
    Reply done = c("complete Receive");
    CHECK(mentions(done, "routing:   Parallel split: Receive -> Validate, Check Stock"));
    CHECK(mentions(done, "available: Validate, Check Stock"));

    Reply toCy = c("assign check stock to cy");
    CHECK(toCy.status.text == "Alex assigned 'Check Stock' to Cy (Available -> Assigned)");
    CHECK(c("as cy").status.text == "Acting as Cy");
    CHECK_FALSE(c("begin Check Stock").status.isError());

    CHECK(c("begin").status.text == "Invalid input: which work item? e.g. complete 2 or complete Check Stock");
    CHECK(c("begin 42").status.isError());
    CHECK(c("complete Check Stock now").status.text == "Invalid input: unexpected 'now' after the item");
    CHECK(c("assign 2 to nobody").status.isError());
}

TEST_CASE("engine refusals show the engine's own message") {
    Console c;
    c("industry 1");
    c("start amount=10 destination=Gotham");
    CHECK(c("escalate Validate").status.text == "cannot escalate 'Validate' while it is Created");
    CHECK(c("complete 1").status.text == "Alex is not assigned to 'Receive'");
}

TEST_CASE("reject-work needs a reason and records it") {
    Console c;
    c("industry 1");
    c("start amount=10 destination=Gotham");
    c("assign 1");
    c("begin 1");
    CHECK(c("reject-work 1").status.text == "Invalid input: a reason is required to reject work");
    CHECK_FALSE(c("reject-work 1 address is missing").status.isError());
    CHECK(mentions(c("history rejected"), "address is missing"));
}

TEST_CASE("approvals, approve, reject and advance as typed commands") {
    Console c;
    c("industry 1");
    c("start amount=1500 destination=Gotham");
    CHECK(mentions(c("approvals"), "No approval requests"));
    c("assign 1");
    c("begin 1");
    c("complete 1");
    c("assign Validate");
    c("begin Validate");
    c("complete Validate");

    Reply list = c("approvals");
    CHECK(mentions(list, "APR-1"));
    CHECK(mentions(list, "Pending"));

    CHECK(c("approve").status.text == "Invalid input: usage: approve <id>, e.g. approve APR-1");
    CHECK(c("approve apr-1").status.text == "Alex cannot decide APR-1: it needs the Manager role");
    c("as Bo");
    Reply approved = c("approve apr-1");
    CHECK(approved.status.text == "Bo approved APR-1 for 'Approve' (Available -> Completed)");
    CHECK(mentions(approved, "routing:   Sequence: Approve -> Pack"));
    CHECK(c("reject APR-1 too late").status.text == "approval APR-1 is already Approved");

    CHECK(c("advance").status.isError());
    CHECK(c("advance 2.5").status.isError());
    Reply moved = c("advance 10");
    CHECK(moved.status.text == "Clock advanced by 10 to t=10");
    CHECK(mentions(moved, "passed its deadline"));
}

TEST_CASE("history and report") {
    Console c;
    CHECK(c("history").status.isError());
    c("industry 1");
    c("start amount=10 destination=Gotham");
    c("assign 1");
    Reply oldest = c("history");
    CHECK(oldest.lines.front().find("Instance FAKE-0001 started") != std::string::npos);
    Reply newest = c("history newest");
    CHECK(newest.lines.front().find("assigned 'Receive'") != std::string::npos);

    Reply report = c("report");
    CHECK(report.lines.front() == "Progress: 6 work items: 1 Assigned, 5 Created (0% complete)");
    CHECK(mentions(report, "Overdue (escalated): none"));
}

TEST_CASE("plain front end runs a script and stops at quit or end of input") {
    FakeEngine engine;
    InterfaceController ui(engine);
    ui.refresh();
    std::istringstream script("industry 1\nstart amount=10 destination=Gotham\nfly\nquit\nindustries\n");
    std::ostringstream out;
    CHECK(PlainFrontEnd(ui, script, out, "FAKE").run() == 0);

    const std::string text = out.str();
    CHECK(text.find("[FAKE]") != std::string::npos);
    CHECK(text.find("Started Fake Order Flow as FAKE-0001") != std::string::npos);
    CHECK(text.find("! Invalid input: unknown command 'fly'") != std::string::npos);
    CHECK(text.find("Goodbye.") != std::string::npos);
    CHECK(text.find("Fake Industry B") == std::string::npos); // nothing ran after quit

    std::istringstream empty("");
    std::ostringstream out2;
    CHECK(PlainFrontEnd(ui, empty, out2, "FAKE").run() == 0); // end of input quits cleanly
}
