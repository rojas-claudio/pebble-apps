#include <pebble.h>
#include <libs/pebble-assist.h>
#include "generated/keys.h"
#include "win-games.h"
#include "win-card.h"
#include "card.h"
#include "sports.h"
#include "config.h"
#include "analytics.h"

static void click_config_provider(void *context);
static uint16_t menu_get_num_sections_callback(struct MenuLayer *menu_layer, void *context);
static uint16_t menu_get_num_rows_callback(struct MenuLayer *menu_layer, uint16_t section_index,
                                           void *context);
static int16_t menu_get_cell_height_callback(struct MenuLayer *menu_layer, MenuIndex *cell_index,
                                             void *context);
static void menu_draw_row_callback(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index,
                                   void *context);
static void menu_select_click_callback(struct MenuLayer *menu_layer, MenuIndex *cell_index,
                                       void *context);
static void window_load(Window *window);
static void window_unload(Window *window);

static Window *s_window = NULL;
static MenuLayer *s_menu_layer = NULL;
static TextLayer *s_header_text = NULL;

void win_games_init(void) {
  s_window = window_create();
  window_set_click_config_provider(s_window, click_config_provider);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
}

void win_games_push(void) {
  window_stack_push(s_window, true);
}

void win_games_deinit(void) {
  window_destroy_safe(s_window);
}

void win_games_mark_dirty(void) {
  if (!window_is_loaded(s_window)) {
    return;
  }
  menu_layer_reload_data(s_menu_layer);
  layer_mark_dirty(menu_layer_get_layer(s_menu_layer));
}

void win_games_set_selected_card(uint16_t row) {
  if (!window_is_loaded(s_window)) {
    return;
  }
  menu_layer_set_selected_index(s_menu_layer, (MenuIndex) { .section = sports_get_selected_league(),
                                                            .row = row },
                                MenuRowAlignCenter, false);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - //

static char* get_title_status() {
  if (sports_is_loading()) {
    return "LOADING";
  }
  return ((sports_get_selected_league() == 0 && !sports_get_has_teams()) ?
      ("FOLLOW TEAMS\nFROM GAME\nCARDS") :
      "NO GAMES\nTODAY");
}

static void down_up_click_handler(ClickRecognizerRef recognizer, void *context) {
  const bool is_up = (click_recognizer_get_button_id(recognizer) == BUTTON_ID_UP);
  if (sports_get_num_cards() != 0) {
    menu_layer_set_selected_next(s_menu_layer, is_up, MenuRowAlignCenter, true);
    return;
  }
  analytics_increment_screen_clicks(is_up);
}

// Shared by the SELECT button and a tap on the highlighted row.
static void select_game(MenuIndex cell_index) {
  if (sports_get_num_cards() > 0) {
    sports_set_selected_card(cell_index.row);
    analytics_set_screen(SCREEN_GAMECARD, BUTTON_ID_SELECT);
    win_card_push();
  }
  menu_layer_reload_data(s_menu_layer);
}

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  select_game(menu_layer_get_selected_index(s_menu_layer));
}

static void back_click_handler(ClickRecognizerRef recognizer, void *context) {
  window_stack_remove(s_window, true);
  analytics_set_screen(SCREEN_LEAGUES, BUTTON_ID_BACK);
  menu_layer_reload_data(s_menu_layer);
}

static void click_config_provider(void *context) {
  window_single_repeating_click_subscribe(BUTTON_ID_UP, 200, down_up_click_handler);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 200, down_up_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  window_single_click_subscribe(BUTTON_ID_BACK, back_click_handler);
}

static void set_compositing_mode(GContext *ctx) {
  graphics_context_set_compositing_mode(ctx, PBL_IF_COLOR_ELSE(GCompOpSet, GCompOpAssign));
}

#ifdef PBL_RECT
static void draw_games_row_rect(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index) {
  const uint16_t row = cell_index->row;
  const GRect bounds = layer_get_bounds(cell_layer);
  const GFont font_gothic_24_bold = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  const GFont name_font = fonts_get_system_font(SIZE_SWITCH(FONT_KEY_GOTHIC_24_BOLD,
                                                            FONT_KEY_GOTHIC_28_BOLD));
  set_compositing_mode(ctx);
  if (sports_get_num_cards() == 0) {
    char *title = get_title_status();
#ifdef PBL_BW
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_rect(ctx, bounds, 0, GCornerNone);
#endif
    graphics_context_set_text_color(ctx, COLOR_FALLBACK(GColorLightGray, GColorBlack));
    // Measured at GOTHIC_24_BOLD on every size, though emery draws at
    // GOTHIC_28_BOLD; emery's -7 was tuned against that measurement.
    const uint16_t title_height =
        graphics_text_layout_get_content_size(title, font_gothic_24_bold, bounds,
                                              GTextOverflowModeTrailingEllipsis,
                                              GTextAlignmentCenter).h;
    const uint16_t y_offset =
        (bounds.size.h - title_height) / 2 + SIZE_SWITCH(PBL_IF_COLOR_ELSE(10, -25), -7);
    graphics_draw_text(ctx, title, name_font, GRect(0, y_offset, bounds.size.w, bounds.size.h),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  } else {
    graphics_context_set_fill_color(ctx, menu_cell_layer_is_highlighted(cell_layer) ?
                                         COLOR_FALLBACK(SPORTS_DEFAULT_COLOR, GColorBlack) :
                                         GColorWhite);
    graphics_fill_rect(ctx, bounds, 0, GCornerNone);
    graphics_context_set_text_color(ctx, menu_cell_layer_is_highlighted(cell_layer) ? GColorWhite :
                                                                                      GColorBlack);

    const CardData *data = sports_get_card_data(row);
    const bool is_pregame = data->gameState == KEY_GAMESTATE_PREGAME;

    // The y values are box origins, not glyph positions. On 144px,
    // GOTHIC_24_BOLD at -4/15/39 in a 59px cell gives a 6/5/5/6 rhythm. emery
    // keeps it at GOTHIC_28_BOLD, which lays its ink 10px below the box top and
    // runs 18px: -2 puts the first row's caps 6px into the cell and 22 leaves a
    // 5px gap under them. emery's name box is ink (28) + 2 slack; at 26 it cut
    // the last pixel row off every name and score.
    const int16_t x = 9;
    const int16_t w = bounds.size.w - (x * 2);
    const int16_t away_y = SIZE_SWITCH(-4, -2);
    const int16_t home_y = SIZE_SWITCH(15, 22);
    const int16_t name_h = SIZE_SWITCH(26, 30);
    const GFont subtitle_font = fonts_get_system_font(SIZE_SWITCH(FONT_KEY_GOTHIC_14,
                                                                  FONT_KEY_GOTHIC_18));
    const GRect subtitle_rect = GRect(x, SIZE_SWITCH(39, bounds.size.h - 26), w, 20);

    graphics_draw_text(ctx, data->nameAway, name_font, GRect(x, away_y, w, name_h),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    graphics_draw_text(ctx, data->nameHome, name_font, GRect(x, home_y, w, name_h),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

    graphics_draw_text(ctx, is_pregame ? data->recordAway : data->scoreAway,
                       name_font, GRect(x, away_y, w, name_h),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);
    graphics_draw_text(ctx, is_pregame ? data->recordHome : data->scoreHome,
                       name_font, GRect(x, home_y, w, name_h),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);

    graphics_draw_text(ctx, data->subtitle, subtitle_font, subtitle_rect,
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);
    graphics_draw_text(ctx, data->league, subtitle_font, subtitle_rect,
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }
}

#else
static bool menu_layer_cell_is_selected(MenuIndex *cell_index) {
  const MenuIndex selected = menu_layer_get_selected_index(s_menu_layer);
  return (selected.section == cell_index->section && selected.row == cell_index->row);
}

static void draw_games_row_round(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index) {
  const uint16_t row = cell_index->row;
  const GRect bounds = layer_get_bounds(cell_layer);
  const GFont font_gothic_24_bold = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  const GFont name_font = fonts_get_system_font(SIZE_SWITCH(FONT_KEY_GOTHIC_24_BOLD,
                                                            FONT_KEY_GOTHIC_28_BOLD));

  set_compositing_mode(ctx);
  if (sports_get_num_cards() == 0) {
    char *title = get_title_status();
    graphics_context_set_text_color(ctx, COLOR_FALLBACK(GColorLightGray, GColorBlack));

    const uint16_t title_height =
        graphics_text_layout_get_content_size(title, name_font, bounds,
                                              GTextOverflowModeTrailingEllipsis,
                                              GTextAlignmentCenter).h;
    const uint16_t y_offset = (bounds.size.h / 2) - (title_height / 2);

    graphics_draw_text(ctx, title, name_font, GRect(0, y_offset, bounds.size.w, bounds.size.h),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  } else {
    graphics_context_set_fill_color(ctx, menu_cell_layer_is_highlighted(cell_layer) ?
                                         COLOR_FALLBACK(SPORTS_DEFAULT_COLOR, GColorBlack) :
                                         GColorWhite);
    graphics_fill_rect(ctx, bounds, 0, GCornerNone);
    graphics_context_set_text_color(ctx, menu_cell_layer_is_highlighted(cell_layer) ? GColorWhite :
                                                                                      GColorBlack);

    const CardData *data = sports_get_card_data(row);
    const int16_t text_buffer_length = 64;
    char text[text_buffer_length];
    const int16_t w = bounds.size.w;

    graphics_draw_text(ctx, "@", font_gothic_24_bold, GRect(0, 0, w, 26),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

    if (menu_layer_cell_is_selected(cell_index)) {
      bool is_pregame = data->gameState == KEY_GAMESTATE_PREGAME;
      // Team columns either side of the "@", pulled in from the curved edge.
      const int16_t col_w = SIZE_SWITCH(65, 94);
      const int16_t score_y = SIZE_SWITCH(20, 29);

      graphics_draw_text(ctx, data->nameAway, name_font, GRect(0, 0, col_w, 26),
                         GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);
      graphics_draw_text(ctx, data->nameHome, name_font, GRect(w - col_w, 0, col_w, 26),
                         GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

      graphics_draw_text(ctx, is_pregame ? data->recordAway : data->scoreAway,
                         name_font, GRect(0, score_y, col_w, 26),
                         GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);
      graphics_draw_text(ctx, is_pregame ? data->recordHome : data->scoreHome,
                         name_font, GRect(w - col_w, score_y, col_w, 26),
                         GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

      // On gabbro this is anchored to the cell bottom rather than a fixed y, so
      // it tracks any change to the cell height instead of silently drifting.
      snprintf(text, sizeof(text), "%s - %s", data->league, data->subtitle);
      graphics_draw_text(ctx, text,
                         fonts_get_system_font(SIZE_SWITCH(FONT_KEY_GOTHIC_14, FONT_KEY_GOTHIC_24)),
                         GRect(0, SIZE_SWITCH(45, bounds.size.h - 38), w, 20),
                         GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    } else {
      const int16_t col_w = SIZE_SWITCH(75, 108);
      graphics_draw_text(ctx, data->nameAway, name_font, GRect(0, 0, col_w, 26),
                         GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);
      graphics_draw_text(ctx, data->nameHome, name_font, GRect(w - col_w, 0, col_w, 26),
                         GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    }
  }
}
#endif

static void menu_draw_row_callback(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index,
                                   void *context) {
  PBL_IF_RECT_ELSE(draw_games_row_rect(ctx, cell_layer, cell_index),
                   draw_games_row_round(ctx, cell_layer, cell_index));
  text_layer_set_text(s_header_text, sports_get_league_title(sports_get_selected_league()));
}

static uint16_t menu_get_num_sections_callback(struct MenuLayer *menu_layer, void *context) {
  return 1;
}

static uint16_t menu_get_num_rows_callback(struct MenuLayer *menu_layer, uint16_t section_index,
                                           void *context) {
  const uint16_t cards = sports_get_num_cards();
  return (cards == 0 ? 1 : cards);
}

static void menu_select_click_callback(struct MenuLayer *menu_layer, MenuIndex *cell_index,
                                       void *context) {
  select_game(*cell_index);
}

static int16_t menu_get_cell_height_callback(struct MenuLayer *menu_layer, MenuIndex *cell_index,
                                             void *context) {
  if (sports_get_num_cards() > 0) {
    return PBL_IF_RECT_ELSE(SIZE_SWITCH(59, 75),
                            menu_layer_cell_is_selected(cell_index) ? SIZE_SWITCH(70, 100)
                                                                    : SIZE_SWITCH(35, 50));
  }
  // Full-bleed "no games" cell. On the large sizes it fills the area below the
  // header; round insets the viewport at top and bottom, so gabbro loses one
  // more header height.
  return SIZE_SWITCH(PBL_IF_COLOR_ELSE(115, 180),
                     PBL_DISPLAY_HEIGHT - STATUS_BAR_LAYER_HEIGHT * PBL_IF_RECT_ELSE(1, 2));
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  window_set_background_color(window, GColorWhite);

  // Top and height both come from STATUS_BAR_LAYER_HEIGHT (16 / 20 emery+gabbro
  // / 24 chalk) so the cell the draw callbacks measure against is exactly the
  // area below the header. The literal 15 this replaced disagreed with
  // menu_get_cell_height_callback, which already used the constant, and left
  // the rect menu a full header-height too tall.
#if PBL_ROUND
  // Symmetric inset on round: with center_focused the MenuLayer centres the
  // focused cell in the viewport, so the viewport itself has to stay centred on
  // the screen or every row sits low. win-leagues insets the same way.
  s_menu_layer = menu_layer_create(GRect(bounds.origin.x,
                                         STATUS_BAR_LAYER_HEIGHT,
                                         bounds.size.w,
                                         bounds.size.h - (STATUS_BAR_LAYER_HEIGHT * 2)));
  menu_layer_set_center_focused(s_menu_layer, true);
#else
  s_menu_layer = menu_layer_create(GRect(bounds.origin.x,
                                         STATUS_BAR_LAYER_HEIGHT,
                                         bounds.size.w,
                                         bounds.size.h - STATUS_BAR_LAYER_HEIGHT));
#endif

  menu_layer_set_normal_colors(s_menu_layer, GColorWhite, GColorBlack);
  menu_layer_set_highlight_colors(s_menu_layer, GColorWhite, GColorBlack);
  menu_layer_set_callbacks(s_menu_layer, NULL, (MenuLayerCallbacks) {
    .get_num_sections = menu_get_num_sections_callback,
    .get_num_rows = menu_get_num_rows_callback,
    .get_cell_height = menu_get_cell_height_callback,
    .draw_row = menu_draw_row_callback,
    .select_click = menu_select_click_callback,
  });
  layer_add_child(window_layer, menu_layer_get_layer(s_menu_layer));

  // One create, not three: the previous version built a TextLayer, configured
  // it, then overwrote the pointer inside the #if — leaking a layer per push
  // and losing the GColorClear background with it. STATUS_TEXT_OFFSET is the
  // same nudge win-card's status bar uses.
  s_header_text = text_layer_create(GRect(bounds.origin.x,
                                          bounds.origin.y + STATUS_TEXT_OFFSET,
                                          bounds.size.w, STATUS_BAR_LAYER_HEIGHT + 4));
  text_layer_set_background_color(s_header_text, GColorClear);
  text_layer_set_font(s_header_text, fonts_get_system_font(STATUS_FONT));
  text_layer_set_text_alignment(s_header_text, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_header_text));
}

static void window_unload(Window *window) {
  text_layer_destroy(s_header_text);
  menu_layer_destroy_safe(s_menu_layer);
}
