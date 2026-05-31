/**
 * @file flipdeck_ui.h
 * @brief FlipDeck User Interface
 */

#ifndef FLIPDECK_UI_H
#define FLIPDECK_UI_H

#include "flipdeck_app.h"
#include <furi.h>

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
void flipdeck_ui_handle_category_browser(furi_t furi);

/**
 * @brief Handle action browser view
 * @param furi Furi object
 */
void flipdeck_ui_handle_action_browser(furi_t furi);

/**
 * @brief Handle action detail view
 * @param furi Furi object
 */
void flipdeck_ui_handle_action_detail(furi_t furi);

/**
 * @brief Handle send confirmation dialog
 * @param furi Furi object
 */
void flipdeck_ui_handle_confirm(furi_t furi);

/**
 * @brief Handle settings screen
 * @param furi Furi object
 */
void flipdeck_ui_handle_settings(furi_t furi);

#endif // FLIPDECK_UI_H