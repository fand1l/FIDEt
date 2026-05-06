#include "ui.h"
#include <string.h>

static void app_state_free(gpointer data) {
    AppState *state = data;
    if (state == NULL) return;
    if (state->current_dir != NULL) g_object_unref(state->current_dir);
    g_free(state->current_file_path);
    g_free(state->selected_runnable_path);
    g_free(state->compile_args);
    if (state->expanded_folders != NULL) g_hash_table_unref(state->expanded_folders);
    g_free(state);
}

static void app_activate(GtkApplication *app, gpointer user_data) {
    (void)user_data;

    AppState *state = g_new0(AppState, 1);
    state->app = app;
    state->compile_args = g_strdup("");
    state->expanded_folders = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

    // Поточна директорія як корінь для file browser
    gchar *cwd = g_get_current_dir();
    state->current_dir = g_file_new_for_path(cwd);
    g_free(cwd);

    ui_load_css();
    ui_build(state);
    ui_refresh_file_list(state);

    // Звільнення AppState при закритті вікна
    g_object_set_data_full(G_OBJECT(state->window), "app-state", state, app_state_free);
}

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new("com.fand1l.FIDET", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(app_activate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}