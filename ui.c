#include "ui.h"
#include "file_ops.h"
#include "build.h"

typedef struct {
    AppState *state;
    GtkWidget *window;
    GtkWidget *entry;
    void (*ok_cb)(AppState *, const gchar *);
} PromptData;

typedef struct {
    AppState *state;
    GtkWidget *window;
} RunBinaryConfirmData;

static void update_window_title(AppState *state) {
    const gchar *base_title = "Mini C IDE (GTK4)";

    if (state->current_file_path == NULL) {
        gtk_window_set_title(GTK_WINDOW(state->window), base_title);
        return;
    }

    gchar *name = g_path_get_basename(state->current_file_path);
    gchar *title = g_strdup_printf("%s%s - %s",
                                   state->is_dirty ? "● " : "",
                                   name,
                                   base_title);
    gtk_window_set_title(GTK_WINDOW(state->window), title);
    g_free(name);
    g_free(title);
}

void ui_set_dirty_state(AppState *state, gboolean dirty) {
    g_return_if_fail(state != NULL);
    state->is_dirty = dirty;
    if (state->window != NULL) {
        update_window_title(state);
    }
}

static void on_editor_buffer_changed(GtkTextBuffer *buffer, gpointer user_data) {
    (void)buffer;
    AppState *state = user_data;

    if (state->suppress_editor_change) return;
    if (state->current_file_path == NULL) return;
    if (state->is_dirty) return;

    ui_set_dirty_state(state, TRUE);
}

static void on_transient_close_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    gtk_window_destroy(GTK_WINDOW(user_data));
}

static void on_prompt_ok_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    PromptData *data = user_data;
    const gchar *txt = gtk_editable_get_text(GTK_EDITABLE(data->entry));
    if (txt != NULL && *txt != '\0') {
        data->ok_cb(data->state, txt);
    }
    gtk_window_destroy(GTK_WINDOW(data->window));
}

static void on_prompt_cancel_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    PromptData *data = user_data;
    gtk_window_destroy(GTK_WINDOW(data->window));
}

static void on_prompt_window_destroy(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    g_free(user_data);
}

static void on_file_list_right_click_pressed(GtkGestureClick *gesture, gint n_press,
                                             gdouble x, gdouble y, gpointer user_data) {
    (void)n_press;
    AppState *state = user_data;
    GMenuModel *menu_model = g_object_get_data(G_OBJECT(state->file_list), "menu-model");
    GtkWidget *popover = gtk_popover_menu_new_from_model(menu_model);
    gtk_popover_set_has_arrow(GTK_POPOVER(popover), FALSE);
    gtk_widget_set_parent(popover, state->file_list);

    GdkRectangle rect = {(int)x, (int)y, 1, 1};
    gtk_popover_set_pointing_to(GTK_POPOVER(popover), &rect);
    gtk_popover_popup(GTK_POPOVER(popover));
    gtk_gesture_set_state(GTK_GESTURE(gesture), GTK_EVENT_SEQUENCE_CLAIMED);
}

static gboolean is_text_like_filename(const gchar *name) {
    if (name == NULL || *name == '\0') return FALSE;

    return g_str_has_suffix(name, ".c") ||
           g_str_has_suffix(name, ".h") ||
           g_str_has_suffix(name, ".txt") ||
           g_str_has_suffix(name, ".md") ||
           g_str_has_suffix(name, ".json") ||
           g_str_has_suffix(name, ".yaml") ||
           g_str_has_suffix(name, ".yml") ||
           g_str_has_suffix(name, ".xml") ||
           g_str_has_suffix(name, ".ini") ||
           g_str_has_suffix(name, ".cfg") ||
           g_str_has_suffix(name, ".css") ||
           g_str_has_suffix(name, ".sh") ||
           g_strcmp0(name, "Makefile") == 0 ||
           g_strcmp0(name, "README") == 0 ||
           g_strcmp0(name, "README.md") == 0;
}

static gboolean has_suffix_case_insensitive(const gchar *name, const gchar *suffix) {
    if (name == NULL || suffix == NULL) return FALSE;
    gchar *lower_name = g_ascii_strdown(name, -1);
    gchar *lower_suffix = g_ascii_strdown(suffix, -1);
    gboolean result = g_str_has_suffix(lower_name, lower_suffix);
    g_free(lower_name);
    g_free(lower_suffix);
    return result;
}

static gboolean is_binary_like_filename(const gchar *name) {
    if (name == NULL || *name == '\0') return FALSE;

    return has_suffix_case_insensitive(name, ".exe") ||
           has_suffix_case_insensitive(name, ".bin") ||
           has_suffix_case_insensitive(name, ".out") ||
           has_suffix_case_insensitive(name, ".o") ||
           has_suffix_case_insensitive(name, ".so") ||
           has_suffix_case_insensitive(name, ".dll") ||
           has_suffix_case_insensitive(name, ".a");
}

static gboolean is_runnable_binary_path(const gchar *path, const gchar *name) {
    if (path == NULL || name == NULL) return FALSE;
    return g_file_test(path, G_FILE_TEST_IS_EXECUTABLE) ||
           has_suffix_case_insensitive(name, ".exe");
}

static void update_action_buttons_for_selection(AppState *state, gboolean text_like, gboolean runnable) {
    gtk_widget_set_visible(state->compile_btn, text_like);
    gtk_widget_set_visible(state->run_btn, text_like);
    gtk_widget_set_visible(state->run_binary_btn, runnable && !text_like);
}

static void set_runnable_selection(AppState *state, const gchar *path_or_null) {
    g_clear_pointer(&state->selected_runnable_path, g_free);
    if (path_or_null != NULL) {
        state->selected_runnable_path = g_strdup(path_or_null);
    }
}

static void run_selected_binary(AppState *state) {
    if (state->selected_runnable_path == NULL) {
        ui_set_output_text(state, "No runnable binary selected.\n");
        return;
    }

    gchar *out = NULL;
    gchar *err = NULL;
    gint status = 0;
    GError *spawn_err = NULL;
    char *argv[] = {state->selected_runnable_path, NULL};

    if (!g_spawn_sync(NULL, argv, NULL, G_SPAWN_SEARCH_PATH,
                      NULL, NULL, &out, &err, &status, &spawn_err)) {
        ui_set_output_text(state, "Failed to run selected binary:\n");
        ui_append_output_text(state, spawn_err->message);
        ui_append_output_text(state, "\n");
        g_clear_error(&spawn_err);
        g_free(out);
        g_free(err);
        return;
    }

    ui_set_output_text(state, "=== Run binary ===\n");
    ui_append_output_text(state, "Warning: Executed selected binary file.\n\n");
    if (out != NULL && *out != '\0') {
        ui_append_output_text(state, out);
    }
    if (err != NULL && *err != '\0') {
        ui_append_output_text(state, err);
    }

    if (status == 0) {
        ui_append_output_text(state, "Binary finished successfully.\n");
    } else {
        ui_append_output_text(state, "Binary exited with non-zero status.\n");
    }

    g_free(out);
    g_free(err);
}

static void on_run_binary_confirm_destroy(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    g_free(user_data);
}

static void on_run_binary_confirm_cancel(GtkButton *btn, gpointer user_data) {
    (void)btn;
    RunBinaryConfirmData *data = user_data;
    gtk_window_destroy(GTK_WINDOW(data->window));
}

static void on_run_binary_confirm_ok(GtkButton *btn, gpointer user_data) {
    (void)btn;
    RunBinaryConfirmData *data = user_data;
    run_selected_binary(data->state);
    gtk_window_destroy(GTK_WINDOW(data->window));
}

static void on_run_binary_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    AppState *state = user_data;
    if (state->selected_runnable_path == NULL) {
        ui_set_output_text(state, "No runnable binary selected.\n");
        return;
    }

    GtkWidget *confirm = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(confirm), "Confirm binary run");
    gtk_window_set_modal(GTK_WINDOW(confirm), TRUE);
    gtk_window_set_transient_for(GTK_WINDOW(confirm), GTK_WINDOW(state->window));

    GtkWidget *content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_start(content, 16);
    gtk_widget_set_margin_end(content, 16);
    gtk_widget_set_margin_top(content, 16);
    gtk_widget_set_margin_bottom(content, 16);

    gchar *msg = g_strdup_printf("Run selected executable?\n\n%s\n\nOnly run trusted files.",
                                 state->selected_runnable_path);
    GtkWidget *label = gtk_label_new(msg);
    g_free(msg);
    gtk_label_set_wrap(GTK_LABEL(label), TRUE);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);

    GtkWidget *btn_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(btn_row, GTK_ALIGN_END);
    GtkWidget *cancel_btn = gtk_button_new_with_label("Cancel");
    GtkWidget *ok_btn = gtk_button_new_with_label("Run");
    gtk_box_append(GTK_BOX(btn_row), cancel_btn);
    gtk_box_append(GTK_BOX(btn_row), ok_btn);

    RunBinaryConfirmData *data = g_new0(RunBinaryConfirmData, 1);
    data->state = state;
    data->window = confirm;

    g_signal_connect(cancel_btn, "clicked", G_CALLBACK(on_run_binary_confirm_cancel), data);
    g_signal_connect(ok_btn, "clicked", G_CALLBACK(on_run_binary_confirm_ok), data);
    g_signal_connect(confirm, "destroy", G_CALLBACK(on_run_binary_confirm_destroy), data);

    gtk_box_append(GTK_BOX(content), label);
    gtk_box_append(GTK_BOX(content), btn_row);
    gtk_window_set_child(GTK_WINDOW(confirm), content);
    gtk_window_present(GTK_WINDOW(confirm));
}

static void show_error_dialog(GtkWindow *parent, const gchar *title, const gchar *message) {
    GtkWidget *window = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(window), title != NULL ? title : "Error");
    gtk_window_set_modal(GTK_WINDOW(window), TRUE);
    if (parent != NULL) {
        gtk_window_set_transient_for(GTK_WINDOW(window), parent);
    }

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_start(box, 16);
    gtk_widget_set_margin_end(box, 16);
    gtk_widget_set_margin_top(box, 16);
    gtk_widget_set_margin_bottom(box, 16);

    GtkWidget *label = gtk_label_new(message != NULL ? message : "Unknown error");
    gtk_label_set_wrap(GTK_LABEL(label), TRUE);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);

    GtkWidget *ok_btn = gtk_button_new_with_label("OK");
    gtk_widget_set_halign(ok_btn, GTK_ALIGN_END);
    g_signal_connect(ok_btn, "clicked", G_CALLBACK(on_transient_close_clicked), window);

    gtk_box_append(GTK_BOX(box), label);
    gtk_box_append(GTK_BOX(box), ok_btn);
    gtk_window_set_child(GTK_WINDOW(window), box);
    gtk_window_present(GTK_WINDOW(window));
}

void ui_set_output_text(AppState *state, const gchar *text) {
    gtk_text_buffer_set_text(state->output_buffer, text != NULL ? text : "", -1);
}

void ui_append_output_text(AppState *state, const gchar *text) {
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(state->output_buffer, &end);
    gtk_text_buffer_insert(state->output_buffer, &end, text != NULL ? text : "", -1);
}

static void on_open_response(GObject *source, GAsyncResult *result, gpointer user_data) {
    AppState *state = user_data;
    GtkFileDialog *dialog = GTK_FILE_DIALOG(source);
    GError *error = NULL;
    GFile *folder = gtk_file_dialog_select_folder_finish(dialog, result, &error);

    if (folder == NULL) {
        if (error != NULL) g_error_free(error);
        return;
    }

    if (state->current_dir != NULL) {
        g_object_unref(state->current_dir);
    }
    state->current_dir = folder;

    g_clear_pointer(&state->current_file_path, g_free);
    set_runnable_selection(state, NULL);
    update_action_buttons_for_selection(state, TRUE, FALSE);
    ui_set_dirty_state(state, FALSE);
    state->suppress_editor_change = TRUE;
    gtk_text_buffer_set_text(state->editor_buffer, "", -1);
    state->suppress_editor_change = FALSE;
    ui_refresh_file_list(state);
    ui_set_output_text(state, "Project folder opened.\n");

    gchar *folder_path = g_file_get_path(state->current_dir);
    gchar *msg = g_strdup_printf("Project folder opened: %s\n", folder_path != NULL ? folder_path : "(unknown)");
    ui_append_output_text(state, msg);
    g_free(msg);
    g_free(folder_path);
}

static void on_open_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    AppState *state = user_data;

    GtkFileDialog *dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Open project folder");
    if (state->current_dir != NULL) {
        gtk_file_dialog_set_initial_folder(dialog, state->current_dir);
    }

    gtk_file_dialog_select_folder(dialog, GTK_WINDOW(state->window), NULL, on_open_response, state);

    g_object_unref(dialog);
}

static void on_save_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    AppState *state = user_data;
    GError *error = NULL;

    if (!file_ops_save_current(state, &error)) {
        show_error_dialog(GTK_WINDOW(state->window), "Save failed", error->message);
        g_clear_error(&error);
        return;
    }

    ui_set_output_text(state, "File saved.\n");
}

static void on_compile_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    build_compile_current((AppState *)user_data);
}

static void on_run_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    build_run_current((AppState *)user_data);
}

static GtkWidget *make_file_row(const gchar *filename, const gchar *full_path) {
    GtkWidget *row = gtk_list_box_row_new();
    GtkWidget *label = gtk_label_new(filename);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_widget_set_margin_start(label, 8);
    gtk_widget_set_margin_end(label, 8);
    gtk_widget_set_margin_top(label, 6);
    gtk_widget_set_margin_bottom(label, 6);
    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), label);

    g_object_set_data_full(G_OBJECT(row), "full-path", g_strdup(full_path), g_free);
    gtk_widget_add_css_class(row, "file-row");
    return row;
}

void ui_refresh_file_list(AppState *state) {
    g_return_if_fail(state != NULL);

    GtkListBox *list_box = GTK_LIST_BOX(state->file_list);
    GtkListBoxRow *row = gtk_list_box_get_row_at_index(list_box, 0);
    while (row != NULL) {
        gtk_list_box_remove(list_box, GTK_WIDGET(row));
        row = gtk_list_box_get_row_at_index(list_box, 0);
    }

    if (state->current_dir == NULL) return;

    gchar *dir_path = g_file_get_path(state->current_dir);
    GDir *dir = g_dir_open(dir_path, 0, NULL);
    if (dir == NULL) {
        g_free(dir_path);
        return;
    }

    const gchar *name = NULL;
    while ((name = g_dir_read_name(dir)) != NULL) {
        gchar *full = g_build_filename(dir_path, name, NULL);
        if (!g_file_test(full, G_FILE_TEST_IS_REGULAR)) {
            g_free(full);
            continue;
        }

        gboolean show = is_text_like_filename(name) ||
                        is_binary_like_filename(name) ||
                        g_file_test(full, G_FILE_TEST_IS_EXECUTABLE);
        if (!show) {
            g_free(full);
            continue;
        }

        GtkWidget *row = make_file_row(name, full);
        gtk_list_box_append(GTK_LIST_BOX(state->file_list), row);
        g_free(full);
    }

    g_dir_close(dir);
    g_free(dir_path);
}

void ui_select_file_in_list(AppState *state, const gchar *filepath) {
    g_return_if_fail(state != NULL);
    if (filepath == NULL) return;

    GtkWidget *row = gtk_widget_get_first_child(state->file_list);
    while (row != NULL) {
        const gchar *row_path = g_object_get_data(G_OBJECT(row), "full-path");
        if (row_path != NULL && g_strcmp0(row_path, filepath) == 0) {
            gtk_list_box_select_row(GTK_LIST_BOX(state->file_list), GTK_LIST_BOX_ROW(row));
            break;
        }
        row = gtk_widget_get_next_sibling(row);
    }
}

static void on_file_selected(GtkListBox *box, GtkListBoxRow *row, gpointer user_data) {
    (void)box;
    AppState *state = user_data;
    if (row == NULL) {
        set_runnable_selection(state, NULL);
        update_action_buttons_for_selection(state, TRUE, FALSE);
        return;
    }

    const gchar *path = g_object_get_data(G_OBJECT(row), "full-path");
    if (path == NULL) return;

    gchar *name = g_path_get_basename(path);
    gboolean text_like = is_text_like_filename(name);
    gboolean runnable = is_runnable_binary_path(path, name);
    g_free(name);

    set_runnable_selection(state, runnable ? path : NULL);
    update_action_buttons_for_selection(state, text_like, runnable);

    if (!text_like) {
        g_clear_pointer(&state->current_file_path, g_free);
        ui_set_dirty_state(state, FALSE);
        state->suppress_editor_change = TRUE;
        gtk_text_buffer_set_text(state->editor_buffer,
                                 "Binary/executable file selected. Editing is disabled for this file type.\n", -1);
        state->suppress_editor_change = FALSE;
        return;
    }

    GError *error = NULL;
    if (!file_ops_open_file(state, path, &error)) {
        show_error_dialog(GTK_WINDOW(state->window), "Open failed", error->message);
        g_clear_error(&error);
    }
}

static void do_create_file(AppState *state, const gchar *name) {
    GError *error = NULL;
    if (!file_ops_create_new(state, name, &error)) {
        show_error_dialog(GTK_WINDOW(state->window), "Create failed", error->message);
        g_clear_error(&error);
        return;
    }
    ui_refresh_file_list(state);
}

static void do_rename_file(AppState *state, const gchar *old_path, const gchar *new_name) {
    GError *error = NULL;
    if (!file_ops_rename_file(state, old_path, new_name, &error)) {
        show_error_dialog(GTK_WINDOW(state->window), "Rename failed", error->message);
        g_clear_error(&error);
        return;
    }
    ui_refresh_file_list(state);
    if (state->current_file_path != NULL) {
        ui_select_file_in_list(state, state->current_file_path);
    }
}

static void prompt_text(GtkWindow *parent, const gchar *title, const gchar *placeholder,
                        void (*ok_cb)(AppState *, const gchar *), AppState *state) {
    GtkWidget *dlg = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(dlg), title != NULL ? title : "Input");
    gtk_window_set_modal(GTK_WINDOW(dlg), TRUE);
    gtk_window_set_transient_for(GTK_WINDOW(dlg), parent);

    GtkWidget *content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_start(content, 16);
    gtk_widget_set_margin_end(content, 16);
    gtk_widget_set_margin_top(content, 16);
    gtk_widget_set_margin_bottom(content, 16);

    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), placeholder);
    gtk_box_append(GTK_BOX(content), entry);

    GtkWidget *btn_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(btn_row, GTK_ALIGN_END);
    GtkWidget *cancel_btn = gtk_button_new_with_label("Cancel");
    GtkWidget *ok_btn = gtk_button_new_with_label("OK");
    gtk_box_append(GTK_BOX(btn_row), cancel_btn);
    gtk_box_append(GTK_BOX(btn_row), ok_btn);
    gtk_box_append(GTK_BOX(content), btn_row);

    PromptData *data = g_new0(PromptData, 1);
    data->state = state;
    data->window = dlg;
    data->entry = entry;
    data->ok_cb = ok_cb;

    g_signal_connect(ok_btn, "clicked", G_CALLBACK(on_prompt_ok_clicked), data);
    g_signal_connect(cancel_btn, "clicked", G_CALLBACK(on_prompt_cancel_clicked), data);
    g_signal_connect(dlg, "destroy", G_CALLBACK(on_prompt_window_destroy), data);

    gtk_window_set_child(GTK_WINDOW(dlg), content);
    gtk_window_present(GTK_WINDOW(dlg));
}

static void on_create_action(GSimpleAction *action, GVariant *param, gpointer user_data) {
    (void)action; (void)param;
    AppState *state = user_data;
    prompt_text(GTK_WINDOW(state->window), "Create new file", "example.c", do_create_file, state);
}

static void rename_ok_cb(AppState *state, const gchar *new_name) {
    GtkListBoxRow *row = gtk_list_box_get_selected_row(GTK_LIST_BOX(state->file_list));
    if (row == NULL) return;
    const gchar *old_path = g_object_get_data(G_OBJECT(row), "full-path");
    if (old_path == NULL) return;
    do_rename_file(state, old_path, new_name);
}

static void on_rename_action(GSimpleAction *action, GVariant *param, gpointer user_data) {
    (void)action; (void)param;
    AppState *state = user_data;
    prompt_text(GTK_WINDOW(state->window), "Rename file", "new_name.c", rename_ok_cb, state);
}

void ui_load_css(void) {
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_path(provider, "style.css");

    gtk_style_context_add_provider_for_display(gdk_display_get_default(),
                                               GTK_STYLE_PROVIDER(provider),
                                               GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
}

void ui_build(AppState *state) {
    state->window = gtk_application_window_new(state->app);
    gtk_window_set_title(GTK_WINDOW(state->window), "Mini C IDE (GTK4)");
    gtk_window_set_default_size(GTK_WINDOW(state->window), 1100, 720);
    state->is_dirty = FALSE;
    state->suppress_editor_change = FALSE;

    state->headerbar = gtk_header_bar_new();
    gtk_window_set_titlebar(GTK_WINDOW(state->window), state->headerbar);

    state->open_btn = gtk_button_new_with_label("Open");
    state->save_btn = gtk_button_new_with_label("Save");
    state->compile_btn = gtk_button_new_with_label("Compile");
    state->run_btn = gtk_button_new_with_label("Run");
    state->run_binary_btn = gtk_button_new_with_label("Run");
    gtk_widget_set_visible(state->run_binary_btn, FALSE);

    gtk_header_bar_pack_start(GTK_HEADER_BAR(state->headerbar), state->open_btn);
    gtk_header_bar_pack_start(GTK_HEADER_BAR(state->headerbar), state->save_btn);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(state->headerbar), state->run_binary_btn);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(state->headerbar), state->run_btn);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(state->headerbar), state->compile_btn);

    g_signal_connect(state->open_btn, "clicked", G_CALLBACK(on_open_clicked), state);
    g_signal_connect(state->save_btn, "clicked", G_CALLBACK(on_save_clicked), state);
    g_signal_connect(state->compile_btn, "clicked", G_CALLBACK(on_compile_clicked), state);
    g_signal_connect(state->run_btn, "clicked", G_CALLBACK(on_run_clicked), state);
    g_signal_connect(state->run_binary_btn, "clicked", G_CALLBACK(on_run_binary_clicked), state);

    state->main_vpaned = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
    state->main_hpaned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_paned_set_start_child(GTK_PANED(state->main_vpaned), state->main_hpaned);

    state->file_list = gtk_list_box_new();
    gtk_widget_add_css_class(state->file_list, "file-list");
    g_signal_connect(state->file_list, "row-selected", G_CALLBACK(on_file_selected), state);

    GtkWidget *left_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(left_scroll), state->file_list);
    gtk_widget_set_size_request(left_scroll, 220, -1);

    state->editor_view = gtk_text_view_new();
    state->editor_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(state->editor_view));
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(state->editor_view), TRUE);
    g_signal_connect(state->editor_buffer, "changed", G_CALLBACK(on_editor_buffer_changed), state);

    GtkWidget *editor_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(editor_scroll), state->editor_view);

    state->output_view = gtk_text_view_new();
    state->output_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(state->output_view));
    gtk_text_view_set_editable(GTK_TEXT_VIEW(state->output_view), FALSE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(state->output_view), TRUE);
    gtk_widget_add_css_class(state->output_view, "output-view");

    GtkWidget *output_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(output_scroll), state->output_view);

    gtk_paned_set_start_child(GTK_PANED(state->main_hpaned), left_scroll);
    gtk_paned_set_end_child(GTK_PANED(state->main_hpaned), editor_scroll);
    gtk_paned_set_position(GTK_PANED(state->main_hpaned), 220);
    gtk_paned_set_end_child(GTK_PANED(state->main_vpaned), output_scroll);

    gtk_window_set_child(GTK_WINDOW(state->window), state->main_vpaned);

    // Контекстне меню на списку файлів (Create / Rename)
    GSimpleActionGroup *group = g_simple_action_group_new();
    GSimpleAction *create_action = g_simple_action_new("create", NULL);
    GSimpleAction *rename_action = g_simple_action_new("rename", NULL);
    g_signal_connect(create_action, "activate", G_CALLBACK(on_create_action), state);
    g_signal_connect(rename_action, "activate", G_CALLBACK(on_rename_action), state);
    g_action_map_add_action(G_ACTION_MAP(group), G_ACTION(create_action));
    g_action_map_add_action(G_ACTION_MAP(group), G_ACTION(rename_action));
    gtk_widget_insert_action_group(state->file_list, "list", G_ACTION_GROUP(group));

    GMenu *menu = g_menu_new();
    g_menu_append(menu, "Create", "list.create");
    g_menu_append(menu, "Rename", "list.rename");

    GtkGesture *right_click = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(right_click), GDK_BUTTON_SECONDARY);
    g_signal_connect(right_click, "pressed", G_CALLBACK(on_file_list_right_click_pressed), state);

    g_object_set_data_full(G_OBJECT(state->file_list), "menu-model", menu, g_object_unref);
    gtk_widget_add_controller(state->file_list, GTK_EVENT_CONTROLLER(right_click));

    g_object_unref(group);
    g_object_unref(create_action);
    g_object_unref(rename_action);

    ui_set_dirty_state(state, FALSE);
    gtk_window_present(GTK_WINDOW(state->window));
}