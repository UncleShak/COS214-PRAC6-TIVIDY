#include "ScenarioBuilder.h"
#include "../workflow/Stage.h"
#include "../workflow/WorkItem.h"
#include <algorithm>
#include <iterator>
#include <stdexcept>
ScenarioBuilder::ScenarioBuilder(const std::string& n, const std::string& v)
    : name(n), version(v), routing(nullptr) {
    if (name.empty() || version.empty()) throw std::invalid_argument("Workflow needs a name and version");
}
void ScenarioBuilder::reset() {
    product.reset(new WorkflowDefinition(new Stage(name), version));
    stages.clear(); items.clear(); edges.clear(); rejectionEdges.clear(); defaults.clear();
    stages[name] = static_cast<Stage*>(product->getRootStage());
    product->routingRule.reset(new DataBasedRouting());
    routing = static_cast<DataBasedRouting*>(product->routingRule.get());
}
void ScenarioBuilder::requireProduct() const {
    if (!product) throw std::logic_error("Reset builder before construction; result may already be transferred");
}
void ScenarioBuilder::requireItem(const std::string& item) const {
    requireProduct();
    if (!items.count(item)) throw std::invalid_argument("Unknown work item: " + item);
}
void ScenarioBuilder::buildStage(const std::string& n, const std::string& parent) {
    requireProduct();
    if (n.empty() || stages.count(n) || items.count(n)) throw std::invalid_argument("Duplicate or empty component name");
    auto found = stages.find(parent.empty() ? name : parent);
    if (found == stages.end()) throw std::invalid_argument("Unknown parent stage");
    std::unique_ptr<Stage> stage(new Stage(n));
    Stage* raw = stage.get();
    found->second->add(raw); stage.release();
    stages[n] = raw;
}
void ScenarioBuilder::buildWorkItem(const std::string& n, const std::string& role, const std::string& stage) {
    requireProduct();
    if (n.empty() || role.empty() || stages.count(n) || items.count(n))
        throw std::invalid_argument("Work item needs a unique name and nonempty role");
    auto found = stages.find(stage);
    if (found == stages.end()) throw std::invalid_argument("Unknown work-item stage");
    std::unique_ptr<WorkItem> item(new WorkItem(n, role));
    found->second->add(item.get()); item.release();
    items.insert(n);
}
void ScenarioBuilder::buildEntry(const std::string& item) {
    requireItem(item);
    auto& entries = product->entryItems;
    if (std::find(entries.begin(), entries.end(), item) == entries.end()) entries.push_back(item);
}
void ScenarioBuilder::buildDependency(const std::string& item, const std::string& pre) {
    requireItem(item); requireItem(pre);
    product->dependencies.addDependency(item, pre);
}
void ScenarioBuilder::checkTargets(const std::string& from, const std::vector<std::string>& targets) const {
    requireItem(from);
    if (targets.empty()) throw std::invalid_argument("Routing branch must have a target");
    for (const auto& to : targets) {
        requireItem(to);
        if (from == to) throw std::invalid_argument("Self-routing is not supported");
    }
}
void ScenarioBuilder::recordEdges(const std::string& from, const std::vector<std::string>& targets) {
    edges[from].insert(targets.begin(), targets.end());
}
void ScenarioBuilder::buildPath(const std::string& from, const std::string& to) {
    checkTargets(from, {to});
    if (routing->hasDecision(from)) throw std::invalid_argument("Cannot mix fixed and conditional paths at one item");
    routing->addPath(from, to); recordEdges(from, {to});
}
void ScenarioBuilder::buildBranch(const std::string& from, const RoutingCondition& condition,
                                  const std::vector<std::string>& targets) {
    checkTargets(from, targets);
    if (edges.count(from) && !routing->hasDecision(from)) throw std::invalid_argument("Item already has fixed paths");
    routing->addBranch(from, condition, targets); recordEdges(from, targets);
}
void ScenarioBuilder::buildDefaultBranch(const std::string& from, const std::vector<std::string>& targets) {
    checkTargets(from, targets);
    if (edges.count(from) && !routing->hasDecision(from)) throw std::invalid_argument("Item already has fixed paths");
    if (defaults.count(from)) throw std::invalid_argument("Decision already has a default branch");
    routing->setDefaultBranch(from, targets); recordEdges(from, targets); defaults.insert(from);
}
void ScenarioBuilder::buildRejectionPath(const std::string& from, const std::vector<std::string>& targets) {
    checkTargets(from, targets);
    if (routing->hasRejectionPath(from)) throw std::invalid_argument("'" + from + "' already has a rejection path");
    routing->setRejectionPath(from, targets); rejectionEdges[from] = targets;
}
void ScenarioBuilder::buildEndOutcome(const std::string& item, const std::string& outcome) {
    requireItem(item);
    if (outcome.empty()) throw std::invalid_argument("End outcome for '" + item + "' needs a name");
    // These describe a run that has not ended at a tagged step (WorkflowInstance::getOutcome).
    if (outcome == "not started" || outcome == "running" || outcome == "stalled" || outcome == "stopped")
        throw std::invalid_argument("'" + outcome + "' is reserved and cannot be an end outcome");
    for (const auto& tagged : product->endOutcomes)
        if (tagged.item == item) throw std::invalid_argument("'" + item + "' already has an end outcome");
    WorkflowDefinition::EndOutcome tag;
    tag.item = item; tag.outcome = outcome;
    product->endOutcomes.push_back(tag);
}
void ScenarioBuilder::buildAssignmentRule(std::unique_ptr<AssignmentRule> rule) {
    requireProduct();
    if (!rule) throw std::invalid_argument("Assignment rule cannot be null");
    product->assignmentRule = std::move(rule);
}
void ScenarioBuilder::buildConnector(const std::string& item, std::shared_ptr<Connector> connector,
                                     const std::string& operation) {
    requireItem(item);
    if (!connector || operation.empty()) throw std::invalid_argument("Connector and operation are required");
    if (product->connectors.count(item)) throw std::invalid_argument("Item already has a connector");
    product->connectors[item] = WorkflowDefinition::ConnectorBinding{connector, operation};
}
void ScenarioBuilder::buildAccessRule(const std::string& action, const std::vector<std::string>& roles) {
    requireProduct();
    static const char* const known[] = {"assign", "start", "complete", "reject", "cancel", "escalate"};
    if (std::find(std::begin(known), std::end(known), action) == std::end(known))
        throw std::invalid_argument("Unknown action for an access rule: " + action);
    product->accessPolicy.restrict(action, roles);
}
void ScenarioBuilder::buildDeadline(const std::string& item, int duration) {
    requireItem(item);
    if (duration <= 0) throw std::invalid_argument("A deadline must be at least one clock unit");
    product->decorations[item].deadline = duration;
}
void ScenarioBuilder::buildAudit(const std::string& item) {
    requireItem(item);
    product->decorations[item].audit = true;
}
void ScenarioBuilder::buildApprovalChain(std::unique_ptr<ApprovalHandler> chain) {
    requireProduct();
    if (!chain) throw std::invalid_argument("Approval chain cannot be null");
    product->approvalChain = std::move(chain);
}
void ScenarioBuilder::buildApproval(const std::string& item, const std::string& valueField,
                                    const std::string& resultField) {
    requireItem(item);
    if (valueField.empty() || resultField.empty())
        throw std::invalid_argument("An approval needs a value field and a result field");
    product->approvals[item] = WorkflowDefinition::ApprovalRule{valueField, resultField};
}
void ScenarioBuilder::buildInterrupt(const std::string& eventType, const std::vector<std::string>& cancel,
                                     const std::vector<std::string>& open) {
    requireProduct();
    if (eventType.empty() || (cancel.empty() && open.empty()))
        throw std::invalid_argument("Interrupt needs an event type and something to cancel or open");
    for (const auto& item : cancel) requireItem(item);
    for (const auto& item : open) requireItem(item);
    for (const auto& rule : product->interrupts)
        if (rule.eventType == eventType) throw std::invalid_argument("Interrupt already defined: " + eventType);
    product->interrupts.push_back(WorkflowDefinition::InterruptRule{eventType, cancel, open});
}
std::unique_ptr<WorkflowDefinition> ScenarioBuilder::getResult() {
    requireProduct();
    if (items.empty() || product->entryItems.empty() || !product->assignmentRule)
        throw std::logic_error("Definition needs work, entry items and an assignment rule");
    if (!product->approvals.empty() && !product->approvalChain)
        throw std::logic_error("Definition has approval items but no approval chain");
    std::set<std::string> visited;
    std::vector<std::string> pending = product->entryItems;
    bool runnableEntry = false;
    for (const auto& item : pending)
        if (product->dependencies.prerequisitesOf(item).empty()) runnableEntry = true;
    if (!runnableEntry) throw std::logic_error("No entry item can start");
    for (const auto& rule : product->interrupts) pending.insert(pending.end(), rule.open.begin(), rule.open.end());
    while (!pending.empty()) {
        const auto item = pending.back(); pending.pop_back();
        if (!visited.insert(item).second) continue;
        const auto found = edges.find(item);
        if (found != edges.end()) pending.insert(pending.end(), found->second.begin(), found->second.end());
        const auto onReject = rejectionEdges.find(item);
        if (onReject != rejectionEdges.end()) pending.insert(pending.end(), onReject->second.begin(), onReject->second.end());
    }
    if (visited.size() != items.size()) throw std::logic_error("Definition contains unreachable work");
    stages.clear(); items.clear(); edges.clear(); rejectionEdges.clear(); defaults.clear(); routing = nullptr;
    return std::move(product);
}

void ScenarioBuilder::buildIdentityField(const std::string& field) {
    requireProduct();
    if (field.empty()) throw std::invalid_argument("Identity field cannot be empty");
    product->identityField = field;
}
void ScenarioBuilder::buildCancellationItem(const std::string& item) {
    requireItem(item);
    product->cancellationItems.insert(item);
}
void ScenarioBuilder::buildExceptionRoute(const std::string& splitsFrom, const std::vector<std::string>& items) {
    requireItem(splitsFrom);
    if (items.empty()) throw std::invalid_argument("An exception route needs at least one item");
    for (std::size_t i = 0; i < items.size(); ++i) requireItem(items[i]);
    WorkflowDefinition::ExceptionRoute route;
    route.splitsFrom = splitsFrom;
    route.items = items;
    product->exceptionRoutes.push_back(route);
}
