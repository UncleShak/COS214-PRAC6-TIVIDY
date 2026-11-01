/**
 * @file Theme.h
 * @brief The New York subway palette for the ncurses front end.
 *
 * Black background, a dark grey band for signs, white text and one line colour per
 * industry: blue (25/33) for the first industry, orange (202) for the second. Switching
 * industry re-runs applyTheme(), so the track, selection bars and focused borders all
 * change colour together. Terminals with fewer than 256 colours get the nearest basic
 * colours, and terminals without colour use bold, dim and reverse only.
 *
 * @author Shakir Alexander (Lust)
 */
#ifndef TIVIDY_THEME_H
#define TIVIDY_THEME_H

#include <string>

namespace tividy {
namespace ui {

/** @brief What a piece of text means, which decides its colour. */
enum class Tone {
    Text,         ///< main text
    TextBold,     ///< main text, bold (Available)
    Bright,       ///< bold white (done stops, current label)
    Muted,        ///< labels, hints, Created, upcoming stops
    Dim,          ///< disabled items, track not yet travelled, Cancelled
    Line,         ///< line text colour, bold: Started, current stop, accents
    Green,        ///< Complete
    Red,          ///< Rejected, exception route, errors
    Purple,       ///< Assigned
    Escalated,    ///< black on yellow block
    Select,       ///< selection bar: bold white on the line colour
    SelectBullet, ///< a key bullet inside the selection bar: line colour on white
    Band,         ///< text on the band
    BandBright,   ///< bold white on the band
    BandMuted,    ///< muted on the band
    BandRed,      ///< bold red on the band ("Stand clear:")
    Plate,        ///< focused title plate: bold white on the line colour
    PlateBand,    ///< unfocused title plate: bold white on the band
    Key,          ///< key bullet: 252 on key grey, bold
    KeyMuted,     ///< inactive industry bullet: muted on key grey
    Border,       ///< unfocused border (rule grey)
    BorderFocus,  ///< focused border (line text colour)
    Track,        ///< travelled track, line colour on black
    BulletLine,   ///< white on the line colour
    BulletGreen,  ///< white on green
    BulletRed,    ///< white on red
    BulletPurple, ///< white on purple
    Bullet1,      ///< the first industry's route bullet: white on blue (always)
    Bullet2       ///< the second industry's route bullet: white on orange (always)
};

/** @brief The colour indices the theme is built from: plain data, one per role. */
struct Palette {
    short bg, band, text, white, muted, dim, key, keyText, rule;
    short line, lineText;   ///< the active industry's line
    short green, red, yellow, purple, black;
    short line1, line2;     ///< the two industries' line colours, for their route bullets

    /** @brief The 256-colour subway palette for industry @p industryIndex (0 blue, 1 orange). */
    static Palette subway(int industryIndex);
    /** @brief The nearest basic colours, for terminals with fewer than 256. */
    static Palette basic(int industryIndex);
};

/** @brief Colour pairs and the state-to-colour mapping. */
class Theme {
public:
    /** @brief Remembers whether colour is available, then applies industry 0. Called by CursesSession. */
    static void init(bool colour);
    /**
     * @brief (Re)creates every colour pair with @p industryIndex's line colour.
     *        Industries alternate blue and orange; cells already drawn change on the next refresh.
     */
    static void applyTheme(int industryIndex);
    /** @brief The industry index applyTheme() last used. */
    static int industry();
    /** @brief Fills @p window with the black background (call after creating it). */
    static void paint(void* window);
    /** @brief Attribute to pass to wattron/wattroff for @p tone. */
    static int attr(Tone tone);
    /** @brief Route bullet tone of industry @p industryIndex. */
    static Tone industryBullet(int industryIndex);
    /**
     * @brief Tone for a work item, from its state name and tracker status.
     *        Waiting items are muted even though their state is Created.
     */
    static Tone forState(const std::string& state, const std::string& trackerStatus);

private:
    Theme();
};

} // namespace ui
} // namespace tividy

#endif
