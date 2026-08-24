#include "lv_port_fs.h"
#include <SD.h>
#include <string.h>

static void *fs_open(lv_fs_drv_t *drv, const char *path, lv_fs_mode_t mode)
{
    (void)drv;
    const char *p = path;
    if (p[0] == '0' && p[1] == ':') p += 2;
    if (p[0] == '/') p++;

    const char *fname = p;

    /* "0:" mount point: open root directory */
    if (fname[0] == '\0') {
        File *f = new File(SD.open("/"));
        return f;
    }

    char full[128];
    snprintf(full, sizeof(full), "/%s", fname);
    File *f = new File(SD.open(full,
        (mode & LV_FS_MODE_WR) ? FILE_WRITE : FILE_READ));
    if (!*f) { delete f; return NULL; }
    return f;
}

static lv_fs_res_t fs_close(lv_fs_drv_t *drv, void *file_p)
{
    (void)drv;
    File *f = (File *)file_p;
    f->close();
    delete f;
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_read(lv_fs_drv_t *drv, void *file_p, void *buf,
                           uint32_t btr, uint32_t *br)
{
    (void)drv;
    File *f = (File *)file_p;
    *br = f->read((uint8_t *)buf, btr);
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_write(lv_fs_drv_t *drv, void *file_p, const void *buf,
                            uint32_t btw, uint32_t *bw)
{
    (void)drv;
    File *f = (File *)file_p;
    *bw = f->write((const uint8_t *)buf, btw);
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_seek(lv_fs_drv_t *drv, void *file_p, uint32_t pos,
                           lv_fs_whence_t whence)
{
    (void)drv;
    File *f = (File *)file_p;
    if (whence == LV_FS_SEEK_SET)
        f->seek(pos);
    else if (whence == LV_FS_SEEK_CUR)
        f->seek(f->position() + pos);
    else if (whence == LV_FS_SEEK_END)
        f->seek(f->size() + pos);
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_tell(lv_fs_drv_t *drv, void *file_p, uint32_t *pos_p)
{
    (void)drv;
    File *f = (File *)file_p;
    *pos_p = f->position();
    return LV_FS_RES_OK;
}

static void *fs_dir_open(lv_fs_drv_t *drv, const char *path)
{
    (void)drv;
    File *d = new File(SD.open("/sd"));
    if (!*d || !d->isDirectory()) { delete d; return NULL; }
    return d;
}

static lv_fs_res_t fs_dir_read(lv_fs_drv_t *drv, void *dir_p, char *fn)
{
    (void)drv;
    File *d = (File *)dir_p;
    File entry = d->openNextFile();
    if (!entry) return LV_FS_RES_FS_ERR;
    strncpy(fn, entry.name(), 255);
    fn[255] = '\0';
    entry.close();
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_dir_close(lv_fs_drv_t *drv, void *dir_p)
{
    (void)drv;
    File *d = (File *)dir_p;
    d->close();
    delete d;
    return LV_FS_RES_OK;
}

void lv_port_fs_init(void)
{
    lv_fs_drv_t fs_drv;
    lv_fs_drv_init(&fs_drv);
    fs_drv.letter      = '0';
    fs_drv.open_cb     = fs_open;
    fs_drv.close_cb    = fs_close;
    fs_drv.read_cb     = fs_read;
    fs_drv.write_cb    = fs_write;
    fs_drv.seek_cb     = fs_seek;
    fs_drv.tell_cb     = fs_tell;
    fs_drv.dir_open_cb = fs_dir_open;
    fs_drv.dir_read_cb = fs_dir_read;
    fs_drv.dir_close_cb = fs_dir_close;
    fs_drv.cache_size   = 4096;
    lv_fs_drv_register(&fs_drv);
}
