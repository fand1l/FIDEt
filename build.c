#include "build.h"
#include "file_ops.h"

static gchar *make_output_binary_path(const gchar *source_file) {
    gchar *dir = g_path_get_dirname(source_file);
    gchar *base = g_path_get_basename(source_file);
    gchar *dot = g_strrstr(base, ".c");
    if (dot != NULL && dot[2] == '\0') {
        *dot = '\0';
    }
    gchar *out = g_build_filename(dir, base, NULL);
    g_free(dir);
    g_free(base);
    return out;
}

static gboolean run_process(char **argv, gchar **stdout_str, gchar **stderr_str, gint *exit_status, GError **error) {
    return g_spawn_sync(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL,
                        stdout_str, stderr_str, exit_status, error);
}

static gboolean make_spawn_argv(const gchar *program, const gchar *args_text, gchar ***argv_out, GError **error) {
    g_return_val_if_fail(program != NULL, FALSE);
    g_return_val_if_fail(argv_out != NULL, FALSE);

    gchar **args = NULL;
    gint args_count = 0;

    if (args_text != NULL && *args_text != '\0') {
        if (!g_shell_parse_argv(args_text, &args_count, &args, error)) {
            return FALSE;
        }
    }

    gchar **argv = g_new0(gchar *, args_count + 2);
    argv[0] = g_strdup(program);
    for (gint i = 0; i < args_count; i++) {
        argv[i + 1] = g_strdup(args[i]);
    }

    g_strfreev(args);
    *argv_out = argv;
    return TRUE;
}

static gboolean run_process_with_args(const gchar *program, const gchar *args_text,
                                      gchar **stdout_str, gchar **stderr_str,
                                      gint *exit_status, GError **error) {
    gchar **argv = NULL;
    if (!make_spawn_argv(program, args_text, &argv, error)) {
        return FALSE;
    }

    gboolean ok = run_process(argv, stdout_str, stderr_str, exit_status, error);
    g_strfreev(argv);
    return ok;
}

void build_compile_current(AppState *state) {
    g_return_if_fail(state != NULL);

    if (state->current_file_path == NULL) {
        ui_set_output_text(state, "No file selected.\n");
        return;
    }

    GError *save_err = NULL;
    if (!file_ops_save_current(state, &save_err)) {
        ui_set_output_text(state, save_err->message);
        ui_append_output_text(state, "\n");
        g_clear_error(&save_err);
        return;
    }

    gchar *out_bin = make_output_binary_path(state->current_file_path);
    gchar **user_args = NULL;
    gint user_count = 0;
    GError *parse_err = NULL;
    if (state->compile_args != NULL && *state->compile_args != '\0') {
        if (!g_shell_parse_argv(state->compile_args, &user_count, &user_args, &parse_err)) {
            ui_set_output_text(state, "Invalid compiler arguments:\n");
            ui_append_output_text(state, parse_err->message);
            ui_append_output_text(state, "\n");
            g_clear_error(&parse_err);
            g_free(out_bin);
            return;
        }
    }

    gchar **argv = g_new0(gchar *, user_count + 6 + 1);
    gint idx = 0;
    argv[idx++] = g_strdup("gcc");
    for (gint i = 0; i < user_count; i++) {
        argv[idx++] = g_strdup(user_args[i]);
    }
    argv[idx++] = g_strdup("-Wall");
    argv[idx++] = g_strdup("-Wextra");
    argv[idx++] = g_strdup("-pedantic");
    argv[idx++] = g_strdup(state->current_file_path);
    argv[idx++] = g_strdup("-o");
    argv[idx++] = g_strdup(out_bin);

    gchar *out = NULL;
    gchar *err = NULL;
    gint status = 0;
    GError *spawn_err = NULL;

    if (!run_process(argv, &out, &err, &status, &spawn_err)) {
        ui_set_output_text(state, "Failed to run gcc:\n");
        ui_append_output_text(state, spawn_err->message);
        ui_append_output_text(state, "\n");
        g_clear_error(&spawn_err);
        g_free(out_bin);
        g_strfreev(user_args);
        g_strfreev(argv);
        g_free(out);
        g_free(err);
        return;
    }

    ui_set_output_text(state, "=== Compile ===\n");
    if (out != NULL && *out != '\0') {
        ui_append_output_text(state, out);
    }
    if (err != NULL && *err != '\0') {
        ui_append_output_text(state, err);
    }

    if (status == 0) {
        ui_append_output_text(state, "Compilation successful.\n");
    } else {
        ui_append_output_text(state, "Compilation failed.\n");
    }

    g_free(out_bin);
    g_strfreev(user_args);
    g_strfreev(argv);
    g_free(out);
    g_free(err);
}

void build_run_current(AppState *state, const gchar *args_text) {
    g_return_if_fail(state != NULL);

    if (state->current_file_path == NULL) {
        ui_set_output_text(state, "No file selected.\n");
        return;
    }

    GError *save_err = NULL;
    if (!file_ops_save_current(state, &save_err)) {
        ui_set_output_text(state, save_err->message);
        ui_append_output_text(state, "\n");
        g_clear_error(&save_err);
        return;
    }

    gchar *out_bin = make_output_binary_path(state->current_file_path);

    char *compile_argv[] = {"gcc", "-Wall", "-Wextra", "-pedantic",
                            state->current_file_path, "-o", out_bin, NULL};
    gchar *c_out = NULL;
    gchar *c_err = NULL;
    gint c_status = 0;
    GError *c_spawn_err = NULL;

    if (!run_process(compile_argv, &c_out, &c_err, &c_status, &c_spawn_err)) {
        ui_set_output_text(state, "Failed to run gcc:\n");
        ui_append_output_text(state, c_spawn_err->message);
        ui_append_output_text(state, "\n");
        g_clear_error(&c_spawn_err);
        g_free(out_bin);
        g_free(c_out);
        g_free(c_err);
        return;
    }

    ui_set_output_text(state, "=== Compile ===\n");
    if (c_out != NULL && *c_out != '\0') {
        ui_append_output_text(state, c_out);
    }
    if (c_err != NULL && *c_err != '\0') {
        ui_append_output_text(state, c_err);
    }

    if (c_status != 0) {
        ui_append_output_text(state, "Compilation failed. Run aborted.\n");
        g_free(out_bin);
        g_free(c_out);
        g_free(c_err);
        return;
    }

    ui_append_output_text(state, "Compilation successful.\n");
    ui_append_output_text(state, "=== Run ===\n");

    gchar *r_out = NULL;
    gchar *r_err = NULL;
    gint r_status = 0;
    GError *r_spawn_err = NULL;

    if (!run_process_with_args(out_bin, args_text, &r_out, &r_err, &r_status, &r_spawn_err)) {
        ui_append_output_text(state, "Failed to run program:\n");
        ui_append_output_text(state, r_spawn_err->message);
        ui_append_output_text(state, "\n");
        g_clear_error(&r_spawn_err);
        g_free(out_bin);
        g_free(c_out);
        g_free(c_err);
        g_free(r_out);
        g_free(r_err);
        return;
    }

    if (r_out != NULL && *r_out != '\0') {
        ui_append_output_text(state, r_out);
    }
    if (r_err != NULL && *r_err != '\0') {
        ui_append_output_text(state, r_err);
    }

    if (r_status == 0) {
        ui_append_output_text(state, "Program finished successfully.\n");
    } else {
        ui_append_output_text(state, "Program exited with non-zero status.\n");
    }

    g_free(out_bin);
    g_free(c_out);
    g_free(c_err);
    g_free(r_out);
    g_free(r_err);
}