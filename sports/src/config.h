#pragma once

#define SPORTS_DEFAULT_COLOR GColorVividCerulean

#define IN_APP_CONFIG 0

// Requires pebble.h; include this header after it.

// Two layout size classes, mirroring firmware's PreferredContentSizeDefault
// (see SPORTS_SIZE_SWITCH in PebbleOS's sports_layout.c):
//   medium: aplite/basalt/diorite/flint 144x168, chalk 180x180
//   large:  emery 200x228, gabbro 260x260
// Combine with PBL_IF_RECT_ELSE for the shape axis, e.g.
//   SIZE_SWITCH(PBL_IF_RECT_ELSE(59, 35), PBL_IF_RECT_ELSE(75, 50))
// Use SIZE_IS_LARGE in #if when the branches can't both compile, such as
// resource IDs that only exist on one size class.
#if PBL_DISPLAY_WIDTH >= 200
#define SIZE_IS_LARGE 1
#define SIZE_SWITCH(medium, large) (large)
#else
#define SIZE_IS_LARGE 0
#define SIZE_SWITCH(medium, large) (medium)
#endif

// Shorthand for the common rect/round x medium/large four-way.
#define LAYOUT_SWITCH(rect_medium, round_medium, rect_large, round_large) \
  SIZE_SWITCH(PBL_IF_RECT_ELSE(rect_medium, round_medium), \
              PBL_IF_RECT_ELSE(rect_large, round_large))

// Status/header strip font, shared by win-card's status bar and win-games'
// header.
#define STATUS_FONT SIZE_SWITCH(FONT_KEY_GOTHIC_14, FONT_KEY_GOTHIC_18)

// Vertical nudge for the status/header strip, shared by win-card's hand-drawn
// status bar and win-games' header TextLayer so the two agree. Both APIs place
// the text *box*, not the ink, so this cancels the font's internal leading
// (GOTHIC_18 leads 7px, GOTHIC_14 leads 5px). Round platforms take a positive
// inset instead, to clear the curved top edge.
#define STATUS_TEXT_OFFSET LAYOUT_SWITCH(-1, 4, -2, 4)

// Loading bar shown while a league or pin list is fetched (win-card, win-pins).
// Centred on round; fixed boxes on rect, tuned to sit under the content.
#define PROGRESS_RECT(bounds) \
  LAYOUT_SWITCH(GRect(32, 80, 80, 6), \
                GRect((bounds).size.w / 4, (bounds).size.h / 2, (bounds).size.w / 2, 6), \
                GRect(44, 109, 112, 8), \
                GRect((bounds).size.w / 4, (bounds).size.h / 2, (bounds).size.w / 2, 8))
