#include <gtk/gtk.h>
#include <cairo.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "config.h"
#include "history.h"
#include "proc_parser.h"
#include "app_metrics.h"

#define APP_TITLE "System Monitor"
#define PROCESS_PREVIEW_ROWS 10

typedef struct {
    GtkWidget *card;
    GtkWidget *drawing_area;
    GtkWidget *value_label;
    GtkWidget *detail_label;
    double value;
    gboolean dark_theme;
    GdkRGBA color;
} MetricCard;

typedef struct {
    GtkWidget *window;
    GtkWidget *status_label;
    GtkWidget *cores_box;
    GtkWidget *history_area;
    GtkWidget *process_count_label;
    GtkWidget *process_window;
    GtkWidget *process_window_count_label;
    GtkWidget *gpu_integrated_button;
    GtkWidget *gpu_nvidia_button;
    GtkWidget *theme_light_button;
    GtkWidget *theme_dark_button;
    GtkListStore *process_store;
    GtkListStore *process_window_store;

    MetricCard cpu_card;
    MetricCard memory_card;
    MetricCard gpu_card;

    CPUStats cpu_prev;
    CPUStats cpu_curr;
    CPUStats cores_prev[MAX_CORES];
    CPUStats cores_curr[MAX_CORES];
    int cores_count;
    gboolean has_cpu_sample;

    MemoryInfo memory;
    GPUInfo gpu;
    GPUSource gpu_source;
    HistoryData history;
    gboolean dark_theme;

    guint refresh_timer_id;
} AppState;

enum {
    COL_PID,
    COL_NAME,
    COL_STATE,
    COL_CPU,
    COL_MEMORY,
    COL_COMMAND,
    N_COLUMNS
};

static GtkCssProvider *theme_provider = NULL;

static gboolean refresh_metrics(gpointer user_data);

static void metric_card_set(MetricCard *card, double value, const char *details) {
    char value_text[32];

    card->value = app_clamp_percent(value);
    snprintf(value_text, sizeof(value_text), "%.0f%%", card->value);

    gtk_label_set_text(GTK_LABEL(card->value_label), value_text);
    gtk_label_set_text(GTK_LABEL(card->detail_label), details);
    gtk_widget_queue_draw(card->drawing_area);
}

static gboolean draw_metric_card(GtkWidget *widget, cairo_t *cr, gpointer user_data) {
    MetricCard *card = user_data;
    GtkAllocation allocation;
    double width;
    double height;
    double radius;
    double line_width;
    double center_x;
    double center_y;
    double start_angle;
    double end_angle;
    double text_color = card->dark_theme ? 0.92 : 0.08;

    gtk_widget_get_allocation(widget, &allocation);
    width = allocation.width;
    height = allocation.height;
    radius = fmin(width, height) * 0.36;
    line_width = fmax(10.0, radius * 0.18);
    center_x = width / 2.0;
    center_y = height / 2.0;
    start_angle = -G_PI / 2.0;
    end_angle = start_angle + (2.0 * G_PI * (card->value / 100.0));

    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_width(cr, line_width);

    if (card->dark_theme) {
        cairo_set_source_rgba(cr, 0.84, 0.87, 0.92, 0.22);
    } else {
        cairo_set_source_rgba(cr, 0.16, 0.18, 0.22, 0.22);
    }
    cairo_arc(cr, center_x, center_y, radius, 0.0, 2.0 * G_PI);
    cairo_stroke(cr);

    cairo_set_source_rgba(cr, card->color.red, card->color.green, card->color.blue, 1.0);
    cairo_arc(cr, center_x, center_y, radius, start_angle, end_angle);
    cairo_stroke(cr);

    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 22.0);
    char text[32];
    cairo_text_extents_t extents;
    snprintf(text, sizeof(text), "%.0f%%", card->value);
    cairo_text_extents(cr, text, &extents);
    cairo_set_source_rgba(cr, text_color, text_color, text_color, 0.96);
    cairo_move_to(cr, center_x - extents.width / 2.0 - extents.x_bearing,
                  center_y - extents.height / 2.0 - extents.y_bearing);
    cairo_show_text(cr, text);

    return FALSE;
}

static gboolean draw_history(GtkWidget *widget, cairo_t *cr, gpointer user_data) {
    AppState *app = user_data;
    GtkAllocation allocation;
    int width;
    int height;
    int count = app->history.count;

    gtk_widget_get_allocation(widget, &allocation);
    width = allocation.width;
    height = allocation.height;

    if (app->dark_theme) {
        cairo_set_source_rgb(cr, 0.08, 0.10, 0.13);
    } else {
        cairo_set_source_rgb(cr, 0.98, 0.98, 0.97);
    }
    cairo_paint(cr);

    cairo_set_line_width(cr, 1.0);
    if (app->dark_theme) {
        cairo_set_source_rgba(cr, 0.92, 0.94, 0.97, 0.14);
    } else {
        cairo_set_source_rgba(cr, 0.12, 0.13, 0.16, 0.12);
    }
    for (int i = 1; i < 5; i++) {
        double y = (height - 24) * i / 5.0 + 12.0;
        cairo_move_to(cr, 16.0, y);
        cairo_line_to(cr, width - 16.0, y);
        cairo_stroke(cr);
    }

    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 11.0);
    if (app->dark_theme) {
        cairo_set_source_rgba(cr, 0.84, 0.88, 0.94, 0.72);
    } else {
        cairo_set_source_rgba(cr, 0.24, 0.27, 0.33, 0.72);
    }
    cairo_move_to(cr, 18.0, 18.0);
    cairo_show_text(cr, "100%");
    cairo_move_to(cr, 18.0, height - 20.0);
    cairo_show_text(cr, "0%");

    if (count < 1) {
        return FALSE;
    }

    const double left = 18.0;
    const double right = width - 18.0;
    const double top = 14.0;
    const double bottom = height - 18.0;
    const double chart_width = right - left;
    const double chart_height = bottom - top;

    const double *series[] = {
        app->history.cpu_usage,
        app->history.memory_usage,
        app->history.gpu_usage
    };
    const double colors[][3] = {
        {0.18, 0.47, 0.95},
        {0.08, 0.62, 0.41},
        {0.82, 0.34, 0.20}
    };

    cairo_set_line_width(cr, 2.0);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);

    for (int s = 0; s < 3; s++) {
        cairo_set_source_rgb(cr, colors[s][0], colors[s][1], colors[s][2]);
        for (int i = 0; i < count; i++) {
            int idx = (app->history.index - count + i + HISTORY_SIZE) % HISTORY_SIZE;
            double x = count == 1 ? left : left + chart_width * i / (count - 1);
            double value = app_clamp_percent(series[s][idx]);
            double y = bottom - chart_height * (value / 100.0);

            if (count == 1) {
                cairo_arc(cr, x, y, 3.5, 0.0, 2.0 * G_PI);
                cairo_fill(cr);
            } else if (i == 0) {
                cairo_move_to(cr, x, y);
            } else {
                cairo_line_to(cr, x, y);
            }
        }
        if (count > 1) {
            cairo_stroke(cr);
        }
    }

    return FALSE;
}

static GtkWidget *create_label(const char *text, const char *class_name) {
    GtkWidget *label = gtk_label_new(text);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0);
    if (class_name) {
        GtkStyleContext *context = gtk_widget_get_style_context(label);
        gtk_style_context_add_class(context, class_name);
    }
    return label;
}

static const char *gpu_source_label(GPUSource source) {
    return source == GPU_SOURCE_INTEGRATED ? "Integrated" : "NVIDIA";
}

static void set_widget_class(GtkWidget *widget, const char *class_name, gboolean enabled) {
    GtkStyleContext *context = gtk_widget_get_style_context(widget);

    if (enabled) {
        gtk_style_context_add_class(context, class_name);
    } else {
        gtk_style_context_remove_class(context, class_name);
    }
}

static MetricCard create_metric_card(const char *title,
                                     const char *subtitle,
                                     const char *color_hex) {
    MetricCard card;
    GtkWidget *title_label;
    GtkWidget *header;
    GtkWidget *content;
    GtkStyleContext *context;

    memset(&card, 0, sizeof(card));
    gdk_rgba_parse(&card.color, color_hex);

    card.card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_hexpand(card.card, TRUE);
    context = gtk_widget_get_style_context(card.card);
    gtk_style_context_add_class(context, "card");

    header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    title_label = create_label(title, "card-title");
    card.value_label = create_label("0%", "metric-value");
    gtk_label_set_xalign(GTK_LABEL(card.value_label), 1.0);
    gtk_box_pack_start(GTK_BOX(header), title_label, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(header), card.value_label, FALSE, FALSE, 0);

    content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    card.drawing_area = gtk_drawing_area_new();
    gtk_widget_set_size_request(card.drawing_area, 150, 150);
    card.detail_label = create_label(subtitle, "muted");
    gtk_label_set_line_wrap(GTK_LABEL(card.detail_label), TRUE);

    gtk_box_pack_start(GTK_BOX(content), card.drawing_area, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(content), card.detail_label, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(card.card), header, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(card.card), content, FALSE, FALSE, 0);

    return card;
}

static void add_text_column(GtkTreeView *tree, const char *title, int column_id, int width) {
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes(
        title, renderer, "text", column_id, NULL);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_column_set_min_width(column, width);
    gtk_tree_view_append_column(tree, column);
}

static GtkListStore *create_process_store(void) {
    return gtk_list_store_new(N_COLUMNS,
                              G_TYPE_INT,
                              G_TYPE_STRING,
                              G_TYPE_STRING,
                              G_TYPE_STRING,
                              G_TYPE_STRING,
                              G_TYPE_STRING);
}

static GtkWidget *create_process_table(GtkListStore *store) {
    GtkWidget *view;
    GtkWidget *scrolled;

    view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(view), TRUE);
    gtk_tree_view_set_enable_search(GTK_TREE_VIEW(view), TRUE);

    add_text_column(GTK_TREE_VIEW(view), "PID", COL_PID, 70);
    add_text_column(GTK_TREE_VIEW(view), "Name", COL_NAME, 150);
    add_text_column(GTK_TREE_VIEW(view), "State", COL_STATE, 60);
    add_text_column(GTK_TREE_VIEW(view), "CPU", COL_CPU, 80);
    add_text_column(GTK_TREE_VIEW(view), "Memory", COL_MEMORY, 100);
    add_text_column(GTK_TREE_VIEW(view), "Command", COL_COMMAND, 260);

    scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(scrolled, TRUE);
    gtk_widget_set_hexpand(scrolled, TRUE);
    gtk_container_add(GTK_CONTAINER(scrolled), view);

    return scrolled;
}

static void refresh_cores(AppState *app) {
    GList *children = gtk_container_get_children(GTK_CONTAINER(app->cores_box));
    for (GList *iter = children; iter; iter = iter->next) {
        gtk_widget_destroy(GTK_WIDGET(iter->data));
    }
    g_list_free(children);

    for (int i = 0; i < app->cores_count && i < MAX_CORES; i++) {
        GtkWidget *tile = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
        GtkWidget *header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        GtkWidget *name_label;
        GtkWidget *percent_label;
        GtkWidget *bar;
        GtkStyleContext *context;
        char name_text[32];
        char percent_text[32];
        char tooltip[96];
        double usage = app_clamp_percent(app->cores_curr[i].usage_percent);

        context = gtk_widget_get_style_context(tile);
        gtk_style_context_add_class(context, "core-tile");
        gtk_widget_set_size_request(tile, 150, 72);

        snprintf(name_text, sizeof(name_text), "Core %d", i);
        snprintf(percent_text, sizeof(percent_text), "%.0f%%", usage);
        snprintf(tooltip, sizeof(tooltip), "Linux CPU core %d (/proc/stat cpu%d)", i, i);

        name_label = create_label(name_text, "core-label");
        percent_label = create_label(percent_text, "core-percent");
        gtk_label_set_xalign(GTK_LABEL(percent_label), 1.0);
        gtk_widget_set_hexpand(name_label, TRUE);

        bar = gtk_progress_bar_new();
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(bar), usage / 100.0);
        gtk_widget_set_hexpand(bar, TRUE);
        gtk_widget_set_tooltip_text(tile, tooltip);

        gtk_box_pack_start(GTK_BOX(header), name_label, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(header), percent_label, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(tile), header, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(tile), bar, FALSE, FALSE, 0);
        gtk_flow_box_insert(GTK_FLOW_BOX(app->cores_box), tile, -1);
    }

    gtk_widget_show_all(app->cores_box);
}

static void append_process_rows(GtkListStore *store,
                                ProcessInfo *processes,
                                int process_count,
                                int max_rows) {
    GtkTreeIter iter;
    char cpu_text[32];
    char memory_text[32];
    char state_text[8];
    int rows = app_visible_process_rows(process_count, max_rows);

    gtk_list_store_clear(store);

    for (int i = 0; i < rows; i++) {
        snprintf(cpu_text, sizeof(cpu_text), "%.1f%%", processes[i].cpu_usage);
        app_format_bytes(memory_text, sizeof(memory_text), (unsigned long long)processes[i].rss * 1024ULL);
        snprintf(state_text, sizeof(state_text), "%c", processes[i].state);

        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
                           COL_PID, processes[i].pid,
                           COL_NAME, processes[i].name,
                           COL_STATE, state_text,
                           COL_CPU, cpu_text,
                           COL_MEMORY, memory_text,
                           COL_COMMAND, processes[i].command_line,
                           -1);
    }
}

static void refresh_processes(AppState *app) {
    ProcessInfo processes[MAX_PROCESSES];
    int process_count = 0;

    if (get_processes(processes, &process_count) != 0) {
        return;
    }

    append_process_rows(app->process_store, processes, process_count, PROCESS_PREVIEW_ROWS);
    append_process_rows(app->process_window_store, processes, process_count, 0);

    char count_text[96];
    int preview_rows = app_visible_process_rows(process_count, PROCESS_PREVIEW_ROWS);
    snprintf(count_text, sizeof(count_text), "%d processes, showing top %d", process_count, preview_rows);
    gtk_label_set_text(GTK_LABEL(app->process_count_label), count_text);

    snprintf(count_text, sizeof(count_text), "%d processes", process_count);
    gtk_label_set_text(GTK_LABEL(app->process_window_count_label), count_text);
}

static void update_gpu_source_buttons(AppState *app) {
    set_widget_class(app->gpu_integrated_button, "gpu-option-active",
                     app->gpu_source == GPU_SOURCE_INTEGRATED);
    set_widget_class(app->gpu_nvidia_button, "gpu-option-active",
                     app->gpu_source == GPU_SOURCE_NVIDIA);
}

static void update_theme_buttons(AppState *app) {
    set_widget_class(app->theme_light_button, "theme-option-active", !app->dark_theme);
    set_widget_class(app->theme_dark_button, "theme-option-active", app->dark_theme);
}

static void apply_theme_css(gboolean dark_theme) {
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

static void set_app_theme(AppState *app, gboolean dark_theme) {
    app->dark_theme = dark_theme;
    app->cpu_card.dark_theme = dark_theme;
    app->memory_card.dark_theme = dark_theme;
    app->gpu_card.dark_theme = dark_theme;
    apply_theme_css(dark_theme);
    update_theme_buttons(app);
    gtk_widget_queue_draw(app->cpu_card.drawing_area);
    gtk_widget_queue_draw(app->memory_card.drawing_area);
    gtk_widget_queue_draw(app->gpu_card.drawing_area);
    gtk_widget_queue_draw(app->history_area);
}

static void on_theme_clicked(GtkButton *button, gpointer user_data) {
    AppState *app = user_data;
    gboolean next_dark = GTK_WIDGET(button) == app->theme_dark_button;

    if (app->dark_theme == next_dark) {
        return;
    }

    set_app_theme(app, next_dark);
}

static void on_gpu_source_clicked(GtkButton *button, gpointer user_data) {
    AppState *app = user_data;
    GPUSource next_source = GTK_WIDGET(button) == app->gpu_integrated_button
        ? GPU_SOURCE_INTEGRATED
        : GPU_SOURCE_NVIDIA;

    if (app->gpu_source == next_source) {
        return;
    }

    app->gpu_source = next_source;
    update_gpu_source_buttons(app);
    init_history(&app->history);
    refresh_metrics(app);
}

static gboolean refresh_metrics(gpointer user_data) {
    AppState *app = user_data;
    char details[256];
    char mem_used[32];
    char mem_total[32];
    char gpu_used[32];
    char gpu_total[32];
    double gpu_memory_percent = 0.0;

    if (read_cpu_stats(&app->cpu_curr, app->cores_curr, &app->cores_count) == 0) {
        if (app->has_cpu_sample) {
            app_calculate_cpu_usage(&app->cpu_prev, &app->cpu_curr);
            for (int i = 0; i < app->cores_count && i < MAX_CORES; i++) {
                app_calculate_cpu_usage(&app->cores_prev[i], &app->cores_curr[i]);
            }
        } else {
            app->has_cpu_sample = TRUE;
        }

        snprintf(details, sizeof(details),
                 "%d cores\n%.1f C\n%lu MHz",
                 app->cores_count,
                 app->cpu_curr.temperature,
                 app->cpu_curr.frequency);
        metric_card_set(&app->cpu_card, app->cpu_curr.usage_percent, details);
        refresh_cores(app);

        memcpy(&app->cpu_prev, &app->cpu_curr, sizeof(CPUStats));
        memcpy(app->cores_prev, app->cores_curr, sizeof(app->cores_prev));
    }

    if (read_memory_info(&app->memory) == 0) {
        app_format_bytes(mem_used, sizeof(mem_used), app->memory.used);
        app_format_bytes(mem_total, sizeof(mem_total), app->memory.total);
        snprintf(details, sizeof(details),
                 "%s used\n%s total\n%.1f%% occupied",
                 mem_used,
                 mem_total,
                 app->memory.percentage);
        metric_card_set(&app->memory_card, app->memory.percentage, details);
    }

    if (read_gpu_info_for_source(&app->gpu, app->gpu_source) == 0) {
        if (app->gpu.memory_total > 0) {
            gpu_memory_percent = (double)app->gpu.memory_used / (double)app->gpu.memory_total * 100.0;
        }
        app_format_bytes(gpu_used, sizeof(gpu_used), app->gpu.memory_used);
        app_format_bytes(gpu_total, sizeof(gpu_total), app->gpu.memory_total);
        if (app->gpu.memory_total > 0) {
            snprintf(details, sizeof(details),
                     "%s\n%s / %s VRAM\n%.1f C, %.0f W\nSource: %s",
                     app->gpu.name,
                     gpu_used,
                     gpu_total,
                     app->gpu.temperature,
                     app->gpu.power,
                     gpu_source_label(app->gpu_source));
        } else {
            if (app->gpu.clock > 0) {
                snprintf(details, sizeof(details),
                         "%s\nShared system memory\nClock: %lu MHz\nSource: %s",
                         app->gpu.name,
                         app->gpu.clock,
                         gpu_source_label(app->gpu_source));
            } else {
                snprintf(details, sizeof(details),
                         "%s\nShared system memory\nDetailed metrics unavailable\nSource: %s",
                         app->gpu.name,
                         gpu_source_label(app->gpu_source));
            }
        }
        metric_card_set(&app->gpu_card, app->gpu.usage, details);
    }

    add_to_history(&app->history,
                   app->cpu_curr.usage_percent,
                   app->memory.percentage,
                   app->gpu.usage,
                   gpu_memory_percent,
                   app->gpu.temperature);

    refresh_processes(app);
    gtk_widget_queue_draw(app->history_area);

    time_t now = time(NULL);
    struct tm local_time;
    char status[96];
    localtime_r(&now, &local_time);
    strftime(status, sizeof(status), "Live - updated %H:%M:%S", &local_time);
    gtk_label_set_text(GTK_LABEL(app->status_label), status);

    return G_SOURCE_CONTINUE;
}

static GtkWidget *create_gpu_selector(AppState *app) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *label = create_label("Source", "muted");
    GtkStyleContext *context;

    context = gtk_widget_get_style_context(box);
    gtk_style_context_add_class(context, "gpu-selector");

    app->gpu_integrated_button = gtk_button_new_with_label("Integrated");
    app->gpu_nvidia_button = gtk_button_new_with_label("NVIDIA");
    set_widget_class(app->gpu_integrated_button, "gpu-option", TRUE);
    set_widget_class(app->gpu_nvidia_button, "gpu-option", TRUE);
    gtk_widget_set_tooltip_text(app->gpu_integrated_button, "Show the integrated GPU");
    gtk_widget_set_tooltip_text(app->gpu_nvidia_button, "Show the NVIDIA GPU");

    g_signal_connect(app->gpu_integrated_button, "clicked", G_CALLBACK(on_gpu_source_clicked), app);
    g_signal_connect(app->gpu_nvidia_button, "clicked", G_CALLBACK(on_gpu_source_clicked), app);
    update_gpu_source_buttons(app);

    gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), app->gpu_integrated_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), app->gpu_nvidia_button, FALSE, FALSE, 0);

    return box;
}

static GtkWidget *create_theme_selector(AppState *app) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *label = create_label("Theme", "muted");
    GtkStyleContext *context;

    context = gtk_widget_get_style_context(box);
    gtk_style_context_add_class(context, "theme-selector");

    app->theme_light_button = gtk_button_new_with_label("Light");
    app->theme_dark_button = gtk_button_new_with_label("Dark");
    set_widget_class(app->theme_light_button, "theme-option", TRUE);
    set_widget_class(app->theme_dark_button, "theme-option", TRUE);
    gtk_widget_set_tooltip_text(app->theme_light_button, "Use light theme");
    gtk_widget_set_tooltip_text(app->theme_dark_button, "Use dark theme");

    g_signal_connect(app->theme_light_button, "clicked", G_CALLBACK(on_theme_clicked), app);
    g_signal_connect(app->theme_dark_button, "clicked", G_CALLBACK(on_theme_clicked), app);
    update_theme_buttons(app);

    gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), app->theme_light_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), app->theme_dark_button, FALSE, FALSE, 0);

    return box;
}

static GtkWidget *create_history_panel(AppState *app) {
    GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GtkWidget *title = create_label("History", "section-title");
    GtkWidget *legend = create_label("CPU blue   RAM green   GPU orange", "muted");
    GtkStyleContext *context = gtk_widget_get_style_context(card);

    gtk_style_context_add_class(context, "card");
    gtk_widget_set_hexpand(card, TRUE);

    app->history_area = gtk_drawing_area_new();
    gtk_widget_set_size_request(app->history_area, -1, 240);
    g_signal_connect(app->history_area, "draw", G_CALLBACK(draw_history), app);

    gtk_box_pack_start(GTK_BOX(header), title, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(header), legend, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(card), header, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(card), app->history_area, TRUE, TRUE, 0);

    return card;
}

static gboolean hide_process_window(GtkWidget *widget, GdkEvent *event, gpointer user_data) {
    (void)event;
    (void)user_data;

    gtk_widget_hide(widget);
    return TRUE;
}

static void show_process_window(GtkButton *button, gpointer user_data) {
    AppState *app = user_data;
    (void)button;

    gtk_widget_show_all(app->process_window);
    gtk_window_present(GTK_WINDOW(app->process_window));
}

static void create_process_window(AppState *app) {
    GtkWidget *page;
    GtkWidget *header;
    GtkWidget *title;
    GtkWidget *table;
    GtkStyleContext *context;

    app->process_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(app->process_window), "System Monitor - Processes");
    gtk_window_set_default_size(GTK_WINDOW(app->process_window), 980, 680);
    gtk_window_set_transient_for(GTK_WINDOW(app->process_window), GTK_WINDOW(app->window));
    g_signal_connect(app->process_window, "delete-event", G_CALLBACK(hide_process_window), NULL);

    page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    context = gtk_widget_get_style_context(page);
    gtk_style_context_add_class(context, "page");
    gtk_container_add(GTK_CONTAINER(app->process_window), page);

    header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    title = create_label("Processes", "title");
    app->process_window_count_label = create_label("No process data yet", "status");
    gtk_label_set_xalign(GTK_LABEL(app->process_window_count_label), 1.0);

    gtk_box_pack_start(GTK_BOX(header), title, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(header), app->process_window_count_label, FALSE, FALSE, 0);

    table = create_process_table(app->process_window_store);

    gtk_box_pack_start(GTK_BOX(page), header, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(page), table, TRUE, TRUE, 0);
}

static GtkWidget *create_process_panel(AppState *app) {
    GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GtkWidget *title = create_label("Top Processes", "section-title");
    GtkWidget *button = gtk_button_new_with_label("Open process window");
    GtkWidget *table = create_process_table(app->process_store);
    GtkStyleContext *context = gtk_widget_get_style_context(card);

    gtk_style_context_add_class(context, "card");
    gtk_widget_set_vexpand(card, TRUE);
    gtk_widget_set_size_request(card, -1, 360);

    app->process_count_label = create_label("No process data yet", "muted");
    gtk_label_set_xalign(GTK_LABEL(app->process_count_label), 1.0);
    set_widget_class(button, "command-button", TRUE);
    g_signal_connect(button, "clicked", G_CALLBACK(show_process_window), app);

    gtk_box_pack_start(GTK_BOX(header), title, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(header), app->process_count_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(header), button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(card), header, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(card), table, TRUE, TRUE, 0);

    return card;
}

static void build_ui(AppState *app) {
    GtkWidget *scrolled_window;
    GtkWidget *page;
    GtkWidget *header;
    GtkWidget *title;
    GtkWidget *theme_selector;
    GtkWidget *process_button;
    GtkWidget *metrics_grid;
    GtkWidget *main_grid;
    GtkWidget *cores_card;
    GtkWidget *cores_title;
    GtkWidget *history_panel;
    GtkWidget *process_panel;
    GtkStyleContext *context;

    app->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(app->window), APP_TITLE);
    gtk_window_set_default_size(GTK_WINDOW(app->window), 1280, 860);
    gtk_window_set_position(GTK_WINDOW(app->window), GTK_WIN_POS_CENTER);
    g_signal_connect(app->window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(app->window), scrolled_window);

    page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    context = gtk_widget_get_style_context(page);
    gtk_style_context_add_class(context, "page");
    gtk_container_add(GTK_CONTAINER(scrolled_window), page);

    app->process_store = create_process_store();
    app->process_window_store = create_process_store();

    header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    title = create_label("System Monitor", "title");
    app->status_label = create_label("Starting...", "status");
    theme_selector = create_theme_selector(app);
    process_button = gtk_button_new_with_label("Processes");
    set_widget_class(process_button, "command-button", TRUE);
    g_signal_connect(process_button, "clicked", G_CALLBACK(show_process_window), app);
    gtk_label_set_xalign(GTK_LABEL(app->status_label), 1.0);
    gtk_box_pack_start(GTK_BOX(header), title, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(header), theme_selector, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(header), process_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(header), app->status_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(page), header, FALSE, FALSE, 0);

    metrics_grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(metrics_grid), 14);
    gtk_grid_set_row_spacing(GTK_GRID(metrics_grid), 14);
    gtk_widget_set_hexpand(metrics_grid, TRUE);

    app->cpu_card = create_metric_card("CPU", "Waiting for CPU sample", "#2f78df");
    app->memory_card = create_metric_card("Memory", "Waiting for memory sample", "#159a63");
    app->gpu_card = create_metric_card("GPU", "Waiting for GPU sample", "#d46333");
    g_signal_connect(app->cpu_card.drawing_area, "draw", G_CALLBACK(draw_metric_card), &app->cpu_card);
    g_signal_connect(app->memory_card.drawing_area, "draw", G_CALLBACK(draw_metric_card), &app->memory_card);
    g_signal_connect(app->gpu_card.drawing_area, "draw", G_CALLBACK(draw_metric_card), &app->gpu_card);
    gtk_box_pack_start(GTK_BOX(app->gpu_card.card), create_gpu_selector(app), FALSE, FALSE, 0);

    gtk_grid_attach(GTK_GRID(metrics_grid), app->cpu_card.card, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(metrics_grid), app->memory_card.card, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(metrics_grid), app->gpu_card.card, 2, 0, 1, 1);
    gtk_box_pack_start(GTK_BOX(page), metrics_grid, FALSE, FALSE, 0);

    main_grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(main_grid), 14);
    gtk_grid_set_row_spacing(GTK_GRID(main_grid), 14);
    gtk_widget_set_hexpand(main_grid, TRUE);
    gtk_widget_set_vexpand(main_grid, TRUE);

    cores_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    context = gtk_widget_get_style_context(cores_card);
    gtk_style_context_add_class(context, "card");
    gtk_widget_set_hexpand(cores_card, TRUE);
    cores_title = create_label("CPU Cores", "section-title");
    app->cores_box = gtk_flow_box_new();
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(app->cores_box), GTK_SELECTION_NONE);
    gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(app->cores_box), 2);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(app->cores_box), 4);
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(app->cores_box), 8);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(app->cores_box), 8);
    gtk_box_pack_start(GTK_BOX(cores_card), cores_title, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(cores_card), app->cores_box, TRUE, TRUE, 0);

    history_panel = create_history_panel(app);
    process_panel = create_process_panel(app);

    gtk_grid_attach(GTK_GRID(main_grid), cores_card, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(main_grid), history_panel, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(main_grid), process_panel, 0, 1, 2, 1);
    gtk_grid_set_column_homogeneous(GTK_GRID(main_grid), TRUE);
    gtk_box_pack_start(GTK_BOX(page), main_grid, TRUE, TRUE, 0);

    create_process_window(app);
}

int main(int argc, char **argv) {
    AppState app;
    memset(&app, 0, sizeof(app));

    gtk_init(&argc, &argv);
    apply_theme_css(FALSE);
    init_history(&app.history);
    app.gpu_source = GPU_SOURCE_NVIDIA;
    app.dark_theme = FALSE;
    build_ui(&app);
    set_app_theme(&app, FALSE);

    gtk_widget_show_all(app.window);
    gtk_window_maximize(GTK_WINDOW(app.window));
    refresh_metrics(&app);
    app.refresh_timer_id = g_timeout_add(UPDATE_INTERVAL_MS, refresh_metrics, &app);

    gtk_main();

    if (app.refresh_timer_id != 0) {
        g_source_remove(app.refresh_timer_id);
    }

    return 0;
}
