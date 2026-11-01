/**
 * @file ViewModels.h
 * @brief What the interface controller hands to the front ends to draw.
 *
 * Front-end neutral: the ncurses and plain front ends render the same data.
 *
 * @author Shakir Alexander (Lust)
 */
#ifndef TIVIDY_VIEW_MODELS_H
#define TIVIDY_VIEW_MODELS_H

#include <string>
#include <vector>

#include "EngineGateway.h"

namespace tividy {
namespace ui {

/** @brief One line of feedback for the status bar. */
struct StatusMessage {
    /** @brief How the front end colours the message. */
    enum class Kind { None, Info, Success, Error };

    Kind kind;
    std::string text;

    StatusMessage() : kind(Kind::None) {}
    StatusMessage(Kind k, const std::string& t) : kind(k), text(t) {}

    bool isError() const { return kind == Kind::Error; }

    static StatusMessage none() { return StatusMessage(); }
    static StatusMessage info(const std::string& t) { return StatusMessage(Kind::Info, t); }
    static StatusMessage success(const std::string& t) { return StatusMessage(Kind::Success, t); }
    /** @brief The user typed or picked something unusable. Nothing was sent to the engine. */
    static StatusMessage invalidInput(const std::string& t) {
        return StatusMessage(Kind::Error, "Invalid input: " + t);
    }
    /** @brief The engine refused or failed. Shows the exception's what() unchanged. */
    static StatusMessage engineError(const std::string& what) {
        return StatusMessage(Kind::Error, what);
    }
};

/**
 * @brief Everything currently on screen, as last fetched from the engine.
 *
 * Rebuilt by InterfaceController::refresh() after every action, so it always matches the
 * engine, including after a refused action.
 */
struct SessionView {
    std::vector<IndustryInfo> industries;

    std::string industryId;                    ///< "" until an industry is chosen
    std::vector<InstanceView> instances;       ///< of the current industry
    std::vector<ParticipantView> participants; ///< of the current industry
    std::string actorId;                       ///< who performs actions

    std::string instanceId;                    ///< "" until an instance is chosen
    InstanceView instance;                     ///< valid when instanceId is not empty
    std::vector<WorkItemView> workItems;
    std::vector<WorkItemView> availableWork;
    std::vector<ApprovalView> approvals;       ///< of the selected instance

    int clock;                                 ///< engine time

    SessionView() : clock(0) {}
};

} // namespace ui
} // namespace tividy

#endif
