#ifndef FILE_OPS_H
#define FILE_OPS_H

#include "ui.h"

gboolean file_ops_open_file(AppState *state, const gchar *path, GError **error);
gboolean file_ops_save_current(AppState *state, GError **error);
gboolean file_ops_create_new(AppState *state, const gchar *filename, GError **error);
gboolean file_ops_rename_file(AppState *state, const gchar *old_path, const gchar *new_name, GError **error);
gboolean file_ops_load_project_config(AppState *state, gchar **compile_args_out, GError **error);
gboolean file_ops_save_project_config(AppState *state, const gchar *compile_args, GError **error);

#endif