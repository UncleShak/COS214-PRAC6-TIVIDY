/**
 * @file Theme.h
 * @brief The Se7en colour palette for the ncurses front end.
 *
 * 256-colour terminals (Windows Terminal) get the team palette: bone 230, gold 179,
 * muted 101, red 160. Terminals with fewer colours fall back to the 8 basic colours,
 * and terminals without colour use bold and dim only.
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
    Red    ///< rejected, cancelled, escalated, errors
};

/** @brief Colour pairs and the state-to-colour mapping. */
class Theme {
public:
    /** @brief Creates the colour pairs. Called once by CursesSession. */
    static void init(bool colour);
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
