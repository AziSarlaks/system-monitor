#include "theme.h"

static GtkCssProvider *theme_provider = NULL;

void apply_theme_css(gboolean dark_theme) {
    const char *light_css =
        "window { background: #f3f4f6; }"
        ".page { background: #f3f4f6; padding: 18px; }"
        ".title { font-size: 26px; font-weight: 700; color: #151820; }"
        ".status { color: #256b4f; font-weight: 600; }"
        ".card { background: #ffffff; border: 1px solid #d8dde6; border-radius: 8px; padding: 14px; }"
        ".card-title { font-size: 17px; font-weight: 700; color: #1b1f2a; }"
        ".section-title { font-size: 16px; font-weight: 700; color: #1b1f2a; }"
        ".metric-value { font-size: 20px; font-weight: 700; color: #1b1f2a; }"
        ".muted { color: #586174; font-size: 13px; }"
        ".core-tile { background: #f8fafc; border: 1px solid #d8dde6; border-radius: 6px; padding: 8px; }"
        ".core-label { color: #334155; font-size: 12px; font-weight: 700; }"
        ".core-percent { color: #1b1f2a; font-size: 12px; font-weight: 700; }"
        ".gpu-selector, .theme-selector { margin-top: 4px; }"
        ".gpu-option, .theme-option, .command-button { background: #f8fafc; color: #1b1f2a; border: 1px solid #cbd5e1; border-radius: 6px; padding: 5px 10px; }"
        ".gpu-option label, .theme-option label, .command-button label { color: #1b1f2a; }"
        ".gpu-option:hover, .theme-option:hover, .command-button:hover { background: #eef2f7; }"
        ".gpu-option-active, .theme-option-active { background: #2f78df; color: #ffffff; border-color: #2f78df; }"
        ".gpu-option-active label, .theme-option-active label { color: #ffffff; }"
        "progressbar trough { min-height: 12px; border-radius: 6px; background: #e5e8ee; }"
        "progressbar progress { min-height: 12px; border-radius: 6px; background: #2f78df; }"
        "treeview { background: #ffffff; color: #1b1f2a; }"
        "treeview header button { background: #eef1f5; color: #1b1f2a; font-weight: 700; }";

    const char *dark_css =
        "window { background: #0f141b; }"
        ".page { background: #0f141b; padding: 18px; }"
        ".title { font-size: 26px; font-weight: 700; color: #f4f7fb; }"
        ".status { color: #7dd3a8; font-weight: 600; }"
        ".card { background: #171d26; border: 1px solid #303948; border-radius: 8px; padding: 14px; }"
        ".card-title { font-size: 17px; font-weight: 700; color: #f4f7fb; }"
        ".section-title { font-size: 16px; font-weight: 700; color: #f4f7fb; }"
        ".metric-value { font-size: 20px; font-weight: 700; color: #f4f7fb; }"
        ".muted { color: #aab4c3; font-size: 13px; }"
        ".core-tile { background: #111821; border: 1px solid #303948; border-radius: 6px; padding: 8px; }"
        ".core-label { color: #dbe3ee; font-size: 12px; font-weight: 700; }"
        ".core-percent { color: #f4f7fb; font-size: 12px; font-weight: 700; }"
        ".gpu-selector, .theme-selector { margin-top: 4px; }"
        ".gpu-option, .theme-option, .command-button { background: #111821; color: #f4f7fb; border: 1px solid #405064; border-radius: 6px; padding: 5px 10px; }"
        ".gpu-option label, .theme-option label, .command-button label { color: #f4f7fb; }"
        ".gpu-option:hover, .theme-option:hover, .command-button:hover { background: #1d2734; }"
        ".gpu-option-active, .theme-option-active { background: #2f78df; color: #ffffff; border-color: #5d9cff; }"
        ".gpu-option-active label, .theme-option-active label { color: #ffffff; }"
        "progressbar trough { min-height: 12px; border-radius: 6px; background: #293342; }"
        "progressbar progress { min-height: 12px; border-radius: 6px; background: #5d9cff; }"
        "treeview { background: #151c25; color: #f4f7fb; }"
        "treeview header button { background: #202a37; color: #f4f7fb; font-weight: 700; }";

    if (!theme_provider) {
        theme_provider = gtk_css_provider_new();
        gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
                                                  GTK_STYLE_PROVIDER(theme_provider),
                                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }

    gtk_css_provider_load_from_data(theme_provider, dark_theme ? dark_css : light_css, -1, NULL);
}
