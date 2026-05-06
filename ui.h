#ifndef UI_H
#define UI_H

#include <gtk/gtk.h>

typedef struct _AppState {
    GtkApplication *app;
    GtkWidget *window;

    GtkWidget *headerbar;
    GtkWidget *open_btn;
    GtkWidget *save_btn;
    GtkWidget *compile_btn;
    GtkWidget *compile_menu_btn;
    GtkWidget *run_btn;
    GtkWidget *run_menu_btn;
    GtkWidget *run_binary_btn;
    GtkWidget *run_args_entry;

    GtkWidget *file_list;      // GtkListBox
    GtkWidget *editor_view;    // GtkTextView
    GtkTextBuffer *editor_buffer;

    GtkWidget *output_view;    // GtkTextView
    GtkTextBuffer *output_buffer;

    GtkWidget *main_vpaned;
    GtkWidget *main_hpaned;

    GFile *current_dir;
    gchar *current_file_path;
    gchar *selected_runnable_path;
    gchar *compile_args;
    gboolean is_dirty;
    gboolean suppress_editor_change;
    GHashTable *expanded_folders;  // Tracks expanded folder paths
} AppState;

void ui_build(AppState *state);
void ui_load_css(void);
void ui_set_output_text(AppState *state, const gchar *text);
void ui_append_output_text(AppState *state, const gchar *text);
void ui_set_dirty_state(AppState *state, gboolean dirty);
void ui_refresh_file_list(AppState *state);
void ui_select_file_in_list(AppState *state, const gchar *filepath);

#endif