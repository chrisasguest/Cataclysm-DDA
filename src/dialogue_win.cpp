#include "dialogue_win.h"

#include "debug.h"
#include "game.h"
#include "input.h"
#include "map.h"
#include "output.h"
#include "translations.h"
#include "ui.h"

#include <algorithm>
#include <string>
#include <vector>

void dialogue_window::open_dialogue( bool text_only )
{
    if( text_only ) {
        this->text_only = true;
        return;
    }
    int win_beginy = ( TERMY > FULL_SCREEN_HEIGHT ) ? ( TERMY - FULL_SCREEN_HEIGHT ) / 4 : 0;
    int win_beginx = ( TERMX > FULL_SCREEN_WIDTH ) ? ( TERMX - FULL_SCREEN_WIDTH ) / 4 : 0;
    int maxy = win_beginy ? TERMY - 2 * win_beginy : FULL_SCREEN_HEIGHT;
    int maxx = win_beginx ? TERMX - 2 * win_beginx : FULL_SCREEN_WIDTH;
    d_win = catacurses::newwin( maxy, maxx, win_beginy, win_beginx );
}

void dialogue_window::print_header( const std::string &name )
{
    if( text_only ) {
        return;
    }
    draw_border( d_win );
    int win_midx = getmaxx( d_win ) / 2;
    int winy = getmaxy( d_win );
    mvwvline( d_win, 1, win_midx + 1, LINE_XOXO, winy - 1 );
    mvwputch( d_win, 0, win_midx + 1, BORDER_COLOR, LINE_OXXX );
    mvwputch( d_win, winy - 1, win_midx + 1, BORDER_COLOR, LINE_XXOX );
    mvwprintz( d_win, 1,  1, c_white, _( "Dialogue: %s" ), name.c_str() );
    mvwprintz( d_win, 1, win_midx + 3, c_white, _( "Your response:" ) );
    npc_name = name;
}

void dialogue_window::clear_window_texts()
{
    if( text_only ) {
        return;
    }
    werase( d_win );
    print_header( npc_name );
}

size_t dialogue_window::add_to_history( const std::string &text )
{
    auto const folded = foldstring( text, getmaxx( d_win ) / 2 );
    history.insert( history.end(), folded.begin(), folded.end() );
    return folded.size();
}

// Empty line between lines of dialogue
void dialogue_window::add_history_separator()
{
    history.push_back( "" );
}

void dialogue_window::print_history( size_t const hilight_lines )
{
    if( text_only ) {
        return;
    }
    int curline = getmaxy( d_win ) - 2;
    int curindex = history.size() - 1;
    // index of the first line that is highlighted
    int newindex = history.size() - hilight_lines;
    // Print at line 2 and below, line 1 contains the header, line 0 the border
    while( curindex >= 0 && curline >= 2 ) {
        // red for new text, gray for old, similar to coloring of messages
        nc_color const col = ( curindex >= newindex ) ? c_red : c_dark_gray;
        mvwprintz( d_win, curline, 1, col, history[curindex] );
        curline--;
        curindex--;
    }
}

// Number of lines that can be used for the list of responses:
// -2 for border, -2 for options that are always there, -1 for header
static int RESPONSE_AREA_HEIGHT( int win_height )
{
    return win_height - 2 - 2 - 1;
}

bool dialogue_window::print_responses( int const yoffset, const std::vector<talk_data> &responses )
{
    if( text_only ) {
        return false;
    }
    // Responses go on the right side of the window, add 2 for spacing
    size_t const xoffset = getmaxx( d_win ) / 2 + 2;
    // First line we can print to, +2 for borders, +1 for the header.
    int const min_line = 2 + 1;
    // Bottom most line we can print to
    int const max_line = min_line + RESPONSE_AREA_HEIGHT( getmaxy( d_win ) ) - 1;

    // Remaining width of the responses area, -2 for the border, -2 for indentation, -2 for spacing
    size_t const fold_width = xoffset - 2 - 2 - 2;

    int curline = min_line - ( int ) yoffset;
    for( size_t i = 0; i < responses.size() && curline <= max_line; i++ ) {
        const std::vector<std::string> folded = foldstring( responses[i].second, fold_width );
        const nc_color &color = responses[i].first;
        for( size_t j = 0; j < folded.size(); j++, curline++ ) {
            if( curline < min_line ) {
                continue;
            } else if( curline > max_line ) {
                break;
            }
            int const off = ( j != 0 ) ? +3 : 0;
            mvwprintz( d_win, curline, xoffset + off, color, folded[j] );
        }
    }
    // Those are always available, their key bindings are fixed as well.
    mvwprintz( d_win, curline + 1, xoffset, c_magenta, _( "Shift+L: Look at" ) );
    mvwprintz( d_win, curline + 2, xoffset, c_magenta, _( "Shift+S: Size up stats" ) );
    mvwprintz( d_win, curline + 3, xoffset, c_magenta, _( "Shift+Y: Yell" ) );
    mvwprintz( d_win, curline + 4, xoffset, c_magenta, _( "Shift+O: Check opinion" ) );
    return curline > max_line; // whether there is more to print.
}

void dialogue_window::refresh_response_display()
{
    yoffset = 0;
    can_scroll_down = false;
    can_scroll_up = false;
}

void dialogue_window::display_responses( int const hilight_lines,
        const std::vector<talk_data> &responses,
        const long &ch )
{
    if( text_only ) {
        return;
    }
#ifdef __ANDROID__
    input_context ctxt( "DIALOGUE_CHOOSE_RESPONSE" );
    for( size_t i = 0; i < responses.size(); i++ ) {
        ctxt.register_manual_key( 'a' + i );
    }
    ctxt.register_manual_key( 'L', "Look at" );
    ctxt.register_manual_key( 'S', "Size up stats" );
    ctxt.register_manual_key( 'Y', "Yell" );
    ctxt.register_manual_key( 'O', "Check opinion" );
#endif
    // adjust scrolling from the last key pressed
    int win_maxy = getmaxy( d_win );
    switch( ch ) {
        case KEY_DOWN:
        case KEY_NPAGE:
            if( can_scroll_down ) {
                yoffset += RESPONSE_AREA_HEIGHT( win_maxy );
            }
            break;
        case KEY_UP:
        case KEY_PPAGE:
            if( can_scroll_up ) {
                yoffset = std::max( 0, yoffset - RESPONSE_AREA_HEIGHT( win_maxy ) );
            }
            break;
        default:
            break;
    }
    clear_window_texts();
    print_history( hilight_lines );
    can_scroll_down = print_responses( yoffset, responses );
    can_scroll_up = yoffset > 0;
    if( can_scroll_up ) {
        mvwprintz( d_win, 2, getmaxx( d_win ) - 2 - 2, c_green, "^^" );
    }
    if( can_scroll_down ) {
        mvwprintz( d_win, win_maxy - 2, FULL_SCREEN_WIDTH - 2 - 2, c_green, "vv" );
    }
    wrefresh( d_win );
}
