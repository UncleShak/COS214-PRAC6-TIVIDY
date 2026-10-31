/**
 * @file RealEngine.h
 * @brief The shipped EngineGateway: the interface's view of the real WorkflowEngine (Facade).
 *
 * Industries come from the IndustryCatalog (one Abstract Factory each) and are all installed
 * when the gateway is built. Only value structs and string ids cross to the interface.
 */
#ifndef TIVIDY_REAL_ENGINE_H
#define TIVIDY_REAL_ENGINE_H

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "EngineGateway.h"

class WorkflowEngine;
class WorkflowInstance;
class IndustryFactory;

namespace tividy {
namespace ui {

class RealEngine : public EngineGateway {
public:
    RealEngine();
    ~RealEngine() override;

    std::vector<IndustryInfo> listIndustries() const override;
    std::vector<DataField> startFields(const std::string& industryId, const std::string& definitionName) const override;
    std::string startInstance(const std::string& industryId, const std::string& definitionName,
                              const std::map<std::string, std::string>& initialData) override;
    std::vector<InstanceView> listInstances(const std::string& industryId) const override;
    InstanceView getInstance(const std::string& instanceId) const override;
    std::vector<WorkItemView> listWorkItems(const std::string& instanceId) const override;
    std::vector<WorkItemView> listAvailableWork(const std::string& instanceId) const override;
    WorkItemView getWorkItem(const std::string& instanceId, const std::string& workItemId) const override;
    std::vector<ParticipantView> listParticipants(const std::string& industryId) const override;
    ActionOutcome act(const ActionRequest& request) override;
    std::vector<std::string> history(const std::string& instanceId) const override;
    std::string progressSummary(const std::string& instanceId) const override;
    std::vector<std::string> auditReport(const std::string& instanceId) const override;
    std::vector<std::string> overdueWork(const std::string& instanceId) const override;
    std::vector<ApprovalView> listApprovals(const std::string& instanceId) const override;
    ActionOutcome approve(const std::string& instanceId, const std::string& approvalId,
                          const std::string& actorId) override;
    ActionOutcome rejectApproval(const std::string& instanceId, const std::string& approvalId,
                                 const std::string& actorId, const std::string& reason) override;
    std::vector<std::string> advanceClock(int units) override;
    int currentTime() const override;

    std::vector<std::string> eventTypes(const std::string& industryId) const override;
    std::vector<std::string> raiseEvent(const std::string& instanceId, const std::string& type,
                                        const std::string& description) override;
    std::string nextUndo(const std::string& instanceId) const override;
    std::string undo(const std::string& instanceId) override;

private:
    const IndustryFactory& industry(const std::string& industryId) const;
    WorkflowInstance& instance(const std::string& instanceId) const;
    WorkItemView itemView(const WorkflowInstance& inst, const std::string& name) const;
    InstanceView instanceView(const WorkflowInstance& inst) const;
    // Fills an outcome for workItem after an action that started from previousState.
    ActionOutcome outcomeFor(const WorkflowInstance& inst, const std::string& workItem,
                             const std::string& previousState, std::size_t commandsBefore) const;
    std::map<std::string, std::string> states(const WorkflowInstance& inst) const;

    std::unique_ptr<WorkflowEngine> engine;
    std::vector<std::unique_ptr<IndustryFactory> > factories;
    std::map<std::string, std::vector<std::string> > participantsByIndustry;
    std::map<std::string, std::string> industryOfInstance;
    std::map<std::string, int> nextNumber;

    RealEngine(const RealEngine&);
    RealEngine& operator=(const RealEngine&);
};

}  // namespace ui
}  // namespace tividy

#endif
