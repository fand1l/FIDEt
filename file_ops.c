#include "file_ops.h"
#include <glib/gstdio.h>
#include <errno.h>

gboolean file_ops_open_file(AppState *state, const gchar *path, GError **error) {
    g_return_val_if_fail(state != NULL, FALSE);
    g_return_val_if_fail(path != NULL, FALSE);

    gchar *content = NULL;
    gsize len = 0;

    if (!g_file_get_contents(path, &content, &len, error)) {
        return FALSE;
    }

    state->suppress_editor_change = TRUE;
    gtk_text_buffer_set_text(state->editor_buffer, content, (gint)len);
    state->suppress_editor_change = FALSE;

    g_free(state->current_file_path);
    state->current_file_path = g_strdup(path);
    ui_set_dirty_state(state, FALSE);

    g_free(content);
    return TRUE;
}

gboolean file_ops_save_current(AppState *state, GError **error) {
    g_return_val_if_fail(state != NULL, FALSE);

    if (state->current_file_path == NULL) {
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_INVAL, "No file selected to save.");
        return FALSE;
    }

    GtkTextIter start;
    GtkTextIter end;
    gtk_text_buffer_get_start_iter(state->editor_buffer, &start);
    gtk_text_buffer_get_end_iter(state->editor_buffer, &end);
    gchar *text = gtk_text_buffer_get_text(state->editor_buffer, &start, &end, FALSE);

    gboolean ok = g_file_set_contents(state->current_file_path, text, -1, error);
    if (ok) {
        ui_set_dirty_state(state, FALSE);
    }
    g_free(text);
    return ok;
}

gboolean file_ops_create_new(AppState *state, const gchar *filename, GError **error) {
    g_return_val_if_fail(state != NULL, FALSE);
    g_return_val_if_fail(filename != NULL, FALSE);

    if (state->current_dir == NULL) {
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_INVAL, "No current directory selected.");
        return FALSE;
    }

    gchar *dir_path = g_file_get_path(state->current_dir);
    gchar *full_path = g_build_filename(dir_path, filename, NULL);

    if (g_file_test(full_path, G_FILE_TEST_EXISTS)) {
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_EXIST, "File already exists: %s", full_path);
        g_free(dir_path);
        g_free(full_path);
        return FALSE;
    }

    gboolean ok = g_file_set_contents(full_path, "", 0, error);

    g_free(dir_path);
    g_free(full_path);
    return ok;
}

gboolean file_ops_rename_file(AppState *state, const gchar *old_path, const gchar *new_name, GError **error) {
    g_return_val_if_fail(state != NULL, FALSE);
    g_return_val_if_fail(old_path != NULL, FALSE);
    g_return_val_if_fail(new_name != NULL, FALSE);

    gchar *dir = g_path_get_dirname(old_path);
    gchar *new_path = g_build_filename(dir, new_name, NULL);

    if (g_rename(old_path, new_path) != 0) {
        g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(errno),
                    "Rename failed: %s", g_strerror(errno));
        g_free(dir);
        g_free(new_path);
        return FALSE;
    }

    if (state->current_file_path != NULL && g_strcmp0(state->current_file_path, old_path) == 0) {
        g_free(state->current_file_path);
        state->current_file_path = g_strdup(new_path);
    }

    g_free(dir);
    g_free(new_path);
    return TRUE;
}