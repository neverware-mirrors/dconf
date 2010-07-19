#include <dconf.h>

static const gchar *
shift (int *argc, char ***argv)
{
  if (argc == 0)
    return NULL;

  (*argc)--;
  return *(*argv)++;
}

static gboolean
grab_args (int           argc,
           char        **argv,
           const gchar  *description,
           GError      **error,
           gint          num,
           ...)
{
  va_list ap;

  if (argc != num)
    {
      g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_FAILED,
                   "require exactly %d arguments: %s", num, description);
      return FALSE;
    }

  va_start (ap, num);
  while (num--)
    *va_arg (ap, gchar **) = *argv++;
  va_end (ap);

  return TRUE;
}

static gboolean
ensure (const gchar  *type,
        const gchar  *string,
        gboolean    (*checker) (const gchar *string),
        GError      **error)
{
  if (!checker (string))
    {
      g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                   "'%s' is not a dconf %s", string, type);
      return FALSE;
    }

  return TRUE;
}

static void
write_done (GObject      *object,
            GAsyncResult *result,
            gpointer      user_data)
{
  GError *error = NULL;

  if (!dconf_client_write_finish (DCONF_CLIENT (object), result, NULL, &error))
    g_error ("%s\n", error->message);

  g_print ("done\n");
}

static void
do_async_command (DConfClient  *client,
                  int           argc,
                  char        **argv)
{
  const gchar *cmd;

  cmd = shift (&argc, &argv);

  if (g_strcmp0 (cmd, "write") == 0)
    {
      const gchar *key, *strval;
      GVariant *value;

      if (!grab_args (argc, argv, "key and value", NULL, 2, &key, &strval))
        g_assert_not_reached ();

      if (!ensure ("key", key, dconf_is_key, NULL))
        g_assert_not_reached ();

      value = g_variant_parse (NULL, strval, NULL, NULL, NULL);

      g_assert (value != NULL);

      return dconf_client_write_async (client, key, value,
                                       NULL, write_done, NULL);
    }
}

static gboolean
do_sync_command (DConfClient  *client,
                 int           argc,
                 char        **argv,
                 GError      **error)
{
  const gchar *cmd;

  cmd = shift (&argc, &argv);

  if (g_strcmp0 (cmd, "async") == 0)
    {
      do_async_command (client, argc, argv);
      g_main_loop_run (g_main_loop_new (NULL, FALSE));
      return TRUE;
    }

  else if (g_strcmp0 (cmd, "read") == 0)
    {
      const gchar *key;
      GVariant *value;
      gchar *printed;

      if (!grab_args (argc, argv, "key", error, 1, &key))
        return FALSE;

      if (!ensure ("key", key, dconf_is_key, error))
        return FALSE;

      value = dconf_client_read (client, key);

      if (value == NULL)
        return TRUE;

      printed = g_variant_print (value, TRUE);
      g_print ("%s\n", printed);
      g_variant_unref (value);
      g_free (printed);

      return TRUE;
    }

  else if (g_strcmp0 (cmd, "write") == 0)
    {
      const gchar *key, *strval;
      GVariant *value;

      if (!grab_args (argc, argv, "key and value", error, 2, &key, &strval))
        return FALSE;

      if (!ensure ("key", key, dconf_is_key, error))
        return FALSE;

      value = g_variant_parse (NULL, strval, NULL, NULL, error);

      if (value == NULL)
        return FALSE;

      return dconf_client_write (client, key, value, NULL, NULL, error);
    }

  else if (g_strcmp0 (cmd, "write-many") == 0)
    {
      g_assert_not_reached ();
    }

  else if (g_strcmp0 (cmd, "list") == 0)
    {
      const gchar *dir;
      gchar **list;

      if (!grab_args (argc, argv, "dir", error, 1, &dir))
        return FALSE;

      if (!ensure ("dir", dir, dconf_is_dir, error))
        return FALSE;

      list = dconf_client_list (client, dir, NULL);

      while (*list)
        g_print ("%s\n", *list++);

      return TRUE;
    }

  else if (g_strcmp0 (cmd, "lock") == 0)
    {
      const gchar *path;

      if (!grab_args (argc, argv, "path", error, 1, &path))
        return FALSE;

      if (!ensure ("path", path, dconf_is_path, error))
        return FALSE;

      return dconf_client_set_locked (client, path, TRUE, NULL, NULL);
    }

  else if (g_strcmp0 (cmd, "unlock") == 0)
    {
      const gchar *path;

      if (!grab_args (argc, argv, "path", error, 1, &path))
        return FALSE;

      if (!ensure ("path", path, dconf_is_path, error))
        return FALSE;

      return dconf_client_set_locked (client, path, FALSE, NULL, NULL);
    }

  else if (g_strcmp0 (cmd, "is-writable") == 0)
    {
      const gchar *path;

      if (!grab_args (argc, argv, "path", error, 1, &path))
        return FALSE;

      if (!ensure ("path", path, dconf_is_path, error))
        return FALSE;

      return dconf_client_is_writable (client, path, error);
    }

  else
    {
      g_set_error (error, 0, 0, "unknown command");
      return FALSE;
    }
}

int
main (int argc, char **argv)
{
  GError *error = NULL;
  DConfClient *client;

  g_type_init ();
  g_set_prgname (shift (&argc, &argv));

  client = dconf_client_new (NULL, FALSE, NULL, NULL, NULL);

  if (!do_sync_command (client, argc, argv, &error))
    g_error ("%s\n", error->message);

  return 0;
}