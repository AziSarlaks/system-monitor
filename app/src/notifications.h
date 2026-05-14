#ifndef NOTIFICATIONS_H
#define NOTIFICATIONS_H

#include "app_state.h"

void send_desktop_notification(AppState *app, const char *summary, const char *body);
void show_error_dialog(GtkWindow *parent, const char *title, const char *message);
void maybe_send_alerts(AppState *app);

#endif
