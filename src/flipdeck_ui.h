/**
 * @file flipdeck_ui.h
 * @brief FlipDeck User Interface
 */

#ifndef FLIPDECK_UI_H
#define FLIPDECK_UI_H

#include <furi.h>
#include "flipdeck_app.h"

/**
 * @brief Initialize the UI system
 * @param app_ctx Application context
 */
void flipdeck_ui_init(FlipDeckApp* app_ctx);

/**
 * @brief Free UI resources
 */
void flipdeck_ui_free(void);

/**
 * @brief Handle category browser view
 * @param furi Furi object
 */
void flipdeck_ui_handle_category_browser(void);

/**
 * @brief Handle action browser view
 * @param furi Furi object
 */
void flipdeck_ui_handle_action_browser(void);

/**
 * @brief Handle action detail view
 * @param furi Furi object
 */
void flipdeck_ui_handle_action_detail(void);

/**
 * @brief Handle send confirmation dialog
 * @param furi Furi object
 */
void flipdeck_ui_handle_confirm(void);

/**
 * @brief Handle settings screen
 * @param furi Furi object
 */
void flipdeck_ui_handle_settings(void);

/**
 * @brief Handle long snippet warning screen
 */
void flipdeck_ui_handle_long_snippet_warning(void);

#endif // FLIPDECK_UI_H
