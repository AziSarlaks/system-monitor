#include "notifications.h"

#include <stdio.h>

#include "app_metrics.h"

void send_desktop_notification(AppState *app, const char *summary, const char *body) {
    GVariantBuilder actions;
    GVariantBuilder hints;
    GError *error = NULL;

    if (!app || !summary || !body) {
        return;
    }

    if (!app->notification_bus) {
        app->notification_bus = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
        if (!app->notification_bus) {
            g_clear_error(&error);
            return;
        }
    }

    g_variant_builder_init(&actions, G_VARIANT_TYPE("as"));
    g_variant_builder_init(&hints, G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add(&hints, "{sv}", "urgency", g_variant_new_byte(1));

    g_dbus_connection_call(app->notification_bus,
                           "org.freedesktop.Notifications",
                           "/org/freedesktop/Notifications",
                           "org.freedesktop.Notifications",
                           "Notify",
                           g_variant_new("(susssasa{sv}i)",
                                         APP_TITLE,
                                         0,
                                         "utilities-system-monitor",
                                         summary,
                                         body,
                                         &actions,
                                         &hints,
                                         ALERT_NOTIFICATION_EXPIRE_MS),
                           NULL,
                           G_DBUS_CALL_FLAGS_NONE,
                           1000,
                           NULL,
                           NULL,
                           NULL);
}

void show_error_dialog(GtkWindow *parent, const char *title, const char *message) {
    GtkWidget *dialog;

    dialog = gtk_message_dialog_new(parent,
                                    GTK_DIALOG_MODAL,
                                    GTK_MESSAGE_ERROR,
                                    GTK_BUTTONS_CLOSE,
                                    "%s",
                                    title);
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog), "%s", message);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

void maybe_send_alerts(AppState *app) {
    char body[256];

    if (!app) {
        return;
    }

    if (app_alert_update_high(app->cpu_curr.temperature,
                              ALERT_CPU_TEMP_C,
                              ALERT_CPU_TEMP_C - 10.0,
                              1,
                              &app->cpu_temp_alert.active,
                              &app->cpu_temp_alert.samples)) {
        snprintf(body, sizeof(body), "CPU temperature is %.1f C.", app->cpu_curr.temperature);
        send_desktop_notification(app, "High CPU temperature", body);
    }

    if (app->gpu.temperature > 0.0 &&
        app_alert_update_high(app->gpu.temperature,
                              ALERT_GPU_TEMP_C,
                              ALERT_GPU_TEMP_C - 10.0,
                              1,
                              &app->gpu_temp_alert.active,
                              &app->gpu_temp_alert.samples)) {
        snprintf(body, sizeof(body), "%s temperature is %.1f C.", app->gpu.name, app->gpu.temperature);
        send_desktop_notification(app, "High GPU temperature", body);
    }

    if (app_battery_status_can_alert(&app->battery) &&
        app_alert_update_low(app->battery.percentage,
                             ALERT_LOW_BATTERY_PERCENT,
                             ALERT_BATTERY_RECOVERY_PERCENT,
                             1,
                             &app->battery_low_alert.active,
                             &app->battery_low_alert.samples)) {
        snprintf(body, sizeof(body), "Battery is %.0f%% (%s).", app->battery.percentage, app->battery.status);
        send_desktop_notification(app, "Low battery", body);
    } else if (!app_battery_status_can_alert(&app->battery)) {
        app->battery_low_alert.active = 0;
        app->battery_low_alert.samples = 0;
    }

    if (app_alert_update_high(app->cpu_curr.usage_percent,
                              ALERT_HIGH_LOAD_PERCENT,
                              ALERT_LOAD_RECOVERY_PERCENT,
                              ALERT_LOAD_REQUIRED_SAMPLES,
                              &app->cpu_load_alert.active,
                              &app->cpu_load_alert.samples)) {
        snprintf(body, sizeof(body), "CPU usage has stayed above %.0f%%.", ALERT_HIGH_LOAD_PERCENT);
        send_desktop_notification(app, "High CPU load", body);
    }

    if (app_alert_update_high(app->memory.percentage,
                              ALERT_HIGH_LOAD_PERCENT,
                              ALERT_LOAD_RECOVERY_PERCENT,
                              ALERT_LOAD_REQUIRED_SAMPLES,
                              &app->memory_load_alert.active,
                              &app->memory_load_alert.samples)) {
        snprintf(body, sizeof(body), "Memory usage is %.0f%%.", app->memory.percentage);
        send_desktop_notification(app, "High memory load", body);
    }

    if (app_alert_update_high(app->gpu.usage,
                              ALERT_HIGH_LOAD_PERCENT,
                              ALERT_LOAD_RECOVERY_PERCENT,
                              ALERT_LOAD_REQUIRED_SAMPLES,
                              &app->gpu_load_alert.active,
                              &app->gpu_load_alert.samples)) {
        snprintf(body, sizeof(body), "GPU usage has stayed above %.0f%%.", ALERT_HIGH_LOAD_PERCENT);
        send_desktop_notification(app, "High GPU load", body);
    }
}
