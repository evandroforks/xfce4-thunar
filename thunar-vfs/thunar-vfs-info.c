/* $Id$ */
/*-
 * Copyright (c) 2005 Benedikt Meurer <benny@xfce.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <thunar-vfs/thunar-vfs-info.h>
#include <thunar-vfs/thunar-vfs-mime-database.h>
#include <thunar-vfs/thunar-vfs-sysdep.h>
#include <thunar-vfs/thunar-vfs-alias.h>

/* Use g_access() if possible */
#if GLIB_CHECK_VERSION(2,8,0)
#include <glib/gstdio.h>
#else
#define g_access(path, mode) (access ((path), (mode)))
#endif

/* Use g_rename() if possible */
#if GLIB_CHECK_VERSION(2,6,0)
#include <glib/gstdio.h>
#else
#define g_rename(from, to) (rename ((from), (to)))
#endif



static ThunarVfsMimeDatabase *mime_database = NULL;



GType
thunar_vfs_info_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      type = g_boxed_type_register_static ("ThunarVfsInfo",
                                           (GBoxedCopyFunc) thunar_vfs_info_ref,
                                           (GBoxedFreeFunc) thunar_vfs_info_unref);
    }

  return type;
}



/**
 * thunar_vfs_info_new_for_uri:
 * @uri   : the #ThunarVfsURI of the file whose info should be queried.
 * @error : return location for errors or %NULL.
 *
 * Queries the #ThunarVfsInfo for the given @uri. Returns the
 * #ThunarVfsInfo if the operation is successfull, else %NULL.
 * In the latter case, @error will be set to point to a #GError
 * describing the cause of the failure.
 *
 * Return value: the #ThunarVfsInfo for @uri or %NULL.
 **/
ThunarVfsInfo*
thunar_vfs_info_new_for_uri (ThunarVfsURI *uri,
                             GError      **error)
{
  ThunarVfsInfo *info;
  const gchar   *path;
  const gchar   *str;
  struct stat    lsb;
  struct stat    sb;
  XfceRc        *rc;
  GList         *mime_infos;
  GList         *lp;

  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  path = thunar_vfs_uri_get_path (uri);

  if (G_UNLIKELY (lstat (path, &lsb) < 0))
    {
      g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno),
                   "Failed to stat file `%s': %s", path, g_strerror (errno));
      return NULL;
    }

  info = g_new (ThunarVfsInfo, 1);
  info->uri = thunar_vfs_uri_ref (uri);
  info->ref_count = 1;
  info->display_name = thunar_vfs_uri_get_display_name (uri);
  info->hints = NULL;

  /* determine the POSIX file attributes */
  if (G_LIKELY (!S_ISLNK (lsb.st_mode)))
    {
      info->type = (lsb.st_mode & S_IFMT) >> 12;
      info->mode = lsb.st_mode & 07777;
      info->flags = THUNAR_VFS_FILE_FLAGS_NONE;
      info->uid = lsb.st_uid;
      info->gid = lsb.st_gid;
      info->size = lsb.st_size;
      info->atime = lsb.st_atime;
      info->ctime = lsb.st_ctime;
      info->mtime = lsb.st_mtime;
      info->inode = lsb.st_ino;
      info->device = lsb.st_dev;
    }
  else
    {
      if (stat (path, &sb) == 0)
        {
          info->type = (sb.st_mode & S_IFMT) >> 12;
          info->mode = sb.st_mode & 07777;
          info->flags = THUNAR_VFS_FILE_FLAGS_SYMLINK;
          info->uid = sb.st_uid;
          info->gid = sb.st_gid;
          info->size = sb.st_size;
          info->atime = sb.st_atime;
          info->ctime = sb.st_ctime;
          info->mtime = sb.st_mtime;
          info->inode = sb.st_ino;
          info->device = sb.st_dev;
        }
      else
        {
          info->type = THUNAR_VFS_FILE_TYPE_SYMLINK;
          info->mode = lsb.st_mode & 07777;
          info->flags = THUNAR_VFS_FILE_FLAGS_SYMLINK;
          info->uid = lsb.st_uid;
          info->gid = lsb.st_gid;
          info->size = lsb.st_size;
          info->atime = lsb.st_atime;
          info->ctime = lsb.st_ctime;
          info->mtime = lsb.st_mtime;
          info->inode = lsb.st_ino;
          info->device = lsb.st_dev;
        }
    }

  /* determine the file's mime type */
  switch (info->type)
    {
    case THUNAR_VFS_FILE_TYPE_SOCKET:
      info->mime_info = thunar_vfs_mime_database_get_info (mime_database, "inode/socket");
      break;

    case THUNAR_VFS_FILE_TYPE_SYMLINK:
      info->mime_info = thunar_vfs_mime_database_get_info (mime_database, "inode/symlink");
      break;

    case THUNAR_VFS_FILE_TYPE_BLOCKDEV:
      info->mime_info = thunar_vfs_mime_database_get_info (mime_database, "inode/blockdevice");
      break;

    case THUNAR_VFS_FILE_TYPE_DIRECTORY:
      info->mime_info = thunar_vfs_mime_database_get_info (mime_database, "inode/directory");
      break;

    case THUNAR_VFS_FILE_TYPE_CHARDEV:
      info->mime_info = thunar_vfs_mime_database_get_info (mime_database, "inode/chardevice");
      break;

    case THUNAR_VFS_FILE_TYPE_FIFO:
      info->mime_info = thunar_vfs_mime_database_get_info (mime_database, "inode/fifo");
      break;

    case THUNAR_VFS_FILE_TYPE_REGULAR:
      /* determine the MIME-type for the regular file */
      info->mime_info = thunar_vfs_mime_database_get_info_for_file (mime_database, path, info->display_name);

      /* check if the file is executable (for security reasons
       * we only allow execution of well known file types).
       */
      if ((info->mode & 0444) != 0 && g_access (path, X_OK) == 0)
        {
          mime_infos = thunar_vfs_mime_database_get_infos_for_info (mime_database, info->mime_info);
          for (lp = mime_infos; lp != NULL; lp = lp->next)
            {
              if (strcmp (thunar_vfs_mime_info_get_name (lp->data), "application/x-executable") == 0
                  || strcmp (thunar_vfs_mime_info_get_name (lp->data), "application/x-shellscript") == 0)
                {
                  info->flags |= THUNAR_VFS_FILE_FLAGS_EXECUTABLE;
                  break;
                }
            }
          thunar_vfs_mime_info_list_free (mime_infos);
        }

      /* check whether we have a .desktop file here */
      if (G_UNLIKELY (strcmp (thunar_vfs_mime_info_get_name (info->mime_info), "application/x-desktop") == 0))
        {
          /* try to query the hints from the .desktop file */
          rc = xfce_rc_simple_open (path, TRUE);
          if (G_LIKELY (rc != NULL))
            {
              /* we're only interested in the desktop data */
              xfce_rc_set_group (rc, "Desktop Entry");

              /* allocate the hints for the VFS info */
              info->hints = g_new0 (gchar *, THUNAR_VFS_FILE_N_HINTS);

              /* check if we have a valid icon info */
              str = xfce_rc_read_entry_untranslated (rc, "Icon", NULL);
              if (G_LIKELY (str != NULL))
                info->hints[THUNAR_VFS_FILE_HINT_ICON] = g_strdup (str);

              /* check if we have a valid name info */
              str = xfce_rc_read_entry (rc, "Name", NULL);
              if (G_LIKELY (str != NULL))
                info->hints[THUNAR_VFS_FILE_HINT_NAME] = g_strdup (str);

              /* check if the desktop file refers to an application
               * and has a non-NULL Exec field set.
               */
              str = xfce_rc_read_entry_untranslated (rc, "Type", "Application");
              if (G_LIKELY (exo_str_is_equal (str, "Application"))
                  && xfce_rc_read_entry (rc, "Exec", NULL) != NULL)
                {
                  info->flags |= THUNAR_VFS_FILE_FLAGS_EXECUTABLE;
                }

              /* close the file */
              xfce_rc_close (rc);
            }
        }
      break;

    default:
      g_assert_not_reached ();
      break;
    }

  return info;
}



/**
 * thunar_vfs_info_ref:
 * @info : a #ThunarVfsInfo.
 *
 * Increments the reference count on @info by 1 and
 * returns a pointer to @info.
 *
 * Return value: a pointer to @info.
 **/
ThunarVfsInfo*
thunar_vfs_info_ref (ThunarVfsInfo *info)
{
  _thunar_vfs_sysdep_inc (&info->ref_count);
  return info;
}



/**
 * thunar_vfs_info_unref:
 * @info : a #ThunarVfsInfo.
 *
 * Decrements the reference count on @info by 1 and if
 * the reference count drops to zero as a result of this
 * operation, the @info will be freed completely.
 **/
void
thunar_vfs_info_unref (ThunarVfsInfo *info)
{
  guint n;

  if (_thunar_vfs_sysdep_dec (&info->ref_count))
    {
      /* drop the public info part */
      thunar_vfs_mime_info_unref (info->mime_info);
      thunar_vfs_uri_unref (info->uri);
      g_free (info->display_name);

      /* free the hints (if any) */
      if (G_UNLIKELY (info->hints != NULL))
        {
          for (n = 0; n < THUNAR_VFS_FILE_N_HINTS; ++n)
            g_free (info->hints[n]);
          g_free (info->hints);
        }

#ifndef G_DISABLE_CHECKS
      memset (info, 0xaa, sizeof (*info));
#endif

      g_free (info);
    }
}



/**
 * thunar_vfs_info_execute:
 * @info   : a #ThunarVfsInfo.
 * @screen : a #GdkScreen or %NULL to use the default #GdkScreen.
 * @uris   : the list of #ThunarVfsURI<!---->s to give as parameters
 *           to the file referred to by @info on execution.
 * @error  : return location for errors or %NULL.
 *
 * Executes the file referred to by @info, given @uris as parameters,
 * on the specified @screen. @info may refer to either a regular,
 * executable file, or a <filename>.desktop</filename> file, whose
 * type is <literal>Application</literal>.
 *
 * Return value: %TRUE on success, else %FALSE.
 **/
gboolean
thunar_vfs_info_execute (const ThunarVfsInfo *info,
                         GdkScreen           *screen,
                         GList               *uris,
                         GError             **error)
{
  const gchar *icon;
  const gchar *name;
  const gchar *path;
  gboolean     terminal;
  gboolean     result = FALSE;
  XfceRc      *rc;
  gchar       *working_directory;
  gchar       *path_escaped;
  gchar      **argv = NULL;
  gchar       *exec;

  g_return_val_if_fail (info != NULL, FALSE);
  g_return_val_if_fail (info->ref_count > 0, FALSE);
  g_return_val_if_fail (screen == NULL || GDK_IS_SCREEN (screen), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  /* fallback to the default screen if none given */
  if (G_UNLIKELY (screen == NULL))
    screen = gdk_screen_get_default ();

  /* determine the path */
  path = thunar_vfs_uri_get_path (info->uri);

  /* check if we have a .desktop file here */
  if (G_UNLIKELY (strcmp (thunar_vfs_mime_info_get_name (info->mime_info), "application/x-desktop") == 0))
    {
      rc = xfce_rc_simple_open (path, TRUE);
      if (G_LIKELY (rc != NULL))
        {
          /* check if we have a valid Exec field */
          exec = (gchar *) xfce_rc_read_entry_untranslated (rc, "Exec", NULL);
          if (G_LIKELY (exec != NULL))
            {
              /* parse the Exec field */
              name = xfce_rc_read_entry (rc, "Name", NULL);
              icon = xfce_rc_read_entry_untranslated (rc, "Icon", NULL);
              terminal = xfce_rc_read_bool_entry (rc, "Terminal", FALSE);
              result = _thunar_vfs_sysdep_parse_exec (exec, uris, icon, name, path,
                                                      terminal, NULL, &argv, error);
            }
          else
            {
              g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL, _("No Exec field specified"));
            }

          /* close the rc file */
          xfce_rc_close (rc);
        }
      else
        {
          g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL, _("Unable to parse file"));
        }
    }
  else
    {
      /* fake the Exec line */
      path_escaped = g_shell_quote (path);
      exec = g_strconcat (path_escaped, " %F", NULL);
      result = _thunar_vfs_sysdep_parse_exec (exec, uris, NULL, NULL, NULL, FALSE, NULL, &argv, error);
      g_free (path_escaped);
      g_free (exec);
    }

  if (G_LIKELY (result))
    {
      /* determine the working directory */
      working_directory = g_path_get_dirname ((uris != NULL) ?  thunar_vfs_uri_get_path (uris->data) : path);

      /* execute the command */
      result = gdk_spawn_on_screen (screen, working_directory, argv,
                                    NULL, G_SPAWN_SEARCH_PATH, NULL,
                                    NULL, NULL, error);

      /* release the working directory */
      g_free (working_directory);
    }

  /* clean up */
  g_strfreev (argv);

  return result;
}



/**
 * thunar_vfs_info_rename:
 * @info  : a #ThunarVfsInfo.
 * @name  : the new file name in UTF-8 encoding.
 * @error : return location for errors or %NULL.
 *
 * Tries to rename the file referred to by @info to the
 * new @name.
 *
 * The rename operation is smart in that it checks the
 * type of @info first, and if @info refers to a
 * <filename>.desktop</filename> file, the file name
 * won't be touched, but instead the <literal>Name</literal>
 * field of the <filename>.desktop</filename> will be
 * changed to @name. Else, if @info refers to a regular
 * file or directory, the file will be given a new
 * name.
 *
 * Return value: %TRUE on success, else %FALSE.
 **/
gboolean
thunar_vfs_info_rename (ThunarVfsInfo *info,
                        const gchar   *name,
                        GError       **error)
{
  const gchar * const *locale;
  const gchar         *src_path;
  GKeyFile            *key_file;
  gsize                data_length;
  gchar               *data;
  gchar               *key;
  gchar               *dir_name;
  gchar               *dst_name;
  gchar               *dst_path;
#if !GLIB_CHECK_VERSION(2,8,0)
  FILE                *fp;
#endif

  g_return_val_if_fail (info != NULL, FALSE);
  g_return_val_if_fail (info->ref_count > 0, FALSE);
  g_return_val_if_fail (g_utf8_validate (name, -1, NULL), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  /* validate the name */
  if (*name == '\0' || strchr (name, '/') != NULL)
    {
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL, _("Invalid file name"));
      return FALSE;
    }

  /* determine the source path */
  src_path = thunar_vfs_uri_get_path (info->uri);

  /* check whether we have a .desktop file here */
  if (G_UNLIKELY (strcmp (thunar_vfs_mime_info_get_name (info->mime_info), "application/x-desktop") == 0))
    {
      /* try to open the .desktop file */
      key_file = g_key_file_new ();
      if (!g_key_file_load_from_file (key_file, src_path, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, error))
        {
          g_key_file_free (key_file);
          return FALSE;
        }

      /* check if the file is valid */
      if (G_UNLIKELY (!g_key_file_has_group (key_file, "Desktop Entry")))
        {
          g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL, _("Invalid desktop file"));
          g_key_file_free (key_file);
          return FALSE;
        }

      /* save the new name (localized if required) */
      for (locale = g_get_language_names (); *locale != NULL; ++locale)
        {
          key = g_strdup_printf ("Name[%s]", *locale);
          if (g_key_file_has_key (key_file, "Desktop Entry", key, NULL))
            {
              g_key_file_set_string (key_file, "Desktop Entry", key, name);
              g_free (key);
              break;
            }
          g_free (key);
        }

      /* fallback to unlocalized name */
      if (G_UNLIKELY (*locale == NULL))
        g_key_file_set_string (key_file, "Desktop Entry", "Name", name);

      /* serialize the key file to a buffer */
      data = g_key_file_to_data (key_file, &data_length, error);
      g_key_file_free (key_file);
      if (G_UNLIKELY (data == NULL))
        return FALSE;

      /* write the data back to the file */
#if GLIB_CHECK_VERSION(2,8,0)
      if (!g_file_set_contents (src_path, data, data_length, error))
        {
          g_free (data);
          return FALSE;
        }
#else
      fp = fopen (src_path, "w");
      if (G_UNLIKELY (fp == NULL))
        {
          g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno), g_strerror (errno));
          g_free (data);
          return FALSE;
        }
      fwrite (data, data_length, 1, fp);
      fclose (fp);
#endif

      /* update the Name hint (if any) */
      if (G_LIKELY (info->hints != NULL))
        {
          g_free (info->hints[THUNAR_VFS_FILE_HINT_NAME]);
          info->hints[THUNAR_VFS_FILE_HINT_NAME] = g_strdup (name);
        }

      /* clean up */
      g_free (data);
    }
  else
    {
      /* convert the destination file to local encoding */
      dst_name = g_filename_from_utf8 (name, -1, NULL, NULL, error);
      if (G_UNLIKELY (dst_name == NULL))
        return FALSE;

      /* determine the destination path */
      dir_name = g_path_get_dirname (src_path);
      dst_path = g_build_filename (dir_name, dst_name, NULL);
      g_free (dst_name);
      g_free (dir_name);

      /* verify that the rename target does not already exist */
      if (G_UNLIKELY (g_file_test (dst_path, G_FILE_TEST_EXISTS)))
        {
          g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_EXIST, g_strerror (EEXIST));
          g_free (dst_path);
          return FALSE;
        }

      /* perform the rename */
      if (G_UNLIKELY (g_rename (src_path, dst_path) < 0))
        {
          g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno), g_strerror (errno));
          g_free (dst_path);
          return FALSE;
        }

      /* update the info's URI */
      thunar_vfs_uri_unref (info->uri);
      info->uri = thunar_vfs_uri_new_for_path (dst_path);

      /* update the info's display name */
      g_free (info->display_name);
      info->display_name = g_strdup (name);

      /* if we have a regular file here, then we'll need to determine
       * the mime type again, as it may be based on the file name
       */
      if (G_LIKELY (info->type == THUNAR_VFS_FILE_TYPE_REGULAR))
        {
          thunar_vfs_mime_info_unref (info->mime_info);
          info->mime_info = thunar_vfs_mime_database_get_info_for_file (mime_database, dst_path, info->display_name);
        }

      /* clean up */
      g_free (dst_path);
    }

  return TRUE;
}



/**
 * thunar_vfs_info_get_hint:
 * @info : a #ThunarVfsInfo.
 * @hint : a #ThunarVfsFileHint.
 *
 * If @info provides the @hint, then the value available for
 * @hint will be returned. Else %NULL will be returned.
 *
 * The returned string - if any - is owned by @info and must
 * not be freed by the caller. You should thereby note that the
 * returned string is no longer valid after @info is finalized.
 *
 * Return value: the string stored for @hint on @info or %NULL.
 **/
const gchar*
thunar_vfs_info_get_hint (const ThunarVfsInfo *info,
                          ThunarVfsFileHint    hint)
{
  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (info->ref_count > 0, NULL);

  if (G_UNLIKELY (hint < THUNAR_VFS_FILE_N_HINTS && info->hints != NULL))
    return info->hints[hint];

  return NULL;
}



/**
 * thunar_vfs_info_matches:
 * @a : a #ThunarVfsInfo.
 * @b : a #ThunarVfsInfo.
 *
 * Checks whether @a and @b refer to the same file
 * and share the same properties.
 *
 * Return value: %TRUE if @a and @b match.
 **/
gboolean
thunar_vfs_info_matches (const ThunarVfsInfo *a,
                         const ThunarVfsInfo *b)
{
  g_return_val_if_fail (a != NULL, FALSE);
  g_return_val_if_fail (b != NULL, FALSE);
  g_return_val_if_fail (a->ref_count > 0, FALSE);
  g_return_val_if_fail (b->ref_count > 0, FALSE);

  return a->type == b->type
      && a->mode == b->mode
      && a->flags == b->flags
      && a->uid == b->uid
      && a->gid == b->gid
      && a->size == b->size
      && a->atime == b->atime
      && a->mtime == b->mtime
      && a->ctime == b->ctime
      && a->inode == b->inode
      && a->device == b->device
      && a->mime_info == b->mime_info
      && thunar_vfs_uri_equal (a->uri, b->uri);
}



/**
 * thunar_vfs_info_list_free:
 * @info_list : a list #ThunarVfsInfo<!---->s.
 *
 * Unrefs all #ThunarVfsInfo<!---->s in @info_list and
 * frees the list itself.
 *
 * This method always returns %NULL for the convenience of
 * being able to do:
 * <informalexample><programlisting>
 * info_list = thunar_vfs_info_list_free (info_list);
 * </programlisting></informalexample>
 *
 * Return value: the empty list (%NULL).
 **/
void
thunar_vfs_info_list_free (GList  *info_list)
{
  g_list_foreach (info_list, (GFunc) thunar_vfs_info_unref, NULL);
  g_list_free (info_list);
}



/**
 * _thunar_vfs_info_init:
 *
 * Initializes the info component of the Thunar-VFS
 * library.
 **/
void
_thunar_vfs_info_init (void)
{
  /* grab a reference on the mime database */
  mime_database = thunar_vfs_mime_database_get_default ();
}



/**
 * _thunar_vfs_info_shutdown:
 *
 * Shuts down the info component of the Thunar-VFS
 * library.
 **/
void
_thunar_vfs_info_shutdown (void)
{
  /* release the reference on the mime database */
  g_object_unref (G_OBJECT (mime_database));
  mime_database = NULL;
}



#define __THUNAR_VFS_INFO_C__
#include <thunar-vfs/thunar-vfs-aliasdef.c>
