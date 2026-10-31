/**
 * @file Widgets.h
 * @brief Drawing helpers and the modal dialogs of the ncurses front end.
 *
 * Every dialog runs its own key loop and returns when the user confirms or cancels.
 * A terminal resize cancels the open dialog and is passed back to the main loop
 * (ungetch(KEY_RESIZE)), which rebuilds the layout; nothing is sent to the engine.
 *
 * @author Shakir Alexander (Lust)
 */
#ifndef TIVIDY_WIDGETS_H
#define TIVIDY_WIDGETS_H

#include <cstddef>
#include <map>
#include <string>
#include <vector>

#include <curses.h>

#include "EngineGateway.h"
#include "InputValidation.h"
#include "Theme.h"

namespace tividy {
namespace ui {

/** @brief One line of text and its colour. */
struct Line {
    std::string text;
    Tone tone;

    Line() : tone(Tone::Bone) {}
    Line(const std::string& t, Tone c = Tone::Bone) : text(t), tone(c) {}
};

/** @brief Writes @p text at (y, x), cut to fit the window width minus @p margin. */
void putText(WINDOW* window, int y, int x, const std::string& text, Tone tone, int margin = 1);

/** @brief Box with a slanted title tab; cyan border when the window has focus. */
void drawFrame(WINDOW* window, const std::string& title, bool focused);

/** @brief Writes UTF-8 decoration (block and box characters) at (y, x) without cutting it. */
void putGlyphs(WINDOW* window, int y, int x, const char* glyphs, Tone tone);

/** @brief One list row: a cyan selection bar with a pointer when @p chosen. */
void putRow(WINDOW* window, int y, int x, int width, const std::string& text, Tone tone, bool chosen);

/** @brief @p text cut or padded with spaces to exactly @p width characters. */
std::string fit(const std::string& text, std::size_t width);

/** @brief True for the keys that mean "confirm" (Enter on every terminal). */
bool isEnter(int key);
/** @brief True for the keys that delete the last character. */
bool isBackspace(int key);

const int kKeyEscape = 27;

/** @brief Modal dialogs drawn over the panes. */
class Dialog {
public:
    /** @brief Returned by menu() when the user pressed Esc or the terminal was resized. */
    static const int kCancelled = -1;

    /**
     * @brief A vertical menu. Every entry can be chosen; the tone is only a hint
     *        (muted entries are actions the State pattern would refuse).
     * @return the chosen index, or kCancelled
     */
    static int menu(const std::string& title, const std::vector<Line>& entries, int initial = 0);

    /**
     * @brief A one-line text prompt.
     * @param value starts as the initial text; set to what was typed on success
     * @return false if cancelled with Esc
     */
    static bool prompt(const std::string& title, const std::string& label, std::string& value,
                       std::size_t maxLength = kMaxTextLength);

    /** @brief Yes/no question. Enter or y is yes; Esc or n is no. */
    static bool confirm(const std::string& question);

    /** @brief Scrollable read-only text. Esc, Enter, q or b closes it. */
    static void text(const std::string& title, const std::vector<Line>& lines);

    /**
     * @brief Full-screen alert (the Persona "all-out" moment): a red band with @p title in
     *        large spaced letters, @p subtitle under it, then @p lines. Any key closes it.
     */
    static void alert(const std::string& title, const std::string& subtitle, const std::vector<Line>& lines);

    /**
     * @brief The start-instance form: one input per field, validated with validateField()
     *        as the user leaves it, so a bad value is reported next to the field.
     * @param values set to the raw text typed, keyed by DataField::key
     * @return false if cancelled with Esc (nothing is started)
     */
    static bool form(const std::string& title, const std::vector<DataField>& fields,
                     std::map<std::string, std::string>& values);

private:
    Dialog();
};

} // namespace ui
} // namespace tividy

#endif
