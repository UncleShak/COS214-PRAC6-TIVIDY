#include "Theme.h"

#include <cstddef>

#include <curses.h>

namespace tividy {
namespace ui {

namespace {

bool useColour = false;
int activeIndustry = 0;

/** One colour pair per tone, numbered from 1 in Tone order. */
short pairOf(Tone tone) {
    return static_cast<short>(static_cast<int>(tone) + 1);
}

} // namespace

Palette Palette::subway(int industryIndex) {
    Palette p;
    p.bg = 16;
    p.band = 235;
    p.text = 255;
    p.white = 231;
    p.muted = 244;
    p.dim = 239;
    p.key = 238;
    p.keyText = 252;
    p.rule = 240;
    p.green = 34;
    p.red = 196;
    p.yellow = 220;
    p.purple = 134;
    p.black = 16;
    p.line1 = 25;
    p.line2 = 202;
    if (industryIndex % 2 == 0) {
        p.line = 25;
        p.lineText = 33;
    } else {
        p.line = 202;
        p.lineText = 202;
    }
    return p;
}

Palette Palette::basic(int industryIndex) {
    Palette p;
    p.bg = COLOR_BLACK;
    p.band = COLOR_BLACK;
    p.text = COLOR_WHITE;
    p.white = COLOR_WHITE;
    p.muted = COLOR_WHITE;
    p.dim = COLOR_WHITE;
    p.key = COLOR_WHITE;
    p.keyText = COLOR_BLACK;
    p.rule = COLOR_WHITE;
    p.green = COLOR_GREEN;
    p.red = COLOR_RED;
    p.yellow = COLOR_YELLOW;
    p.purple = COLOR_MAGENTA;
    p.black = COLOR_BLACK;
    p.line1 = COLOR_BLUE;
    p.line2 = COLOR_CYAN;
    p.line = industryIndex % 2 == 0 ? COLOR_BLUE : COLOR_CYAN;
    p.lineText = p.line;
    return p;
}

void Theme::init(bool colour) {
    useColour = colour;
    applyTheme(0);
}

void Theme::applyTheme(int industryIndex) {
    activeIndustry = industryIndex < 0 ? 0 : industryIndex;
    if (!useColour) {
        return;
    }
    const Palette p = COLORS >= 256 ? Palette::subway(activeIndustry) : Palette::basic(activeIndustry);
    struct PairSpec { Tone tone; short fg; short bg; };
    const PairSpec pairs[] = {
        {Tone::Text, p.text, p.bg},
        {Tone::TextBold, p.text, p.bg},
        {Tone::Bright, p.white, p.bg},
        {Tone::Muted, p.muted, p.bg},
        {Tone::Dim, p.dim, p.bg},
        {Tone::Line, p.lineText, p.bg},
        {Tone::Green, p.green, p.bg},
        {Tone::Red, p.red, p.bg},
        {Tone::Purple, p.purple, p.bg},
        {Tone::Escalated, p.black, p.yellow},
        {Tone::Select, p.white, p.line},
        {Tone::SelectBullet, p.line, p.white},
        {Tone::Band, p.text, p.band},
        {Tone::BandBright, p.white, p.band},
        {Tone::BandMuted, p.muted, p.band},
        {Tone::BandRed, p.red, p.band},
        {Tone::Plate, p.white, p.line},
        {Tone::PlateBand, p.white, p.band},
        {Tone::Key, p.keyText, p.key},
        {Tone::KeyMuted, p.muted, p.key},
        {Tone::Border, p.rule, p.bg},
        {Tone::BorderFocus, p.lineText, p.bg},
        {Tone::Track, p.line, p.bg},
        {Tone::BulletLine, p.white, p.line},
        {Tone::BulletGreen, p.white, p.green},
        {Tone::BulletRed, p.white, p.red},
        {Tone::BulletPurple, p.white, p.purple},
        {Tone::Bullet1, p.white, p.line1},
        {Tone::Bullet2, p.white, p.line2},
    };
    for (std::size_t i = 0; i < sizeof(pairs) / sizeof(pairs[0]); ++i) {
        init_pair(pairOf(pairs[i].tone), pairs[i].fg, pairs[i].bg);
    }
}

int Theme::industry() {
    return activeIndustry;
}

void Theme::paint(void* window) {
    WINDOW* w = static_cast<WINDOW*>(window);
    if (w != 0) {
        wbkgd(w, useColour ? (COLOR_PAIR(pairOf(Tone::Text)) | ' ') : ' ');
    }
}

int Theme::attr(Tone tone) {
    if (!useColour) {
        switch (tone) {
        case Tone::TextBold:
        case Tone::Bright:
        case Tone::Line:
        case Tone::BandBright:
        case Tone::Red:
        case Tone::BandRed:      return A_BOLD;
        case Tone::Muted:
        case Tone::Dim:
        case Tone::Border:
        case Tone::BandMuted:    return A_DIM;
        case Tone::Escalated:
        case Tone::Select:
        case Tone::Plate:
        case Tone::BulletLine:
        case Tone::BulletGreen:
        case Tone::BulletRed:
        case Tone::BulletPurple:
        case Tone::Bullet1:
        case Tone::Bullet2:      return A_REVERSE | A_BOLD;
        case Tone::Key:
        case Tone::KeyMuted:
        case Tone::PlateBand:    return A_REVERSE;
        default:                 return A_NORMAL;
        }
    }
    const int pair = COLOR_PAIR(pairOf(tone));
    switch (tone) {
    case Tone::TextBold:
    case Tone::Bright:
    case Tone::Line:
    case Tone::Red:
    case Tone::Escalated:
    case Tone::Select:
    case Tone::SelectBullet:
    case Tone::BandBright:
    case Tone::BandRed:
    case Tone::Plate:
    case Tone::PlateBand:
    case Tone::Key:
    case Tone::BulletLine:
    case Tone::BulletGreen:
    case Tone::BulletRed:
    case Tone::BulletPurple:
    case Tone::Bullet1:
    case Tone::Bullet2:
        return pair | A_BOLD;
    case Tone::Muted:
    case Tone::Dim:
    case Tone::BandMuted:
        return pair | (COLORS >= 256 ? 0 : A_DIM);
    default:
        return pair;
    }
}

Tone Theme::industryBullet(int industryIndex) {
    return industryIndex % 2 == 0 ? Tone::Bullet1 : Tone::Bullet2;
}

Tone Theme::forState(const std::string& state, const std::string& trackerStatus) {
    if (trackerStatus == "waiting") return Tone::Muted;
    if (state == "Created")   return Tone::Muted;
    if (state == "Available") return Tone::TextBold;
    if (state == "Assigned")  return Tone::Purple;
    if (state == "Started")   return Tone::Line;
    if (state == "Completed") return Tone::Green;
    if (state == "Escalated") return Tone::Escalated;
    if (state == "Rejected")  return Tone::Red;
    if (state == "Cancelled") return Tone::Dim;
    return Tone::Text;
}

} // namespace ui
} // namespace tividy
