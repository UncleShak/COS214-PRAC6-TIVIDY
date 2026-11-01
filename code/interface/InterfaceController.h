/**
 * @file InterfaceController.h
 * @brief Turns user actions into engine calls and engine results into things to show.
 *
 * This is the interface's logic layer. It has no ncurses and no std::cin/std::cout, so it is
 * unit-tested directly (tests/lust) against FakeEngine, and both front ends share it.
 *
 * Named "controller" rather than "command layer" so it is not confused with the team's GoF
 * Command classes in code/command, which the engine runs on the interface's behalf.
 *
 * Rules every action follows:
 * - Free text and menu input is validated here first; bad input never reaches the engine
 *   and is reported as "Invalid input: ...".
 * - The controller never decides whether a transition is legal. It forwards the request,
 *   and an engine refusal is reported as "Not allowed: ...".
 * - After every call the view is re-fetched, so the screen matches the engine.
 *
 * @author Shakir Alexander (Lust)
 */
#ifndef TIVIDY_INTERFACE_CONTROLLER_H
#define TIVIDY_INTERFACE_CONTROLLER_H

#include <map>
#include <string>
#include <vector>

#include "EngineGateway.h"
#include "ViewModels.h"

namespace tividy {
namespace ui {

/** @brief Session state and every user action the interface offers. */
class InterfaceController {
public:
    /** @param engine must outlive the controller */
    explicit InterfaceController(EngineGateway& engine);

    /** @brief The data currently on screen. */
    const SessionView& view() const { return view_; }
    /** @brief Result of the most recent action, if any. */
    StatusMessage status() const { return status_; }

    /** @brief Re-fetches everything for the current industry and instance. */
    StatusMessage refresh();

    /** @brief Chooses the industry to work in. Clears the selected instance. */
    StatusMessage selectIndustry(const std::string& industryId);

    /** @brief Fields to ask for before starting @p definition in the current industry. */
    StatusMessage startFields(const std::string& definition, std::vector<DataField>& fields);

    /**
     * @brief Validates @p values against startFields() and starts an instance.
     *        On success the new instance becomes the selected one.
     * @param values raw user text, keyed by DataField::key
     */
    StatusMessage startInstance(const std::string& definition,
                                const std::map<std::string, std::string>& values);

    StatusMessage selectInstance(const std::string& instanceId);

    /** @brief Chooses who performs actions from now on. */
    StatusMessage setActor(const std::string& participantId);

    /**
     * @brief Performs @p action on @p workItemId in the selected instance as the current actor.
     * @param assigneeId Assign only; empty lets the engine's assignment rule choose
     * @param reason     Reject only; required
     */
    StatusMessage perform(WorkAction action, const std::string& workItemId,
                          const std::string& assigneeId = "", const std::string& reason = "");

    /** @brief Approves approval request @p approvalId of the selected instance, as the current actor. */
    StatusMessage approve(const std::string& approvalId);

    /** @brief Rejects approval request @p approvalId. @p reason is required. */
    StatusMessage rejectApproval(const std::string& approvalId, const std::string& reason);

    /**
     * @brief Moves the engine clock forward.
     * @param unitsText as typed; must be a whole number from 1 to 1000
     * @param changes   set to what the engine reports changed (e.g. escalations)
     */
    StatusMessage advance(const std::string& unitsText, std::vector<std::string>& changes);

    /** @brief Domain events the current industry reacts to (e.g. WORM_SIGN). */
    std::vector<std::string> eventTypes() const;
    /**
     * @brief Raises a domain event on the selected instance (Observer); interrupt rules react.
     * @param changes set to the work items whose state changed
     */
    StatusMessage raiseEvent(const std::string& type, const std::string& description,
                             std::vector<std::string>& changes);
    /** @brief What undo() would reverse in the selected instance, or "" if nothing. */
    std::string nextUndo() const;
    /** @brief Reverses the selected instance's most recent action (Memento). */
    StatusMessage undo();
    bool hasOutcome() const { return hasOutcome_; }
    /** @brief What the last successful perform() changed (state and routing). */
    const ActionOutcome& lastOutcome() const { return outcome_; }

    /**
     * @brief History of the selected instance, through the team's HistoryIterator.
     * @param filter only lines containing this text; empty for all
     */
    StatusMessage history(std::vector<std::string>& lines, bool newestFirst,
                          const std::string& filter);

    /** @brief The three visitor reports for the selected instance. */
    StatusMessage reports(std::string& progress, std::vector<std::string>& audit,
                          std::vector<std::string>& overdue);

    /**
     * @brief Actions the State pattern would accept from @p state, used only to grey out
     *        menu entries. The engine still decides; a greyed action can still be sent.
     */
    static std::vector<WorkAction> hintedActions(const std::string& state);

    /**
     * @brief The route map of @p definition in @p industryId: main line and exception branches.
     *        Empty if the engine gives none or refuses; the caller then draws every item.
     */
    std::vector<RouteStop> routeMap(const std::string& industryId, const std::string& definition) const;

    /** @brief Work items of any instance of the current industry (for overviews); empty if refused. */
    std::vector<WorkItemView> workItemsOf(const std::string& instanceId) const;

    /** @brief Display name of the participant with @p participantId, or "" if unknown. */
    std::string participantName(const std::string& participantId) const;

private:
    StatusMessage requireIndustry() const;
    StatusMessage requireInstance() const;
    /** @brief Records a successful outcome, refreshes, and builds the success message. */
    StatusMessage accept(const ActionOutcome& outcome);
    /** @brief Records a refusal (call inside a catch block), refreshes, returns the message. */
    StatusMessage refuse();

    EngineGateway& engine_;
    SessionView view_;
    StatusMessage status_;
    ActionOutcome outcome_;
    bool hasOutcome_;

    InterfaceController(const InterfaceController&);
    InterfaceController& operator=(const InterfaceController&);
};

} // namespace ui
} // namespace tividy

#endif
