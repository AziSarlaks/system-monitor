#include <gtk/gtk.h>
#include <cairo.h>
#include <ctype.h>
#include <errno.h>
#include <glib/gstdio.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#include "config.h"
#include "history.h"
#include "proc_parser.h"
#include "app_metrics.h"
#include "app_state.h"
#include "metrics_sampler.h"
#include "notifications.h"
#include "theme.h"

static gboolean refresh_metrics(gpointer user_data);
static void render_process_stores(AppState *app);
static void save_settings(AppState *app);

static char *settings_path(void) {
    char *config_dir = g_build_filename(g_get_user_config_dir(), "system-monitor", NULL);
    char *path;

    g_mkdir_with_parents(config_dir, 0700);
    path = g_build_filename(config_dir, "config.ini", NULL);
    g_free(config_dir);
    return path;
}

static void load_settings(AppState *app) {
    GKeyFile *key_file;
    char *path;
    GError *error = NULL;

    app->dark_theme = FALSE;
    app->gpu_source = GPU_SOURCE_NVIDIA;
    app->refresh_interval_ms = UPDATE_INTERVAL_MS;
    app->window_width = 1280;
    app->window_height = 860;

    key_file = g_key_file_new();
    path = settings_path();
    if (g_key_file_load_from_file(key_file, path, G_KEY_FILE_NONE, &error)) {
        if (g_key_file_has_key(key_file, "General", "dark_theme", NULL)) {
            app->dark_theme = g_key_file_get_boolean(key_file, "General", "dark_theme", NULL);
        }
        if (g_key_file_has_key(key_file, "General", "gpu_source", NULL)) {
            int gpu_source = g_key_file_get_integer(key_file, "General", "gpu_source", NULL);
            app->gpu_source = gpu_source == GPU_SOURCE_INTEGRATED
                ? GPU_SOURCE_INTEGRATED
                : GPU_SOURCE_NVIDIA;
        }
        if (g_key_file_has_key(key_file, "General", "refresh_interval_ms", NULL)) {
            int interval = g_key_file_get_integer(key_file, "General", "refresh_interval_ms", NULL);
            if (interval == 1000 || interval == 2000 || interval == 5000) {
                app->refresh_interval_ms = interval;
            }
        }
        if (g_key_file_has_key(key_file, "Window", "width", NULL)) {
            int width = g_key_file_get_integer(key_file, "Window", "width", NULL);
            if (width >= 900) {
                app->window_width = width;
            }
        }
        if (g_key_file_has_key(key_file, "Window", "height", NULL)) {
            int height = g_key_file_get_integer(key_file, "Window", "height", NULL);
            if (height >= 600) {
                app->window_height = height;
            }
        }
    }
    g_clear_error(&error);
    g_free(path);
    g_key_file_unref(key_file);
}

static void save_settings(AppState *app) {
    GKeyFile *key_file;
    char *path;
    char *data;
    gsize size;

    if (!app) {
        return;
    }

    if (app->window) {
        gtk_window_get_size(GTK_WINDOW(app->window), &app->window_width, &app->window_height);
    }

    key_file = g_key_file_new();
    g_key_file_set_boolean(key_file, "General", "dark_theme", app->dark_theme);
    g_key_file_set_integer(key_file, "General", "gpu_source", app->gpu_source);
    g_key_file_set_integer(key_file, "General", "refresh_interval_ms", app->refresh_interval_ms);
    g_key_file_set_integer(key_file, "Window", "width", app->window_width);
    g_key_file_set_integer(key_file, "Window", "height", app->window_height);

    data = g_key_file_to_data(key_file, &size, NULL);
    path = settings_path();
    if (data) {
        g_file_set_contents(path, data, size, NULL);
    }

    g_free(data);
    g_free(path);
    g_key_file_unref(key_file);
}

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

static gboolean draw_io_history(GtkWidget *widget, cairo_t *cr, gpointer user_data) {
    AppState *app = user_data;
    GtkAllocation allocation;
    int count = app->history.count;
    int width;
    int height;
    double max_value = 1024.0;

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

    if (count < 1) {
        return FALSE;
    }

    for (int i = 0; i < count; i++) {
        int idx = (app->history.index - count + i + HISTORY_SIZE) % HISTORY_SIZE;
        double values[] = {
            app->history.disk_read[idx],
            app->history.disk_write[idx],
            app->history.network_rx[idx],
            app->history.network_tx[idx]
        };

        for (int v = 0; v < 4; v++) {
            if (values[v] > max_value) {
                max_value = values[v];
            }
        }
    }

    const double left = 18.0;
    const double right = width - 18.0;
    const double top = 14.0;
    const double bottom = height - 18.0;
    const double chart_width = right - left;
    const double chart_height = bottom - top;
    const double *series[] = {
        app->history.disk_read,
        app->history.disk_write,
        app->history.network_rx,
        app->history.network_tx
    };
    const double colors[][3] = {
        {0.48, 0.37, 0.95},
        {0.95, 0.58, 0.22},
        {0.10, 0.70, 0.86},
        {0.93, 0.32, 0.45}
    };

    cairo_set_line_width(cr, 2.0);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);

    for (int s = 0; s < 4; s++) {
        cairo_set_source_rgb(cr, colors[s][0], colors[s][1], colors[s][2]);
        for (int i = 0; i < count; i++) {
            int idx = (app->history.index - count + i + HISTORY_SIZE) % HISTORY_SIZE;
            double x = count == 1 ? left : left + chart_width * i / (count - 1);
            double y = bottom - chart_height * (series[s][idx] / max_value);

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

static GtkWidget *create_info_card(const char *title, GtkWidget **detail_label) {
    GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *title_label = create_label(title, "card-title");
    GtkStyleContext *context = gtk_widget_get_style_context(card);

    gtk_style_context_add_class(context, "card");
    gtk_widget_set_hexpand(card, TRUE);

    *detail_label = create_label("Waiting for data", "muted");
    gtk_label_set_line_wrap(GTK_LABEL(*detail_label), TRUE);

    gtk_box_pack_start(GTK_BOX(card), title_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(card), *detail_label, FALSE, FALSE, 0);

    return card;
}

static void format_rate(char *buffer, size_t size, double bytes_per_sec) {
    const double gib = 1024.0 * 1024.0 * 1024.0;
    const double mib = 1024.0 * 1024.0;
    const double kib = 1024.0;

    if (bytes_per_sec >= gib) {
        snprintf(buffer, size, "%.1f GB/s", bytes_per_sec / gib);
    } else if (bytes_per_sec >= mib) {
        snprintf(buffer, size, "%.1f MB/s", bytes_per_sec / mib);
    } else if (bytes_per_sec >= kib) {
        snprintf(buffer, size, "%.1f KB/s", bytes_per_sec / kib);
    } else {
        snprintf(buffer, size, "%.0f B/s", bytes_per_sec);
    }
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

static GtkWidget *create_process_table(GtkListStore *store, GtkWidget **view_out) {
    GtkWidget *view;
    GtkWidget *scrolled;

    view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
    if (view_out) {
        *view_out = view;
    }
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

static int process_matches_filter(const ProcessInfo *process, const char *filter) {
    char haystack[900];
    char needle[128];

    if (!filter || filter[0] == '\0') {
        return 1;
    }

    snprintf(haystack, sizeof(haystack), "%d %s %s",
             process->pid,
             process->name,
             process->command_line);
    snprintf(needle, sizeof(needle), "%s", filter);

    for (char *p = haystack; *p; p++) *p = tolower((unsigned char)*p);
    for (char *p = needle; *p; p++) *p = tolower((unsigned char)*p);

    return strstr(haystack, needle) != NULL;
}

static int compare_processes(const void *left, const void *right, void *user_data) {
    const ProcessInfo *a = left;
    const ProcessInfo *b = right;
    int sort_mode = GPOINTER_TO_INT(user_data);

    if (sort_mode == 1) {
        if (a->mem_usage < b->mem_usage) return 1;
        if (a->mem_usage > b->mem_usage) return -1;
        return 0;
    }

    if (sort_mode == 2) {
        return strcasecmp(a->name, b->name);
    }

    if (a->cpu_usage < b->cpu_usage) return 1;
    if (a->cpu_usage > b->cpu_usage) return -1;
    return 0;
}

static void render_process_stores(AppState *app) {
    ProcessInfo filtered[MAX_PROCESSES];
    int filtered_count = 0;

    for (int i = 0; i < app->last_process_count && i < MAX_PROCESSES; i++) {
        if (process_matches_filter(&app->last_processes[i], app->process_filter)) {
            filtered[filtered_count++] = app->last_processes[i];
        }
    }

    g_qsort_with_data(filtered,
                      filtered_count,
                      sizeof(ProcessInfo),
                      compare_processes,
                      GINT_TO_POINTER(app->process_sort_mode));

    append_process_rows(app->process_store, filtered, filtered_count, PROCESS_PREVIEW_ROWS);
    append_process_rows(app->process_window_store, filtered, filtered_count, 0);

    char count_text[96];
    int preview_rows = app_visible_process_rows(filtered_count, PROCESS_PREVIEW_ROWS);
    snprintf(count_text, sizeof(count_text), "%d processes, showing top %d", filtered_count, preview_rows);
    gtk_label_set_text(GTK_LABEL(app->process_count_label), count_text);

    snprintf(count_text, sizeof(count_text), "%d processes", filtered_count);
    gtk_label_set_text(GTK_LABEL(app->process_window_count_label), count_text);
}

static void refresh_processes(AppState *app) {
    int process_count = 0;

    if (get_processes(app->last_processes, &process_count) != 0) {
        return;
    }

    app->last_process_count = process_count;
    render_process_stores(app);
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

static void update_refresh_combo(AppState *app) {
    int active = 1;

    if (!app->refresh_combo) {
        return;
    }

    if (app->refresh_interval_ms == 1000) {
        active = 0;
    } else if (app->refresh_interval_ms == 5000) {
        active = 2;
    }

    gtk_combo_box_set_active(GTK_COMBO_BOX(app->refresh_combo), active);
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
    gtk_widget_queue_draw(app->io_history_area);
}

static void restart_refresh_timer(AppState *app) {
    if (app->refresh_timer_id != 0) {
        g_source_remove(app->refresh_timer_id);
    }

    app->refresh_timer_id = g_timeout_add(app->refresh_interval_ms, refresh_metrics, app);
}

static void on_theme_clicked(GtkButton *button, gpointer user_data) {
    AppState *app = user_data;
    gboolean next_dark = GTK_WIDGET(button) == app->theme_dark_button;

    if (app->dark_theme == next_dark) {
        return;
    }

    set_app_theme(app, next_dark);
    save_settings(app);
}

static void on_refresh_interval_changed(GtkComboBox *combo, gpointer user_data) {
    AppState *app = user_data;
    int active = gtk_combo_box_get_active(combo);

    if (active == 0) {
        app->refresh_interval_ms = 1000;
    } else if (active == 2) {
        app->refresh_interval_ms = 5000;
    } else {
        app->refresh_interval_ms = 2000;
    }

    restart_refresh_timer(app);
    save_settings(app);
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
    app->metrics_generation++;
    update_gpu_source_buttons(app);
    init_history(&app->history);
    save_settings(app);
    refresh_metrics(app);
}

static void on_process_search_changed(GtkEditable *editable, gpointer user_data) {
    AppState *app = user_data;
    const char *text = gtk_entry_get_text(GTK_ENTRY(editable));

    snprintf(app->process_filter, sizeof(app->process_filter), "%s", text ? text : "");
    render_process_stores(app);
}

static void on_process_sort_changed(GtkComboBox *combo, gpointer user_data) {
    AppState *app = user_data;

    app->process_sort_mode = gtk_combo_box_get_active(combo);
    if (app->process_sort_mode < 0) {
        app->process_sort_mode = 0;
    }
    render_process_stores(app);
}

static void terminate_selected_process(AppState *app, int signal_number, const char *action_label) {
    GtkTreeSelection *selection;
    GtkTreeModel *model;
    GtkTreeIter iter;
    int pid = 0;
    char name[256] = "";
    GtkWidget *dialog;
    int response;

    if (!app->process_window_view) {
        return;
    }

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(app->process_window_view));
    if (!gtk_tree_selection_get_selected(selection, &model, &iter)) {
        return;
    }

    gtk_tree_model_get(model, &iter, COL_PID, &pid, COL_NAME, &name, -1);
    if (pid <= 1) {
        return;
    }

    dialog = gtk_message_dialog_new(GTK_WINDOW(app->process_window),
                                    GTK_DIALOG_MODAL,
                                    GTK_MESSAGE_WARNING,
                                    GTK_BUTTONS_CANCEL,
                                    "%s process?",
                                    action_label);
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
                                             "Send %s to %s (PID %d).",
                                             signal_number == SIGKILL ? "SIGKILL" : "SIGTERM",
                                             name,
                                             pid);
    gtk_dialog_add_button(GTK_DIALOG(dialog), action_label, GTK_RESPONSE_ACCEPT);
    response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    if (response == GTK_RESPONSE_ACCEPT) {
        if (kill(pid, signal_number) == -1) {
            char error_text[512];
            snprintf(error_text,
                     sizeof(error_text),
                     "Could not send %s to %s (PID %d): %s.",
                     signal_number == SIGKILL ? "SIGKILL" : "SIGTERM",
                     name,
                     pid,
                     strerror(errno));
            show_error_dialog(GTK_WINDOW(app->process_window), "Process signal failed", error_text);
        } else {
            refresh_processes(app);
        }
    }
}

static void on_kill_process_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    terminate_selected_process(user_data, SIGTERM, "Terminate");
}

static void on_force_kill_process_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    terminate_selected_process(user_data, SIGKILL, "Kill");
}

static void on_process_refresh_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    refresh_metrics(user_data);
}

static void collect_metrics_task(GTask *task,
                                 gpointer source_object,
                                 gpointer task_data,
                                 GCancellable *cancellable) {
    MetricsSnapshot *snapshot = task_data;
    (void)source_object;
    (void)cancellable;

    collect_metrics_snapshot(snapshot);
    g_task_return_boolean(task, TRUE);
}

static void apply_metrics_snapshot(AppState *app, const MetricsSnapshot *snapshot) {
    char details[256];
    char mem_used[32];
    char mem_total[32];
    char gpu_used[32];
    char gpu_total[32];
    char disk_read_rate[32];
    char disk_write_rate[32];
    char network_rx_rate[32];
    char network_tx_rate[32];
    char sensor_details[256];
    char temp_text[32];
    char freq_text[32];
    double history_cpu = 0.0;
    double history_memory = 0.0;
    double history_gpu = 0.0;
    double history_gpu_memory = 0.0;
    double history_gpu_temp = 0.0;
    double history_disk_read = 0.0;
    double history_disk_write = 0.0;
    double history_network_rx = 0.0;
    double history_network_tx = 0.0;
    double history_battery = 0.0;

    if (snapshot->cpu_ok) {
        app->cpu_curr = snapshot->cpu;
        memcpy(app->cores_curr, snapshot->cores, sizeof(app->cores_curr));
        app->cores_count = snapshot->cores_count;

        if (app->has_cpu_sample) {
            app_calculate_cpu_usage(&app->cpu_prev, &app->cpu_curr);
            for (int i = 0; i < app->cores_count && i < MAX_CORES; i++) {
                app_calculate_cpu_usage(&app->cores_prev[i], &app->cores_curr[i]);
            }
        } else {
            app->has_cpu_sample = TRUE;
        }

        if (app->cpu_curr.temperature > 0.0) {
            snprintf(temp_text, sizeof(temp_text), "%.1f C", app->cpu_curr.temperature);
        } else {
            snprintf(temp_text, sizeof(temp_text), "Temp unavailable");
        }
        if (app->cpu_curr.frequency > 0) {
            snprintf(freq_text, sizeof(freq_text), "%lu MHz", app->cpu_curr.frequency);
        } else {
            snprintf(freq_text, sizeof(freq_text), "Frequency unavailable");
        }
        snprintf(details, sizeof(details),
                 "%d cores\n%s\n%s",
                 app->cores_count,
                 temp_text,
                 freq_text);
        metric_card_set(&app->cpu_card, app->cpu_curr.usage_percent, details);
        refresh_cores(app);

        memcpy(&app->cpu_prev, &app->cpu_curr, sizeof(CPUStats));
        memcpy(app->cores_prev, app->cores_curr, sizeof(app->cores_prev));
        history_cpu = app->cpu_curr.usage_percent;
    } else {
        memset(&app->cpu_curr, 0, sizeof(app->cpu_curr));
        app->cores_count = 0;
        app->has_cpu_sample = FALSE;
        metric_card_set(&app->cpu_card, 0.0, "CPU metrics unavailable");
        refresh_cores(app);
    }

    if (snapshot->memory_ok) {
        app->memory = snapshot->memory;
        app_format_bytes(mem_used, sizeof(mem_used), app->memory.used);
        app_format_bytes(mem_total, sizeof(mem_total), app->memory.total);
        snprintf(details, sizeof(details),
                 "%s used\n%s total\n%.1f%% occupied",
                 mem_used,
                 mem_total,
                 app->memory.percentage);
        metric_card_set(&app->memory_card, app->memory.percentage, details);
        history_memory = app->memory.percentage;
    } else {
        memset(&app->memory, 0, sizeof(app->memory));
        metric_card_set(&app->memory_card, 0.0, "Memory metrics unavailable");
    }

    if (snapshot->gpu_ok) {
        app->gpu = snapshot->gpu;
        app_format_bytes(gpu_used, sizeof(gpu_used), app->gpu.memory_used);
        app_format_bytes(gpu_total, sizeof(gpu_total), app->gpu.memory_total);
        if (app->gpu.memory_total > 0) {
            if (app->gpu.fan_rpm > 0.0) {
                snprintf(details, sizeof(details),
                         "%s\n%s / %s VRAM\n%.1f C, %.0f W\nFan: %.0f RPM\nSource: %s",
                         app->gpu.name,
                         gpu_used,
                         gpu_total,
                         app->gpu.temperature,
                         app->gpu.power,
                         app->gpu.fan_rpm,
                         gpu_source_label(app->gpu_source));
            } else {
                snprintf(details, sizeof(details),
                         "%s\n%s / %s VRAM\n%.1f C, %.0f W\nSource: %s",
                         app->gpu.name,
                         gpu_used,
                         gpu_total,
                         app->gpu.temperature,
                         app->gpu.power,
                         gpu_source_label(app->gpu_source));
            }
        } else {
            if (app->gpu.clock > 0) {
                snprintf(details, sizeof(details),
                         "%s\nShared system memory\nClock: %lu MHz\nUsage: %.0f%%\nSource: %s",
                         app->gpu.name,
                         app->gpu.clock,
                         app->gpu.usage,
                         gpu_source_label(app->gpu_source));
            } else {
                snprintf(details, sizeof(details),
                         "%s\nShared system memory\nDetailed metrics unavailable\nSource: %s",
                         app->gpu.name,
                         gpu_source_label(app->gpu_source));
            }
        }
        metric_card_set(&app->gpu_card, app->gpu.usage, details);
        history_gpu = app->gpu.usage;
        history_gpu_memory = snapshot->gpu_memory_percent;
        history_gpu_temp = app->gpu.temperature;
    } else {
        app->gpu = snapshot->gpu;
        snprintf(details, sizeof(details),
                 "%s\nMetrics unavailable\nSource: %s",
                 app->gpu.name[0] ? app->gpu.name : "GPU unavailable",
                 gpu_source_label(app->gpu_source));
        metric_card_set(&app->gpu_card, 0.0, details);
    }

    if (snapshot->disk_ok) {
        app->disk = snapshot->disk;
        format_rate(disk_read_rate, sizeof(disk_read_rate), app->disk.read_rate);
        format_rate(disk_write_rate, sizeof(disk_write_rate), app->disk.write_rate);
        snprintf(details, sizeof(details),
                 "Read: %s\nWrite: %s",
                 disk_read_rate,
                 disk_write_rate);
        gtk_label_set_text(GTK_LABEL(app->disk_detail_label), details);
        history_disk_read = app->disk.read_rate;
        history_disk_write = app->disk.write_rate;
    } else {
        memset(&app->disk, 0, sizeof(app->disk));
        gtk_label_set_text(GTK_LABEL(app->disk_detail_label), "Disk metrics unavailable");
    }

    if (snapshot->network_ok) {
        app->network = snapshot->network;
        format_rate(network_rx_rate, sizeof(network_rx_rate), app->network.rx_rate);
        format_rate(network_tx_rate, sizeof(network_tx_rate), app->network.tx_rate);
        snprintf(details, sizeof(details),
                 "RX: %s\nTX: %s",
                 network_rx_rate,
                 network_tx_rate);
        gtk_label_set_text(GTK_LABEL(app->network_detail_label), details);
        history_network_rx = app->network.rx_rate;
        history_network_tx = app->network.tx_rate;
    } else {
        memset(&app->network, 0, sizeof(app->network));
        gtk_label_set_text(GTK_LABEL(app->network_detail_label), "Network metrics unavailable");
    }

    if (snapshot->battery_ok) {
        app->battery = snapshot->battery;
        if (app->battery.present) {
            snprintf(details, sizeof(details),
                     "%.0f%%\n%s\nPower: %.1f W\nTemp: %.1f C",
                     app->battery.percentage,
                     app->battery.status,
                     app->battery.power_watts,
                     app->battery.temperature);
        } else {
            snprintf(details, sizeof(details), "No battery detected");
        }
        gtk_label_set_text(GTK_LABEL(app->battery_detail_label), details);
        history_battery = app->battery.present ? app->battery.percentage : 0.0;
    } else {
        memset(&app->battery, 0, sizeof(app->battery));
        gtk_label_set_text(GTK_LABEL(app->battery_detail_label), "Battery metrics unavailable");
    }

    if (snapshot->sensor_ok) {
        app->sensor = snapshot->sensor;
        snprintf(sensor_details,
                 sizeof(sensor_details),
                 "Storage: %s\nFan: %s",
                 app->sensor.storage_temperature_available
                     ? ""
                     : "Unavailable",
                 app->sensor.fan_available
                     ? ""
                     : "Unavailable");
        if (app->sensor.storage_temperature_available && app->sensor.fan_available) {
            snprintf(sensor_details,
                     sizeof(sensor_details),
                     "%s: %.1f C\n%s: %.0f RPM",
                     app->sensor.storage_name,
                     app->sensor.storage_temperature,
                     app->sensor.fan_name,
                     app->sensor.fan_rpm);
        } else if (app->sensor.storage_temperature_available) {
            snprintf(sensor_details,
                     sizeof(sensor_details),
                     "%s: %.1f C\nFan: Unavailable",
                     app->sensor.storage_name,
                     app->sensor.storage_temperature);
        } else if (app->sensor.fan_available) {
            snprintf(sensor_details,
                     sizeof(sensor_details),
                     "Storage: Unavailable\n%s: %.0f RPM",
                     app->sensor.fan_name,
                     app->sensor.fan_rpm);
        }
        gtk_label_set_text(GTK_LABEL(app->sensor_detail_label), sensor_details);
    } else {
        memset(&app->sensor, 0, sizeof(app->sensor));
        gtk_label_set_text(GTK_LABEL(app->sensor_detail_label), "Sensor metrics unavailable");
    }

    maybe_send_alerts(app);

    add_to_history(&app->history,
                   history_cpu,
                   history_memory,
                   history_gpu,
                   history_gpu_memory,
                   history_gpu_temp,
                   history_disk_read,
                   history_disk_write,
                   history_network_rx,
                   history_network_tx,
                   history_battery);

    if (snapshot->processes_ok) {
        memcpy(app->last_processes,
               snapshot->processes,
               sizeof(ProcessInfo) * snapshot->process_count);
        app->last_process_count = snapshot->process_count;
        render_process_stores(app);
    } else {
        app->last_process_count = 0;
        render_process_stores(app);
        gtk_label_set_text(GTK_LABEL(app->process_count_label), "Process data unavailable");
        gtk_label_set_text(GTK_LABEL(app->process_window_count_label), "Process data unavailable");
    }

    gtk_widget_queue_draw(app->history_area);
    gtk_widget_queue_draw(app->io_history_area);

    time_t now = time(NULL);
    struct tm local_time;
    char status[96];
    localtime_r(&now, &local_time);
    strftime(status, sizeof(status), "Live - updated %H:%M:%S", &local_time);
    gtk_label_set_text(GTK_LABEL(app->status_label), status);
}

static void collect_metrics_done(GObject *source_object, GAsyncResult *result, gpointer user_data) {
    AppState *app = user_data;
    GTask *task = G_TASK(result);
    MetricsSnapshot *snapshot = g_task_get_task_data(task);
    GError *error = NULL;
    (void)source_object;

    app->metrics_refresh_running = FALSE;

    if (!g_task_propagate_boolean(task, &error)) {
        g_clear_error(&error);
        return;
    }

    if (!snapshot ||
        snapshot->generation != app->metrics_generation ||
        snapshot->gpu_source != app->gpu_source) {
        refresh_metrics(app);
        return;
    }

    apply_metrics_snapshot(app, snapshot);
}

static gboolean refresh_metrics(gpointer user_data) {
    AppState *app = user_data;
    MetricsSnapshot *snapshot;
    GTask *task;

    if (app->metrics_refresh_running) {
        return G_SOURCE_CONTINUE;
    }

    snapshot = g_new0(MetricsSnapshot, 1);
    snapshot->gpu_source = app->gpu_source;
    snapshot->generation = app->metrics_generation;

    task = g_task_new(NULL, NULL, collect_metrics_done, app);
    g_task_set_task_data(task, snapshot, g_free);
    app->metrics_refresh_running = TRUE;
    g_task_run_in_thread(task, collect_metrics_task);
    g_object_unref(task);

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

static GtkWidget *create_refresh_selector(AppState *app) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *label = create_label("Refresh", "muted");
    GtkStyleContext *context;

    context = gtk_widget_get_style_context(box);
    gtk_style_context_add_class(context, "theme-selector");

    app->refresh_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->refresh_combo), "1s");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->refresh_combo), "2s");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->refresh_combo), "5s");
    update_refresh_combo(app);
    gtk_widget_set_tooltip_text(app->refresh_combo, "Metric refresh interval");
    g_signal_connect(app->refresh_combo, "changed", G_CALLBACK(on_refresh_interval_changed), app);

    gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), app->refresh_combo, FALSE, FALSE, 0);

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

static GtkWidget *create_io_history_panel(AppState *app) {
    GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GtkWidget *title = create_label("I/O History", "section-title");
    GtkWidget *legend = create_label("Disk read purple   Disk write orange   RX cyan   TX red", "muted");
    GtkStyleContext *context = gtk_widget_get_style_context(card);

    gtk_style_context_add_class(context, "card");
    gtk_widget_set_hexpand(card, TRUE);

    app->io_history_area = gtk_drawing_area_new();
    gtk_widget_set_size_request(app->io_history_area, -1, 220);
    g_signal_connect(app->io_history_area, "draw", G_CALLBACK(draw_io_history), app);

    gtk_box_pack_start(GTK_BOX(header), title, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(header), legend, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(card), header, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(card), app->io_history_area, TRUE, TRUE, 0);

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

static void toggle_main_window(AppState *app) {
    if (gtk_widget_get_visible(app->window)) {
        gtk_widget_hide(app->window);
    } else {
        gtk_widget_show_all(app->window);
        gtk_window_present(GTK_WINDOW(app->window));
    }
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
static gboolean tray_icon_is_usable(AppState *app) {
    return app->tray_icon && gtk_status_icon_is_embedded(app->tray_icon);
}
#pragma GCC diagnostic pop

static gboolean hide_main_window(GtkWidget *widget, GdkEvent *event, gpointer user_data) {
    AppState *app = user_data;
    (void)event;

    if (tray_icon_is_usable(app)) {
        save_settings(app);
        gtk_widget_hide(widget);
    } else {
        save_settings(app);
        gtk_main_quit();
    }

    return TRUE;
}

static void on_tray_activate(GtkStatusIcon *status_icon, gpointer user_data) {
    (void)status_icon;
    toggle_main_window(user_data);
}

static void on_tray_show(GtkMenuItem *item, gpointer user_data) {
    AppState *app = user_data;
    (void)item;

    gtk_widget_show_all(app->window);
    gtk_window_present(GTK_WINDOW(app->window));
}

static void on_tray_quit(GtkMenuItem *item, gpointer user_data) {
    AppState *app = user_data;
    (void)item;
    save_settings(app);
    gtk_main_quit();
}

static void on_tray_popup(GtkStatusIcon *status_icon,
                          guint button,
                          guint activate_time,
                          gpointer user_data) {
    GtkWidget *menu = gtk_menu_new();
    GtkWidget *show_item = gtk_menu_item_new_with_label("Show");
    GtkWidget *quit_item = gtk_menu_item_new_with_label("Quit");
    (void)status_icon;

    g_signal_connect(show_item, "activate", G_CALLBACK(on_tray_show), user_data);
    g_signal_connect(quit_item, "activate", G_CALLBACK(on_tray_quit), user_data);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), show_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), quit_item);
    gtk_widget_show_all(menu);
    gtk_menu_popup_at_pointer(GTK_MENU(menu), NULL);
    (void)button;
    (void)activate_time;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
static void create_tray_icon(AppState *app) {
    app->tray_icon = gtk_status_icon_new_from_icon_name("utilities-system-monitor");
    gtk_status_icon_set_tooltip_text(app->tray_icon, "System Monitor");
    gtk_status_icon_set_visible(app->tray_icon, TRUE);
    g_signal_connect(app->tray_icon, "activate", G_CALLBACK(on_tray_activate), app);
    g_signal_connect(app->tray_icon, "popup-menu", G_CALLBACK(on_tray_popup), app);
}
#pragma GCC diagnostic pop

static void create_process_window(AppState *app) {
    GtkWidget *page;
    GtkWidget *header;
    GtkWidget *controls;
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

    controls = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    app->process_search_entry = gtk_search_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(app->process_search_entry), "Search processes");
    gtk_widget_set_hexpand(app->process_search_entry, TRUE);
    app->process_sort_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->process_sort_combo), "CPU");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->process_sort_combo), "Memory");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->process_sort_combo), "Name");
    gtk_combo_box_set_active(GTK_COMBO_BOX(app->process_sort_combo), 0);
    app->process_refresh_button = gtk_button_new_with_label("Refresh");
    app->process_kill_button = gtk_button_new_with_label("Terminate");
    app->process_force_kill_button = gtk_button_new_with_label("Kill");
    set_widget_class(app->process_refresh_button, "command-button", TRUE);
    set_widget_class(app->process_kill_button, "command-button", TRUE);
    set_widget_class(app->process_force_kill_button, "command-button", TRUE);
    g_signal_connect(app->process_search_entry, "changed", G_CALLBACK(on_process_search_changed), app);
    g_signal_connect(app->process_sort_combo, "changed", G_CALLBACK(on_process_sort_changed), app);
    g_signal_connect(app->process_refresh_button, "clicked", G_CALLBACK(on_process_refresh_clicked), app);
    g_signal_connect(app->process_kill_button, "clicked", G_CALLBACK(on_kill_process_clicked), app);
    g_signal_connect(app->process_force_kill_button, "clicked", G_CALLBACK(on_force_kill_process_clicked), app);

    gtk_box_pack_start(GTK_BOX(controls), app->process_search_entry, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(controls), app->process_sort_combo, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(controls), app->process_refresh_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(controls), app->process_kill_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(controls), app->process_force_kill_button, FALSE, FALSE, 0);

    table = create_process_table(app->process_window_store, &app->process_window_view);

    gtk_box_pack_start(GTK_BOX(page), header, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(page), controls, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(page), table, TRUE, TRUE, 0);
}

static GtkWidget *create_process_panel(AppState *app) {
    GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GtkWidget *title = create_label("Top Processes", "section-title");
    GtkWidget *button = gtk_button_new_with_label("Open process window");
    GtkWidget *table = create_process_table(app->process_store, NULL);
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
    GtkWidget *refresh_selector;
    GtkWidget *process_button;
    GtkWidget *metrics_grid;
    GtkWidget *extra_grid;
    GtkWidget *main_grid;
    GtkWidget *cores_card;
    GtkWidget *cores_title;
    GtkWidget *history_panel;
    GtkWidget *io_history_panel;
    GtkWidget *process_panel;
    GtkStyleContext *context;

    app->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(app->window), APP_TITLE);
    gtk_window_set_default_size(GTK_WINDOW(app->window), app->window_width, app->window_height);
    gtk_window_set_position(GTK_WINDOW(app->window), GTK_WIN_POS_CENTER);
    g_signal_connect(app->window, "delete-event", G_CALLBACK(hide_main_window), app);

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
    refresh_selector = create_refresh_selector(app);
    process_button = gtk_button_new_with_label("Processes");
    set_widget_class(process_button, "command-button", TRUE);
    g_signal_connect(process_button, "clicked", G_CALLBACK(show_process_window), app);
    gtk_label_set_xalign(GTK_LABEL(app->status_label), 1.0);
    gtk_box_pack_start(GTK_BOX(header), title, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(header), theme_selector, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(header), refresh_selector, FALSE, FALSE, 0);
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

    extra_grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(extra_grid), 14);
    gtk_grid_set_row_spacing(GTK_GRID(extra_grid), 14);
    gtk_widget_set_hexpand(extra_grid, TRUE);
    gtk_grid_attach(GTK_GRID(extra_grid), create_info_card("Disk I/O", &app->disk_detail_label), 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(extra_grid), create_info_card("Network", &app->network_detail_label), 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(extra_grid), create_info_card("Battery", &app->battery_detail_label), 2, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(extra_grid), create_info_card("Sensors", &app->sensor_detail_label), 3, 0, 1, 1);
    gtk_grid_set_column_homogeneous(GTK_GRID(extra_grid), TRUE);
    gtk_box_pack_start(GTK_BOX(page), extra_grid, FALSE, FALSE, 0);

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
    io_history_panel = create_io_history_panel(app);
    process_panel = create_process_panel(app);

    gtk_grid_attach(GTK_GRID(main_grid), cores_card, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(main_grid), history_panel, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(main_grid), io_history_panel, 0, 1, 2, 1);
    gtk_grid_attach(GTK_GRID(main_grid), process_panel, 0, 2, 2, 1);
    gtk_grid_set_column_homogeneous(GTK_GRID(main_grid), TRUE);
    gtk_box_pack_start(GTK_BOX(page), main_grid, TRUE, TRUE, 0);

    create_process_window(app);
    create_tray_icon(app);
}

int main(int argc, char **argv) {
    AppState app;
    memset(&app, 0, sizeof(app));

    gtk_init(&argc, &argv);
    load_settings(&app);
    apply_theme_css(app.dark_theme);
    init_history(&app.history);
    build_ui(&app);
    set_app_theme(&app, app.dark_theme);

    gtk_widget_show_all(app.window);
    refresh_metrics(&app);
    restart_refresh_timer(&app);

    gtk_main();

    save_settings(&app);

    if (app.refresh_timer_id != 0) {
        g_source_remove(app.refresh_timer_id);
    }
    if (app.notification_bus) {
        g_object_unref(app.notification_bus);
    }

    return 0;
}
