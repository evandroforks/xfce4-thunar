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

#ifndef __THUNAR_VFS_SYSDEP_H__
#define __THUNAR_VFS_SYSDEP_H__

#include <glib.h>

G_BEGIN_DECLS;

/* forward declarations */
struct dirent;

gboolean _thunar_vfs_sysdep_readdir    (gpointer        dirp,
                                        struct dirent  *entry,
                                        struct dirent **result,
                                        GError        **error);

gboolean _thunar_vfs_sysdep_parse_exec (const gchar    *exec,
                                        GList          *uris,
                                        const gchar    *icon,
                                        const gchar    *name,
                                        const gchar    *path,
                                        gboolean        terminal,
                                        gint           *argc,
                                        gchar        ***argv,
                                        GError        **error)

G_END_DECLS;

#endif /* !__THUNAR_VFS_SYSDEP_H__ */
