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

/** @brief A run of text in one colour, part of a Line. */
struct Span {
    std::string text;
    Tone tone;

    Span(const std::string& t, Tone c) : text(t), tone(c) {}
};

/**
 * @brief One line of text and its colour. A line may instead be built from spans (a bullet,
 *        then a label in another colour), with more spans right-aligned (a state).
 */
struct Line {
    std::string text;
    Tone tone;
    std::vector<Span> spans; ///< drawn instead of text when not empty
    std::vector<Span> right; ///< drawn right-aligned

    Line() : tone(Tone::Text) {}
    Line(const std::string& t, Tone c = Tone::Text) : text(t), tone(c) {}

    /** @brief Appends a span on the left. @return this line, to chain calls */
    Line& add(const std::string& t, Tone c) { spans.push_back(Span(t, c)); return *this; }
    /** @brief Appends a span to the right-aligned part. @return this line */
    Line& addRight(const std::string& t, Tone c) { right.push_back(Span(t, c)); return *this; }
};

/**
 * @brief Draws @p line in the @p width cells starting at (y, x).
 * @param chosen fill the cells with the line-colour selection bar first; plain text becomes
 *               bold white, key bullets turn white with line-colour text, coloured bullets stay
 */
void putLine(WINDOW* window, int y, int x, int width, const Line& line, bool chosen);

/** @brief A route bullet: @p key with one space each side, e.g. " C ". */
std::string bullet(const std::string& key);

/** @brief Writes @p text (UTF-8) at (y, x), cut to fit the window width minus @p margin, in cells. */
void putText(WINDOW* window, int y, int x, const std::string& text, Tone tone, int margin = 1);

/**
 * @brief Rounded box (╭ ╮ ╰ ╯) with the title as a sign plate in the top edge, 2 columns in.
 *        Focused: line-text border and a plate on the line colour. Otherwise: rule-grey
 *        border and a plate on the band.
 * @param right optional summary at the right of the top edge, e.g. "2 of 10 stops"
 */
void drawFrame(WINDOW* window, const std::string& title, bool focused, const std::string& right = "");

/** @brief Fills @p width cells from (y, x) with spaces in @p tone (a bar or band). */
void fillRow(WINDOW* window, int y, int x, int width, Tone tone);

/** @brief Writes UTF-8 decoration (block and box characters) at (y, x) without cutting it. */
void putGlyphs(WINDOW* window, int y, int x, const char* glyphs, Tone tone);

/** @brief One list row: a line-colour selection bar when @p chosen. */
void putRow(WINDOW* window, int y, int x, int width, const std::string& text, Tone tone, bool chosen);

/** @brief @p text cut or padded with spaces to exactly @p width terminal cells (UTF-8 aware). */
std::string fit(const std::string& text, std::size_t width);

/** @brief Terminal cells @p text takes: one per UTF-8 code point (box and geometric glyphs are narrow). */
std::size_t cells(const std::string& text);

/** @brief The first @p width cells of @p text, never splitting a UTF-8 sequence. */
std::string cutCells(const std::string& text, std::size_t width);

/** @brief @p text broken into lines of at most @p width cells, at spaces where possible. */
std::vector<std::string> wrap(const std::string& text, std::size_t width);

/** @brief Plain lines longer than @p width are wrapped onto more lines in the same tone; span lines are kept. */
std::vector<Line> wrapLines(const std::vector<Line>& lines, std::size_t width);

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
