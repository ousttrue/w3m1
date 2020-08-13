#include "fm.h"
#include "html/image.h"
#include "indep.h"
#include "gc_helper.h"
#include "file.h"
#include "frontend/display.h"
#include "frontend/terms.h"

#include "frontend/buffer.h"
#include "transport/local.h"
#include "html/anchor.h"
#include "transport/loader.h"
#include "rc.h"
#include "myctype.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>

#ifdef HAVE_WAITPID
#include <sys/wait.h>
#endif

#ifdef USE_IMAGE

static int image_index = 0;

/* display image */

struct TerminalImage
{
    ImageCache *cache;
    short x;
    short y;
    short sx;
    short sy;
    short width;
    short height;
};

static TerminalImage *terminal_image = NULL;
static int n_terminal_image = 0;
static int max_terminal_image = 0;
static FILE *Imgdisplay_rf = NULL, *Imgdisplay_wf = NULL;
static pid_t Imgdisplay_pid = 0;
static int openImgdisplay();
static void closeImgdisplay();
int getCharSize();

void initImage()
{
    if (w3mApp::Instance().activeImage)
        return;
    if (getCharSize())
        w3mApp::Instance().activeImage = TRUE;
}

int getCharSize()
{
    FILE *f;
    Str tmp;
    int w = 0, h = 0;

    set_environ("W3M_TTY", ttyname_tty());
    tmp = Strnew();
    if (!strchr(Imgdisplay, '/'))
        Strcat_m_charp(tmp, w3m_auxbin_dir(), "/", NULL);
    Strcat_m_charp(tmp, Imgdisplay, " -test 2>/dev/null", NULL);
    f = popen(tmp->ptr, "r");
    if (!f)
        return FALSE;
    while (fscanf(f, "%d %d", &w, &h) < 0)
    {
        if (feof(f))
            break;
    }
    pclose(f);

    if (!(w > 0 && h > 0))
        return FALSE;
    if (!w3mApp::Instance().set_pixel_per_char)
        w3mApp::Instance().pixel_per_char = (int)(1.0 * w / COLS + 0.5);
    if (!w3mApp::Instance().set_pixel_per_line)
        w3mApp::Instance().pixel_per_line = (int)(1.0 * h / LINES + 0.5);
    return TRUE;
}

void termImage()
{
    if (!w3mApp::Instance().activeImage)
        return;
    clearImage();
    if (Imgdisplay_wf)
    {
        fputs("2;\n", Imgdisplay_wf); /* ClearImage() */
        fflush(Imgdisplay_wf);
    }
    closeImgdisplay();
}

static int
openImgdisplay()
{
    Imgdisplay_pid = open_pipe_rw(&Imgdisplay_rf, &Imgdisplay_wf);
    if (Imgdisplay_pid < 0)
        goto err0;
    if (Imgdisplay_pid == 0)
    {
        /* child */
        char *cmd;
        setup_child(FALSE, 2, -1);
        if (!strchr(Imgdisplay, '/'))
            cmd = Strnew_m_charp(w3m_auxbin_dir(), "/", Imgdisplay, NULL)->ptr;
        else
            cmd = Imgdisplay;
        myExec(cmd);
        /* XXX: ifdef __EMX__, use start /f ? */
    }
    w3mApp::Instance().activeImage = TRUE;
    return TRUE;
err0:
    Imgdisplay_pid = 0;
    w3mApp::Instance().activeImage = FALSE;
    return FALSE;
}

static void
closeImgdisplay()
{
    if (Imgdisplay_wf)
        fclose(Imgdisplay_wf);
    if (Imgdisplay_rf)
    {
        /* sync with the child */
        getc(Imgdisplay_rf); /* EOF expected */
        fclose(Imgdisplay_rf);
    }
    if (Imgdisplay_pid)
        kill(Imgdisplay_pid, SIGKILL);
    Imgdisplay_rf = NULL;
    Imgdisplay_wf = NULL;
    Imgdisplay_pid = 0;
}

void addImage(ImageCache *cache, int x, int y, int sx, int sy, int w, int h)
{
    TerminalImage *i;

    if (!w3mApp::Instance().activeImage)
        return;
    if (n_terminal_image >= max_terminal_image)
    {
        max_terminal_image = max_terminal_image ? (2 * max_terminal_image) : 8;
        terminal_image = New_Reuse(TerminalImage, terminal_image,
                                   max_terminal_image);
    }
    i = &terminal_image[n_terminal_image];
    i->cache = cache;
    i->x = x;
    i->y = y;
    i->sx = sx;
    i->sy = sy;
    i->width = w;
    i->height = h;
    n_terminal_image++;
}

static void
syncImage(void)
{
    fputs("3;\n", Imgdisplay_wf); /* XSync() */
    fputs("4;\n", Imgdisplay_wf); /* put '\n' */
    while (fflush(Imgdisplay_wf) != 0)
    {
        if (ferror(Imgdisplay_wf))
            goto err;
    }
    if (!fgetc(Imgdisplay_rf))
        goto err;
    return;
err:
    closeImgdisplay();
    image_index += MAX_IMAGE;
    n_terminal_image = 0;
}

void drawImage()
{
    static char buf[64];
    int j, draw = FALSE;
    TerminalImage *i;

    if (!w3mApp::Instance().activeImage)
        return;
    if (!n_terminal_image)
        return;
    for (j = 0; j < n_terminal_image; j++)
    {
        i = &terminal_image[j];
        if (!(i->cache->loaded & IMG_FLAG_LOADED &&
              i->width > 0 && i->height > 0))
            continue;
        if (!(Imgdisplay_rf && Imgdisplay_wf))
        {
            if (!openImgdisplay())
                return;
        }
        if (i->cache->index > 0)
        {
            i->cache->index *= -1;
            fputs("0;", Imgdisplay_wf); /* DrawImage() */
        }
        else
            fputs("1;", Imgdisplay_wf); /* DrawImage(redraw) */
        sprintf(buf, "%d;%d;%d;%d;%d;%d;%d;%d;%d;",
                (-i->cache->index - 1) % MAX_IMAGE + 1, i->x, i->y,
                (i->cache->width > 0) ? i->cache->width : 0,
                (i->cache->height > 0) ? i->cache->height : 0,
                i->sx, i->sy, i->width, i->height);
        fputs(buf, Imgdisplay_wf);
        fputs(i->cache->file, Imgdisplay_wf);
        fputs("\n", Imgdisplay_wf);
        draw = TRUE;
    }
    if (!draw)
        return;
    syncImage();
    touch_cursor();
    refresh();
}

void clearImage()
{
    static char buf[64];
    int j;
    TerminalImage *i;

    if (!w3mApp::Instance().activeImage)
        return;
    if (!n_terminal_image)
        return;
    if (!Imgdisplay_wf)
    {
        n_terminal_image = 0;
        return;
    }
    for (j = 0; j < n_terminal_image; j++)
    {
        i = &terminal_image[j];
        if (!(i->cache->loaded & IMG_FLAG_LOADED &&
              i->width > 0 && i->height > 0))
            continue;
        sprintf(buf, "6;%d;%d;%d;%d\n", i->x, i->y, i->width, i->height);
        fputs(buf, Imgdisplay_wf);
    }
    syncImage();
    n_terminal_image = 0;
}

/* load image */

#ifndef MAX_LOAD_IMAGE
#define MAX_LOAD_IMAGE 8
#endif
static int n_load_image = 0;
static Hash_sv *image_hash = NULL;
static Hash_sv *image_file = NULL;
static GeneralList *image_list = NULL;
static ImageCache **image_cache = NULL;
static BufferPtr image_buffer = NULL;

void deleteImage(Buffer* buf)
{
    Anchor *a;
    int i;

    if (!buf)
        return;
    auto &al = buf->img;
    if (!al)
        return;
        
    for (auto &a: al.anchors)
    {
        if (a.image && a.image->cache &&
            a.image->cache->loaded != IMG_FLAG_UNLOADED &&
            !(a.image->cache->loaded & IMG_FLAG_DONT_REMOVE) &&
            a.image->cache->index < 0)
            unlink(a.image->cache->file);
    }
    loadImage(NULL, IMG_FLAG_STOP);
}

void getAllImage(BufferPtr buf)
{
    Anchor *a;
    URL *current;
    int i;

    image_buffer = buf;
    if (!buf)
        return;
    buf->image_loaded = TRUE;
    auto &al = buf->img;
    if (!al)
        return;
    current = buf->BaseURL();
    for (auto &a: al.anchors)
    {
        if (a.image)
        {
            a.image->cache = getImage(a.image, current, buf->image_flag);
            if (a.image->cache &&
                a.image->cache->loaded == IMG_FLAG_UNLOADED)
                buf->image_loaded = FALSE;
        }
    }
}

void showImageProgress(BufferPtr buf)
{
    Anchor *a;
    int i, l, n;

    if (!buf)
        return;
    auto &al = buf->img;
    if (!al)
        return;
    for (auto &a: al.anchors)
    {
        if (a.image && a.hseq >= 0)
        {
            n++;
            if (a.image->cache && a.image->cache->loaded & IMG_FLAG_LOADED)
                l++;
        }
    }
    if (n)
    {
        message(Sprintf("%d/%d images loaded", l, n)->ptr, buf->rect);
        refresh();
    }
}

void loadImage(BufferPtr buf, int flag)
{
    ImageCache *cache;
    struct stat st;
    int i, draw = FALSE;
    /* int wait_st; */

    if (maxLoadImage > MAX_LOAD_IMAGE)
        maxLoadImage = MAX_LOAD_IMAGE;
    else if (maxLoadImage < 1)
        maxLoadImage = 1;
    if (n_load_image == 0)
        n_load_image = maxLoadImage;
    if (!image_cache)
    {
        image_cache = New_N(ImageCache *, MAX_LOAD_IMAGE);
        bzero(image_cache, sizeof(ImageCache *) * MAX_LOAD_IMAGE);
    }
    for (i = 0; i < n_load_image; i++)
    {
        cache = image_cache[i];
        if (!cache)
            continue;
        if (lstat(cache->touch, &st))
            continue;
        if (cache->pid)
        {
            kill(cache->pid, SIGKILL);
            /*
	     * #ifdef HAVE_WAITPID
	     * waitpid(cache->pid, &wait_st, 0);
	     * #else
	     * wait(&wait_st);
	     * #endif
	     */
            cache->pid = 0;
        }
        if (!stat(cache->file, &st))
        {
            cache->loaded = IMG_FLAG_LOADED;
            if (getImageSize(cache))
            {
                if (image_buffer)
                    image_buffer->need_reshape = TRUE;
            }
            draw = TRUE;
        }
        else
            cache->loaded = IMG_FLAG_ERROR;
        unlink(cache->touch);
        image_cache[i] = NULL;
    }

    for (i = (buf != image_buffer) ? 0 : maxLoadImage; i < n_load_image; i++)
    {
        cache = image_cache[i];
        if (!cache)
            continue;
        if (cache->pid)
        {
            kill(cache->pid, SIGKILL);
            /*
	     * #ifdef HAVE_WAITPID
	     * waitpid(cache->pid, &wait_st, 0);
	     * #else
	     * wait(&wait_st);
	     * #endif
	     */
            cache->pid = 0;
        }
        unlink(cache->touch);
        image_cache[i] = NULL;
    }

    if (flag == IMG_FLAG_STOP)
    {
        image_list = NULL;
        image_file = NULL;
        n_load_image = maxLoadImage;
        image_buffer = NULL;
        return;
    }

    if (draw && image_buffer)
    {
        drawImage();
        showImageProgress(image_buffer);
    }

    image_buffer = buf;

    if (!image_list)
        return;
    for (i = 0; i < n_load_image; i++)
    {
        if (image_cache[i])
            continue;
        while (1)
        {
            cache = (ImageCache *)popValue(image_list);
            if (!cache)
            {
                for (i = 0; i < n_load_image; i++)
                {
                    if (image_cache[i])
                        return;
                }
                image_list = NULL;
                image_file = NULL;
                if (image_buffer)
                    displayBuffer(image_buffer, B_NORMAL);
                return;
            }
            if (cache->loaded == IMG_FLAG_UNLOADED)
                break;
        }
        image_cache[i] = cache;

        flush_tty();
        if ((cache->pid = fork()) == 0)
        {
            BufferPtr b;
            /*
	     * setup_child(TRUE, 0, -1);
	     */
            setup_child(FALSE, 0, -1);
            image_source = cache->file;
            b = loadGeneralFile(URL::Parse(cache->url), cache->current, HttpReferrerPolicy::StrictOriginWhenCrossOrigin, RG_NONE, NULL);
            if (!b || b->real_type.empty() || !b->real_type.starts_with("image/"))
                unlink(cache->file);
#if defined(HAVE_SYMLINK) && defined(HAVE_LSTAT)
            symlink(cache->file, cache->touch);
#else
            {
                FILE *f = fopen(cache->touch, "w");
                if (f)
                    fclose(f);
            }
#endif
            exit(0);
        }
        else if (cache->pid < 0)
        {
            cache->pid = 0;
            return;
        }
    }
}

ImageCache *
getImage(Image *image, URL *current, int flag)
{
    Str key = NULL;
    ImageCache *cache;

    if (!w3mApp::Instance().activeImage)
        return NULL;
    if (!image_hash)
        image_hash = newHash_sv(100);
    if (image->cache)
        cache = image->cache;
    else
    {
        key = Sprintf("%d;%d;%s", image->width, image->height, image->url);
        cache = (ImageCache *)getHash_sv(image_hash, key->ptr, NULL);
    }
    if (cache && cache->index && abs(cache->index) <= image_index - MAX_IMAGE)
    {
        struct stat st;
        if (stat(cache->file, &st))
            cache->loaded = IMG_FLAG_UNLOADED;
        cache->index = 0;
    }

    if (!cache)
    {
        if (flag == IMG_FLAG_SKIP)
            return NULL;

        cache = New(ImageCache);
        cache->url = image->url;
        cache->current = current;
        cache->file = tmpfname(TMPF_DFL, image->ext)->ptr;
        cache->touch = tmpfname(TMPF_DFL, NULL)->ptr;
        cache->pid = 0;
        cache->index = 0;
        cache->loaded = IMG_FLAG_UNLOADED;
        cache->width = image->width;
        cache->height = image->height;
        putHash_sv(image_hash, key->ptr, (void *)cache);
    }
    if (flag != IMG_FLAG_SKIP)
    {
        if (cache->loaded == IMG_FLAG_UNLOADED)
        {
            if (!image_file)
                image_file = newHash_sv(100);
            if (!getHash_sv(image_file, cache->file, NULL))
            {
                putHash_sv(image_file, cache->file, (void *)cache);
                if (!image_list)
                    image_list = newGeneralList();
                pushValue(image_list, (void *)cache);
            }
        }
        if (!cache->index)
            cache->index = ++image_index;
    }
    if (cache->loaded & IMG_FLAG_LOADED)
        getImageSize(cache);
    return cache;
}

int getImageSize(ImageCache *cache)
{
    Str tmp;
    FILE *f;
    int w = 0, h = 0;

    if (!w3mApp::Instance().activeImage)
        return FALSE;
    if (!cache || !(cache->loaded & IMG_FLAG_LOADED) ||
        (cache->width > 0 && cache->height > 0))
        return FALSE;
    tmp = Strnew();
    if (!strchr(Imgdisplay, '/'))
        Strcat_m_charp(tmp, w3m_auxbin_dir(), "/", NULL);
    Strcat_m_charp(tmp, Imgdisplay, " -size ", shell_quote(cache->file), NULL);
    f = popen(tmp->ptr, "r");
    if (!f)
        return FALSE;
    while (fscanf(f, "%d %d", &w, &h) < 0)
    {
        if (feof(f))
            break;
    }
    pclose(f);

    if (!(w > 0 && h > 0))
        return FALSE;
    w = (int)(w * w3mApp::Instance().image_scale / 100 + 0.5);
    if (w == 0)
        w = 1;
    h = (int)(h * w3mApp::Instance().image_scale / 100 + 0.5);
    if (h == 0)
        h = 1;
    if (cache->width < 0 && cache->height < 0)
    {
        cache->width = (w > MAX_IMAGE_SIZE) ? MAX_IMAGE_SIZE : w;
        cache->height = (h > MAX_IMAGE_SIZE) ? MAX_IMAGE_SIZE : h;
    }
    else if (cache->width < 0)
    {
        int tmp = (int)((double)cache->height * w / h + 0.5);
        cache->width = (tmp > MAX_IMAGE_SIZE) ? MAX_IMAGE_SIZE : tmp;
    }
    else if (cache->height < 0)
    {
        int tmp = (int)((double)cache->width * h / w + 0.5);
        cache->height = (tmp > MAX_IMAGE_SIZE) ? MAX_IMAGE_SIZE : tmp;
    }
    if (cache->width == 0)
        cache->width = 1;
    if (cache->height == 0)
        cache->height = 1;
    tmp = Sprintf("%d;%d;%s", cache->width, cache->height, cache->url);
    putHash_sv(image_hash, tmp->ptr, (void *)cache);
    return TRUE;
}
#endif

#ifdef USE_IMAGE
#ifdef USE_XFACE
char *
xface2xpm(char *xface)
{
    Image image;
    ImageCache *cache;
    FILE *f;
    struct stat st;

    SKIP_BLANKS(&xface);
    image.url = xface;
    image.ext = ".xpm";
    image.width = 48;
    image.height = 48;
    image.cache = NULL;
    cache = getImage(&image, NULL, IMG_FLAG_AUTO);
    if (cache->loaded & IMG_FLAG_LOADED && !stat(cache->file, &st))
        return cache->file;
    cache->loaded = IMG_FLAG_ERROR;

    f = popen(Sprintf("%s > %s", shell_quote(auxbinFile(XFACE2XPM)),
                      shell_quote(cache->file))
                  ->ptr,
              "w");
    if (!f)
        return NULL;
    fputs(xface, f);
    pclose(f);
    if (stat(cache->file, &st) || !st.st_size)
        return NULL;
    cache->loaded = IMG_FLAG_LOADED | IMG_FLAG_DONT_REMOVE;
    cache->index = 0;
    return cache->file;
}
#endif
#endif
