#include "DuneHarvestBuilder.h"
#include "../../assignment/RoleBasedAssignment.h"
#include "../../approval/AutoApprover.h"
#include "../../approval/RoleBasedApprover.h"
#include <stdexcept>

DuneHarvestBuilder::DuneHarvestBuilder() : ScenarioBuilder("DuneSpiceHarvest", "1.0") {}

void DuneHarvestBuilder::buildStructure() {
    buildStage("Preparation");
    buildStage("Deployment");
    buildStage("Extraction");
    buildStage("Emergency");
    buildWorkItem("SurveyField", "Surveyor", "Preparation");
    buildWorkItem("ApproveHarvestPermit", "OperationsManager", "Preparation");
    buildWorkItem("AbandonRun", "OperationsManager", "Preparation");
    buildWorkItem("DeployHarvester", "CrewChief", "Deployment");
    buildWorkItem("DeployCarryall", "CarryallPilot", "Deployment");
    buildWorkItem("Harvest", "CrewChief", "Extraction");
    buildWorkItem("TransportToRefinery", "CarryallPilot", "Extraction");
    buildWorkItem("Evacuate", "CarryallPilot", "Emergency");
}

void DuneHarvestBuilder::buildRules() {
    buildEntry("SurveyField");
    buildAssignmentRule(std::unique_ptr<AssignmentRule>(new RoleBasedAssignment()));

    buildPath("SurveyField", "ApproveHarvestPermit");
    buildBranch("ApproveHarvestPermit", RoutingCondition::equals("permitGranted", "yes"),
                {"DeployHarvester", "DeployCarryall"});
    buildDefaultBranch("ApproveHarvestPermit", {"AbandonRun"});
    buildPath("DeployHarvester", "Harvest");
    buildPath("DeployCarryall", "Harvest");
    buildPath("Harvest", "TransportToRefinery");
// A rejected step abandons the run (Bug 1); see ArkhamExpressBuilder.
    const char* const rejectable[] = {"SurveyField", "ApproveHarvestPermit", "DeployHarvester",
                                      "DeployCarryall", "Harvest"};
    for (const char* item : rejectable) buildRejectionPath(item, {"AbandonRun"});

    // A rejected step abandons the run (Bug 1); see ArkhamExpressBuilder.
    const char* const rejectable[] = {"SurveyField", "ApproveHarvestPermit", "DeployHarvester",
                                      "DeployCarryall", "Harvest"};
    for (const char* item : rejectable) buildRejectionPath(item, {"AbandonRun"});

    buildDependency("ApproveHarvestPermit", "SurveyField");
    buildDependency("Harvest", "DeployHarvester");   // the harvester waits for both machines
    buildDependency("Harvest", "DeployCarryall");
    buildDependency("TransportToRefinery", "Harvest");

    // Chain of Responsibility: the permit is judged on the run's estimated yield.
    // Up to 10 t the field desk approves on its own; up to 100 t the operations manager decides
    // within 3 clock units, after which the request escalates to any Manager.
    std::unique_ptr<ApprovalHandler> chain(new AutoApprover("Field desk", 10));
    chain->setNext(new RoleBasedApprover("Operations", "OperationsManager", 100, 3))
         ->setNext(new RoleBasedApprover("House management", "Manager", UNLIMITED, 0));
    buildApprovalChain(std::move(chain));
    buildApproval("ApproveHarvestPermit", "estimatedYieldTonnes", "permitGranted");

    // Decorators: the evacuation must finish before the worm arrives; the harvest is audited.
    buildDeadline("Evacuate", 3);
    buildAudit("Harvest");

    buildAccessRule("cancel", {"OperationsManager"});
    // Collected spice may still ship if Harvest was already completed.
    buildInterrupt("WORM_SIGN",
                   {"SurveyField", "ApproveHarvestPermit", "AbandonRun", "DeployHarvester",
                    "DeployCarryall", "Harvest"},
                   {"Evacuate"});
}

void DuneHarvestBuilder::buildConnectors() {}

KeyValuePayload DuneHarvestBuilder::exampleRun(const std::string& runId) {
    if (runId.empty()) throw std::invalid_argument("Run ID cannot be empty");
    KeyValuePayload data;
    data.set("runId", runId);
    data.set("field", "Hagga Basin, sector 7");
    data.set("spiceDensity", "high");
    data.set("estimatedYieldTonnes", "40");
    return data;
}
