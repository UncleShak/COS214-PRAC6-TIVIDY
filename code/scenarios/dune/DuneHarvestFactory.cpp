#include "DuneHarvestFactory.h"
#include "DuneHarvestBuilder.h"

namespace {
std::unique_ptr<Participant> person(const std::string& name, const std::string& id,
                                    const std::vector<std::string>& roles) {
    std::unique_ptr<Participant> p(new Participant(name, id));
    for (std::size_t i = 0; i < roles.size(); ++i) p->addRole(Role(roles[i]));
    return p;
}
}

std::string DuneHarvestFactory::getWorkflowName() const { return "DuneSpiceHarvest"; }
std::string DuneHarvestFactory::getDisplayName() const { return "Dune: Spice Harvest"; }
std::string DuneHarvestFactory::getDescription() const {
    return "Mining: one harvest run on a spice field of Arrakis. Watch for worm sign.";
}

std::string DuneHarvestFactory::getIdPrefix() const { return "RUN"; }

std::unique_ptr<WorkflowBuilder> DuneHarvestFactory::createBuilder() const {
    return std::unique_ptr<WorkflowBuilder>(new DuneHarvestBuilder());
}

std::vector<std::unique_ptr<Participant> > DuneHarvestFactory::createParticipants() const {
    std::vector<std::unique_ptr<Participant> > crew;
    crew.push_back(person("Idris Kane", "du-surveyor", {"Surveyor"}));
    crew.push_back(person("Mara Voss", "du-chief", {"CrewChief"}));
    crew.push_back(person("Tobin Reyes", "du-pilot", {"CarryallPilot"}));
    crew.push_back(person("Sela Orrin", "du-ops", {"OperationsManager", "Manager"}));
    return crew;
}

std::unique_ptr<DataPayload> DuneHarvestFactory::createInitialData(const std::string& instanceId) const {
    return std::unique_ptr<DataPayload>(new KeyValuePayload(DuneHarvestBuilder::exampleRun(instanceId)));
}

std::vector<std::string> DuneHarvestFactory::getEventTypes() const {
    return std::vector<std::string>(1, "WORM_SIGN");
}
