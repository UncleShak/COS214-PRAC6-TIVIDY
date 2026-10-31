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
const short kSelectPair = 5;
const short kTitlePair = 6;
const short kStripPair = 7;
const short kBannerPair = 8;

} // namespace

void Theme::init(bool colour) {
    useColour = colour;
    if (!colour) {
        return;
    }
    if (COLORS >= 256) {
        const short navy = 17;   // the Persona 3 Reload deep blue
        init_pair(kBonePair, 255, navy);
        init_pair(kGoldPair, 45, navy);    // cyan accent
        init_pair(kMutedPair, 67, navy);   // steel blue
        init_pair(kRedPair, 197, navy);    // crimson
        init_pair(kSelectPair, navy, 45);
        init_pair(kTitlePair, 231, 25);
        init_pair(kStripPair, 159, 18);
        init_pair(kBannerPair, 231, 160);
    } else {
        init_pair(kBonePair, COLOR_WHITE, COLOR_BLUE);
        init_pair(kGoldPair, COLOR_CYAN, COLOR_BLUE);
        init_pair(kMutedPair, COLOR_WHITE, COLOR_BLUE);
        init_pair(kRedPair, COLOR_RED, COLOR_BLUE);
        init_pair(kSelectPair, COLOR_BLUE, COLOR_CYAN);
        init_pair(kTitlePair, COLOR_WHITE, COLOR_CYAN);
        init_pair(kStripPair, COLOR_CYAN, COLOR_BLUE);
        init_pair(kBannerPair, COLOR_WHITE, COLOR_RED);
    }
}

void Theme::paint(void* window) {
    WINDOW* w = static_cast<WINDOW*>(window);
    if (w != 0) {
        wbkgd(w, useColour ? (COLOR_PAIR(kBonePair) | ' ') : ' ');
    }
}

int Theme::attr(Tone tone) {
    if (!useColour) {
        switch (tone) {
        case Tone::Gold:   return A_BOLD;
        case Tone::Muted:  return A_DIM;
        case Tone::Red:    return A_BOLD | A_UNDERLINE;
        case Tone::Select: return A_REVERSE | A_BOLD;
        case Tone::Title:  return A_REVERSE;
        case Tone::Strip:  return A_NORMAL;
        case Tone::Banner: return A_REVERSE | A_BOLD;
        case Tone::Bone:   return A_NORMAL;
        }
        return A_NORMAL;
    }
    switch (tone) {
    case Tone::Gold:   return COLOR_PAIR(kGoldPair) | A_BOLD;
    case Tone::Muted:  return COLOR_PAIR(kMutedPair) | (COLORS >= 256 ? 0 : A_DIM);
    case Tone::Red:    return COLOR_PAIR(kRedPair) | A_BOLD;
    case Tone::Select: return COLOR_PAIR(kSelectPair) | A_BOLD;
    case Tone::Title:  return COLOR_PAIR(kTitlePair) | A_BOLD;
    case Tone::Strip:  return COLOR_PAIR(kStripPair);
    case Tone::Banner: return COLOR_PAIR(kBannerPair) | A_BOLD;
    case Tone::Bone:   return COLOR_PAIR(kBonePair);
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
