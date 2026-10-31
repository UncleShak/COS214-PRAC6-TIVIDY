/**
 * @file Theme.h
 * @brief The Persona-style palette for the ncurses front end.
 *
 * 256-colour terminals (xterm-256color, Windows Terminal) get deep navy (17) as the
 * background, white (255) text, cyan (45) accents, steel blue (67) for secondary text and
 * crimson (197) for alerts. Terminals with 8 colours use blue/cyan/white/red, and terminals
 * without colour use bold, dim and reverse only.
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
    Bone,  ///< normal text
    Gold,  ///< active work, selection, focus
    Muted, ///< done, waiting or secondary
    Red,   ///< rejected, cancelled, escalated, errors
    Select, ///< the selection bar: navy on cyan
    Title,  ///< title band and panel tabs: white on blue
    Strip,  ///< calendar strip and ticker: pale text on dark blue
    Banner  ///< full-screen alert: white on red
};

/** @brief Colour pairs and the state-to-colour mapping. */
class Theme {
public:
    /** @brief Creates the colour pairs. Called once by CursesSession. */
    static void init(bool colour);
    /** @brief Fills @p window with the navy background (call after creating it). */
    static void paint(void* window);
    /** @brief Attribute to pass to wattron/wattroff for @p tone. */
    static int attr(Tone tone);
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
