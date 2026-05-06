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

static gchar *get_project_config_path(AppState *state) {
    if (state == NULL || state->current_dir == NULL) return NULL;

    gchar *dir_path = g_file_get_path(state->current_dir);
    if (dir_path == NULL) return NULL;

    gchar *config_path = g_build_filename(dir_path, ".fidet.ini", NULL);
    g_free(dir_path);
    return config_path;
}

gboolean file_ops_load_project_config(AppState *state, gchar **compile_args_out, GError **error) {
    g_return_val_if_fail(state != NULL, FALSE);

    if (compile_args_out != NULL) {
        *compile_args_out = g_strdup("");
    }

    gchar *config_path = get_project_config_path(state);
    if (config_path == NULL) {
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_INVAL, "No project directory selected.");
        return FALSE;
    }

    if (!g_file_test(config_path, G_FILE_TEST_EXISTS)) {
        g_free(config_path);
        return TRUE;
    }

    GKeyFile *key_file = g_key_file_new();
    gboolean ok = g_key_file_load_from_file(key_file, config_path, G_KEY_FILE_NONE, error);
    if (!ok) {
        g_key_file_unref(key_file);
        g_free(config_path);
        return FALSE;
    }

    gchar *compile_args = NULL;
    if (g_key_file_has_key(key_file, "build", "compiler_args", NULL)) {
        compile_args = g_key_file_get_string(key_file, "build", "compiler_args", error);
        if (compile_args == NULL) {
            g_key_file_unref(key_file);
            g_free(config_path);
            return FALSE;
        }
    } else {
        compile_args = g_strdup("");
    }

    if (compile_args_out != NULL) {
        g_free(*compile_args_out);
        *compile_args_out = compile_args;
    } else {
        g_free(compile_args);
    }

    g_key_file_unref(key_file);
    g_free(config_path);
    return TRUE;
}

gboolean file_ops_save_project_config(AppState *state, const gchar *compile_args, GError **error) {
    g_return_val_if_fail(state != NULL, FALSE);

    gchar *config_path = get_project_config_path(state);
    if (config_path == NULL) {
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_INVAL, "No project directory selected.");
        return FALSE;
    }

    GKeyFile *key_file = g_key_file_new();
    g_key_file_set_string(key_file, "build", "compiler_args", compile_args != NULL ? compile_args : "");

    gsize len = 0;
    gchar *data = g_key_file_to_data(key_file, &len, error);
    g_key_file_unref(key_file);
    if (data == NULL) {
        g_free(config_path);
        return FALSE;
    }

    gboolean ok = g_file_set_contents(config_path, data, (gssize)len, error);
    g_free(data);
    g_free(config_path);
    return ok;
}
