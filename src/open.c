#include <errno.h>
#include <glib.h>

#include "open.h"
#include "log.h"
#include "glob.h"
#include "file.h"
#include "tmpstr.h"

extern GHashTable *hash;

int
dfs_open(const char *path,
         struct fuse_file_info *info)
{
        pentry_t *pe = NULL;
        char *key = NULL;
        char *file = NULL;
        int fd = -1;
        int ret = -1;

        LOG("path=%s", path);
        PRINT_FLAGS(path, info);

        pe = g_hash_table_lookup(hash, path);
        if (! pe) {
                LOG("'%s': entry not found in hashtable", path);
                pe = pentry_new();
                if (! pe) {
                        ret = -1;
                        goto err;
                }
                pentry_set_fd(pe, -1);
                key = strdup(path);
                if (! key) {
                        LOG("strdup(%s): %s", path, strerror(errno));
                        pentry_free(pe);
                        goto err;
                }
                LOG("adding file '%s' to the hashtable", path);
                g_hash_table_insert(hash, key, pe);
        } else {
                fd = pentry_get_fd(pe);
                LOG("'%s': found in the hashtable, fd=%d", path, fd);
        }

        pentry_inc_refcount(pe);

        if (O_RDONLY != (info->flags & O_ACCMODE)) {
                if (pentry_lock(pe)) {
                        ret = -1;
                        goto err;
                }
        }

        info->fh = (uint64_t)pe;
        file = build_cache_tree(path);

        if (! file) {
                LOG("build_cache_tree(%s) was unable to build a path", path);
                ret = -1;
                goto err;
        }

        /* open in order to write on remote storage */
        if (O_WRONLY == (info->flags & O_ACCMODE)) {
                LOG("opening cache file '%s'", file);
                fd = open(file, O_RDWR|O_CREAT|O_TRUNC, 0644);
                if (-1 == fd) {
                        LOG("%s: %s", file, strerror(errno));
                        ret = -1;
                        goto err;
                }
                pentry_set_fd(pe, fd);
                pentry_set_flag(pe, FLAG_DIRTY);
        } else {
                /* get the fd of the file we want to read */
                fd = pentry_get_fd(pe);

                /* negative fd? then we don't have any cache file, get it! */
                if (fd < 0) {
                        fd = dfs_get_local_copy(pe, path);
                        if (-1 == fd) {
                                ret = -1;
                                goto err;
                        }
                        pentry_set_fd(pe, fd);
                }
        }

        ret = 0;
  err:
        LOG("@pentry=%p, fd=%d, flags=0x%X, ret=%d", pe, fd, info->flags, ret);
        return ret;
}
