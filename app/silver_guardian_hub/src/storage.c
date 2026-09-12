/****************************************************************************
 * Silver Guardian Hub - 本地持久化实现
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <syslog.h>

#include "include/storage.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define LOG_TAG      "storage"
#define STORAGE_DIR  "/data/silver_guardian"

#define KEY_MAXLEN   32
#define PATH_MAXLEN  (sizeof(STORAGE_DIR) + KEY_MAXLEN + 8)

/****************************************************************************
 * Private Data
 ****************************************************************************/

static bool g_available;
static int  g_last_err;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int build_path(char *out, size_t outlen, const char *key,
                      const char *suffix)
{
  if (key == NULL || strlen(key) >= KEY_MAXLEN)
    {
      return -EINVAL;
    }

  return snprintf(out, outlen, "%s/%s%s", STORAGE_DIR, key,
                  suffix ? suffix : "") >= (int)outlen ? -ENAMETOOLONG : OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int storage_init(void)
{
  struct stat st;
  int fd;

  g_available = false;
  g_last_err  = 0;

  if (stat(STORAGE_DIR, &st) != 0)
    {
      if (mkdir(STORAGE_DIR, 0777) != 0 && errno != EEXIST)
        {
          g_last_err = errno;
          syslog(LOG_WARNING,
                 "[%s] 无法创建 %s: %d —— 设置将不会被保存\n",
                 LOG_TAG, STORAGE_DIR, g_last_err);
          return -g_last_err;
        }
    }

  /* 真正写一次再删掉，确认分区可写（只 mkdir 成功不代表能写文件） */

  char probe[PATH_MAXLEN];
  if (build_path(probe, sizeof(probe), ".probe", NULL) != OK)
    {
      g_last_err = ENAMETOOLONG;
      return -g_last_err;
    }

  fd = open(probe, O_WRONLY | O_CREAT | O_TRUNC, 0666);
  if (fd < 0)
    {
      g_last_err = errno;
      syslog(LOG_WARNING, "[%s] %s 不可写: %d —— 设置将不会被保存\n",
             LOG_TAG, STORAGE_DIR, g_last_err);
      return -g_last_err;
    }

  if (write(fd, "ok", 2) != 2)
    {
      g_last_err = errno;
      close(fd);
      unlink(probe);
      syslog(LOG_WARNING, "[%s] 写入测试失败: %d\n", LOG_TAG, g_last_err);
      return -g_last_err;
    }

  close(fd);
  unlink(probe);

  g_available = true;
  syslog(LOG_INFO, "[%s] 持久化就绪: %s\n", LOG_TAG, STORAGE_DIR);
  return OK;
}

bool storage_available(void)
{
  return g_available;
}

const char *storage_dir(void)
{
  return STORAGE_DIR;
}

int storage_save(const char *key, const void *data, size_t len)
{
  char    path[PATH_MAXLEN];
  char    tmp[PATH_MAXLEN];
  ssize_t written;
  int     fd;
  int     ret;

  if (!g_available)
    {
      return -ENODEV;
    }

  if (data == NULL || len == 0)
    {
      return -EINVAL;
    }

  if (build_path(path, sizeof(path), key, ".dat") != OK ||
      build_path(tmp,  sizeof(tmp),  key, ".tmp") != OK)
    {
      return -ENAMETOOLONG;
    }

  /* 先写临时文件再 rename，掉电不会留半个文件 */

  fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0666);
  if (fd < 0)
    {
      ret = -errno;
      syslog(LOG_ERR, "[%s] 打开 %s 失败: %d\n", LOG_TAG, tmp, -ret);
      return ret;
    }

  written = write(fd, data, len);
  if (written != (ssize_t)len)
    {
      ret = (written < 0) ? -errno : -EIO;
      syslog(LOG_ERR, "[%s] 写入 %s 失败: 期望 %u 实写 %d\n",
             LOG_TAG, tmp, (unsigned)len, (int)written);
      close(fd);
      unlink(tmp);
      return ret;
    }

  close(fd);

  if (rename(tmp, path) != 0)
    {
      ret = -errno;
      syslog(LOG_ERR, "[%s] rename %s -> %s 失败: %d\n",
             LOG_TAG, tmp, path, -ret);
      unlink(tmp);
      return ret;
    }

  return OK;
}

int storage_load(const char *key, void *data, size_t maxlen)
{
  char    path[PATH_MAXLEN];
  ssize_t n;
  int     fd;

  if (!g_available)
    {
      return -ENODEV;
    }

  if (data == NULL || maxlen == 0)
    {
      return -EINVAL;
    }

  if (build_path(path, sizeof(path), key, ".dat") != OK)
    {
      return -ENAMETOOLONG;
    }

  fd = open(path, O_RDONLY);
  if (fd < 0)
    {
      return -errno;
    }

  n = read(fd, data, maxlen);
  close(fd);

  if (n < 0)
    {
      return -errno;
    }

  return (int)n;
}

int storage_remove(const char *key)
{
  char path[PATH_MAXLEN];

  if (!g_available)
    {
      return -ENODEV;
    }

  if (build_path(path, sizeof(path), key, ".dat") != OK)
    {
      return -ENAMETOOLONG;
    }

  return (unlink(path) == 0) ? OK : -errno;
}
