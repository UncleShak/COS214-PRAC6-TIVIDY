#include "Theme.h"

#include <curses.h>

namespace tividy {
namespace ui {

namespace {
bool useColour = false;
const short kBonePair = 1;
const short kGoldPair = 2;
const short kMutedPair = 3;
const short kRedPair = 4;
} // namespace

void Theme::init(bool colour) {
    useColour = colour;
    if (!colour) {
        return;
    }
    // -1 keeps the terminal's own background (use_default_colors).
    if (COLORS >= 256) {
        init_pair(kBonePair, 230, -1);
        init_pair(kGoldPair, 179, -1);
        init_pair(kMutedPair, 101, -1);
        init_pair(kRedPair, 160, -1);
    } else {
        init_pair(kBonePair, COLOR_WHITE, -1);
        init_pair(kGoldPair, COLOR_YELLOW, -1);
        init_pair(kMutedPair, COLOR_WHITE, -1);
        init_pair(kRedPair, COLOR_RED, -1);
    }
}

int Theme::attr(Tone tone) {
    if (!useColour) {
        switch (tone) {
        case Tone::Gold:  return A_BOLD;
        case Tone::Muted: return A_DIM;
        case Tone::Red:   return A_BOLD | A_UNDERLINE;
        case Tone::Bone:  return A_NORMAL;
        }
        return A_NORMAL;
    }
    switch (tone) {
    case Tone::Gold:  return COLOR_PAIR(kGoldPair) | A_BOLD;
    case Tone::Muted: return COLOR_PAIR(kMutedPair) | (COLORS >= 256 ? 0 : A_DIM);
    case Tone::Red:   return COLOR_PAIR(kRedPair) | A_BOLD;
    case Tone::Bone:  return COLOR_PAIR(kBonePair);
    }
    return A_NORMAL;
}

Tone Theme::forState(const std::string& state, const std::string& trackerStatus) {
    if (state == "Rejected" || state == "Cancelled" || state == "Escalated") {
        return Tone::Red;
    }
    if (state == "Available" || state == "Assigned" || state == "Started") {
        return Tone::Gold;
    }
    if (state == "Completed" || trackerStatus == "waiting") {
        return Tone::Muted;
    }
    return Tone::Bone;
}

} // namespace ui
} // namespace tividy
