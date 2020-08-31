#include <string_view_util.h>
#include <unordered_map>
#include <list>
#include "w3m.h"
#include "indep.h"
#include "file.h"
#include "html/image.h"
#include "frontend/terminal.h"
#include "frontend/screen.h"
#include "frontend/display.h"
#include "frontend/event.h"
#include "stream/local_cgi.h"
#include "loader.h"
#include <signal.h>
#include <unistd.h>

static int image_index = 0;

/* display image */

struct TerminalImage
{
    ImageCachePtr cache;
    short x;
    short y;
    short sx;
    short sy;
    short width;
    short height;
};
using TerminalImagePtr = std::shared_ptr<TerminalImage>;

static std::vector<TerminalImagePtr> terminal_image;

class ImageDisplay
{
    FILE *Imgdisplay_rf = NULL;
    FILE *Imgdisplay_wf = NULL;
    pid_t Imgdisplay_pid = 0;

public:
    bool openImgdisplay()
    {
        Imgdisplay_pid = open_pipe_rw(&Imgdisplay_rf, &Imgdisplay_wf);
        if (Imgdisplay_pid < 0)
            goto err0;
        if (Imgdisplay_pid == 0)
        {
            /* child */
            std::string cmd;
            setup_child(false, 2, -1);
            if (!strchr(ImageManager::Instance().Imgdisplay.c_str(), '/'))
                cmd = svu::join("", w3m_auxbin_dir(), "/", ImageManager::Instance().Imgdisplay);
            else
                cmd = ImageManager::Instance().Imgdisplay;
            myExec(cmd.c_str());
            /* XXX: ifdef __EMX__, use start /f ? */
        }
        ImageManager::Instance().activeImage = true;
        return true;
    err0:
        Imgdisplay_pid = 0;
        ImageManager::Instance().activeImage = false;
        return false;
    }

    ~ImageDisplay()
    {
        if (Imgdisplay_wf)
        {
            fputs("2;\n", Imgdisplay_wf); /* ClearImage() */
            fflush(Imgdisplay_wf);
        }
        // closeImgdisplay();

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

    bool m_syncRequired = false;

    bool syncImage()
    {
        if (!m_syncRequired)
        {
            return false;
        }
        m_syncRequired = false;

        fputs("3;\n", Imgdisplay_wf); /* XSync() */
        fputs("4;\n", Imgdisplay_wf); /* put '\n' */
        while (fflush(Imgdisplay_wf) != 0)
        {
            if (ferror(Imgdisplay_wf))
                return false;
        }
        if (!fgetc(Imgdisplay_rf))
            return false;
        return true;
        // err:
        // closeImgdisplay();
        // image_index += MAX_IMAGE;
        // terminal_image.clear();
    }

    void draw(const TerminalImagePtr &i)
    {
        char buf[64];
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

        m_syncRequired = true;
    }

    void clear(const TerminalImagePtr &i)
    {
        char buf[64];
        sprintf(buf, "6;%d;%d;%d;%d\n", i->x, i->y, i->width, i->height);
        fputs(buf, Imgdisplay_wf);

        m_syncRequired = true;
    }
};

///
/// ImageManager
///
ImageManager::ImageManager()
    : m_imgDisplay(new ImageDisplay)
{
}

ImageManager::~ImageManager()
{
    clearImage();
    delete m_imgDisplay;
}

void ImageManager::initImage()
{
    // if (ImageManager::Instance().displayImage)
    // initImage();
    // if (this->fmInitialized && ImageManager::Instance().displayImage)

    if (ImageManager::Instance().activeImage)
        return;
    if (getCharSize())
        ImageManager::Instance().activeImage = true;
}

void ImageManager::addImage(const ImageCachePtr &cache, int x, int y, int sx, int sy, int w, int h)
{
    if (!ImageManager::Instance().activeImage)
        return;

    auto i = std::make_shared<TerminalImage>();
    terminal_image.push_back(i);
    i->cache = cache;
    i->x = x;
    i->y = y;
    i->sx = sx;
    i->sy = sy;
    i->width = w;
    i->height = h;
}

void ImageManager::drawImage()
{
    if (!ImageManager::Instance().activeImage)
        return;
    if (!ImageManager::Instance().displayImage)
        return;

    for (auto &i : terminal_image)
    {
        if (!(i->cache->loaded & IMG_FLAG_LOADED &&
              i->width > 0 && i->height > 0))
            continue;
        m_imgDisplay->draw(i);
    }
    m_imgDisplay->syncImage();
    Screen::Instance().TouchCursor();
    Screen::Instance().Refresh();
    Terminal::flush();
}

void ImageManager::clearImage()
{
    if (!ImageManager::Instance().activeImage)
        return;
    if (!terminal_image.empty())
        return;

    for (auto &i : terminal_image)
    {
        if (!(i->cache->loaded & IMG_FLAG_LOADED &&
              i->width > 0 && i->height > 0))
            continue;
        m_imgDisplay->clear(i);
    }
    m_imgDisplay->syncImage();
    terminal_image.clear();
}

/* load image */

#ifndef MAX_LOAD_IMAGE
#define MAX_LOAD_IMAGE 8
#endif

static std::unordered_map<std::string, ImageCachePtr> image_hash;
static std::unordered_map<std::string, ImageCachePtr> image_file;
static std::list<ImageCachePtr> image_list;
static std::vector<ImageCachePtr> image_cache;
static BufferPtr image_buffer = NULL;

void ImageManager::deleteImage(Buffer *buf)
{
    if (!buf)
        return;

    for (auto &a : buf->m_document->img.anchors)
    {
        if (a->image && a->image->cache &&
            a->image->cache->loaded != IMG_FLAG_UNLOADED &&
            !(a->image->cache->loaded & IMG_FLAG_DONT_REMOVE) &&
            a->image->cache->index < 0)
            unlink(a->image->cache->file);
    }
    loadImage(NULL, IMG_FLAG_STOP);
}

void ImageManager::getAllImage(const BufferPtr &buf)
{
    image_buffer = buf;
    if (!buf)
        return;

    AnchorPtr a;
    URL *current;
    int i;

    buf->image_loaded = true;
    auto &al = buf->m_document->img;
    if (!al)
        return;
    current = &buf->url;
    for (auto &a : al.anchors)
    {
        if (a->image)
        {
            a->image->cache = getImage(a->image, current, buf->image_flag);
            if (a->image->cache &&
                a->image->cache->loaded == IMG_FLAG_UNLOADED)
                buf->image_loaded = false;
        }
    }
}

void showImageProgress(const BufferPtr &buf)
{
    AnchorPtr a;
    int i, l, n;

    if (!buf)
        return;
    auto &al = buf->m_document->img;
    if (!al)
        return;
    for (auto &a : al.anchors)
    {
        if (a->image && a->hseq >= 0)
        {
            n++;
            if (a->image->cache && a->image->cache->loaded & IMG_FLAG_LOADED)
                l++;
        }
    }
    if (n)
    {
        message(Sprintf("%d/%d images loaded", l, n)->ptr, buf->rect);
        Screen::Instance().Refresh();
        Terminal::flush();
    }
}

void ImageManager::loadImage(const BufferPtr &buf, ImageLoadFlags flag)
{
    if (!ImageManager::Instance().activeImage)
        return;

    if (ImageManager::Instance().maxLoadImage > MAX_LOAD_IMAGE)
        ImageManager::Instance().maxLoadImage = MAX_LOAD_IMAGE;
    else if (ImageManager::Instance().maxLoadImage < 1)
        ImageManager::Instance().maxLoadImage = 1;

    bool draw = false;
    for (int i = 0; i < image_cache.size(); ++i)
    {
        auto cache = image_cache[i];
        if (!cache)
            continue;
        struct stat st;
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
                    image_buffer->need_reshape = true;
            }
            draw = true;
        }
        else
            cache->loaded = IMG_FLAG_ERROR;
        unlink(cache->touch);
        image_cache[i] = NULL;
    }

    for (int i = 0; i < image_cache.size(); ++i)
    {
        auto cache = image_cache[i];
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
        image_list.clear();
        image_file.clear();
        image_cache.clear();
        image_buffer = NULL;
        return;
    }

    if (draw && image_buffer)
    {
        drawImage();
        showImageProgress(image_buffer);
    }

    image_buffer = buf;

    if (image_list.empty())
        return;
    for (int i = 0; i < image_cache.size(); i++)
    {
        if (image_cache[i])
            continue;

        ImageCachePtr cache;
        while (1)
        {
            cache = image_list.back();
            image_list.pop_back();
            if (!cache)
            {
                for (i = 0; i < image_cache.size(); i++)
                {
                    if (image_cache[i])
                        return;
                }
                // if (image_buffer)
                //     displayBuffer(image_buffer, B_NORMAL);
                return;
            }
            if (cache->loaded == IMG_FLAG_UNLOADED)
                break;
        }
        image_cache[i] = cache;

        Terminal::flush();
        if ((cache->pid = fork()) == 0)
        {
            /*
            * setup_child(true, 0, -1);
            */
            setup_child(false, 0, -1);
            this->image_source = cache->file;
            auto b = loadGeneralFile(URL::Parse(cache->url, nullptr), cache->current, HttpReferrerPolicy::StrictOriginWhenCrossOrigin);
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

ImageCachePtr
getImage(Image *image, URL *current, ImageFlags flag)
{
    std::string key;
    ImageCachePtr cache;

    if (!ImageManager::Instance().activeImage)
        return NULL;
    if (image->cache)
        cache = image->cache;
    else
    {
        key = Sprintf("%d;%d;%s", image->width, image->height, image->url)->ptr;
        auto found = image_hash.find(key);
        if (found != image_hash.end())
        {
            cache = found->second;
        }
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

        cache = std::make_shared<ImageCache>();
        cache->url = image->url;
        cache->current = current;
        cache->file = tmpfname(TMPF_DFL, image->ext)->ptr;
        cache->touch = tmpfname(TMPF_DFL, NULL)->ptr;
        cache->pid = 0;
        cache->index = 0;
        cache->loaded = IMG_FLAG_UNLOADED;
        cache->width = image->width;
        cache->height = image->height;

        image_hash.insert(std::make_pair(key, cache));
    }
    if (flag != IMG_FLAG_SKIP)
    {
        if (cache->loaded == IMG_FLAG_UNLOADED)
        {
            auto found = image_file.find(cache->file);
            if (found == image_file.end())
            {
                image_file.insert(std::make_pair(cache->file, cache));
                image_list.push_back(cache);
            }
        }
        if (!cache->index)
            cache->index = ++image_index;
    }
    if (cache->loaded & IMG_FLAG_LOADED)
        getImageSize(cache);
    return cache;
}

int getImageSize(ImageCachePtr cache)
{
    Str tmp;
    FILE *f;
    int w = 0, h = 0;

    if (!ImageManager::Instance().activeImage)
        return false;
    if (!cache || !(cache->loaded & IMG_FLAG_LOADED) ||
        (cache->width > 0 && cache->height > 0))
        return false;
    tmp = Strnew();
    if (ImageManager::Instance().Imgdisplay.find('/') == std::string::npos)
        Strcat_m_charp(tmp, w3m_auxbin_dir(), "/", NULL);
    Strcat_m_charp(tmp, ImageManager::Instance().Imgdisplay, " -size ", shell_quote(cache->file), NULL);
    f = popen(tmp->ptr, "r");
    if (!f)
        return false;
    while (fscanf(f, "%d %d", &w, &h) < 0)
    {
        if (feof(f))
            break;
    }
    pclose(f);

    if (!(w > 0 && h > 0))
        return false;
    w = (int)(w * ImageManager::Instance().image_scale / 100 + 0.5);
    if (w == 0)
        w = 1;
    h = (int)(h * ImageManager::Instance().image_scale / 100 + 0.5);
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
    image_hash.insert(std::make_pair(tmp->ptr, cache));
    return true;
}

bool ImageManager::getCharSize()
{
    set_environ("W3M_TTY", Terminal::ttyname_tty());
    auto tmp = Strnew();
    if (ImageManager::Instance().Imgdisplay.find('/') == std::string::npos)
        Strcat_m_charp(tmp, w3m_auxbin_dir(), "/", NULL);
    Strcat_m_charp(tmp, ImageManager::Instance().Imgdisplay, " -test 2>/dev/null", NULL);

    int w = 0, h = 0;
    {
        auto f = popen(tmp->ptr, "r");
        if (!f)
            return false;

        while (fscanf(f, "%d %d", &w, &h) < 0)
        {
            if (feof(f))
                break;
        }
        pclose(f);
    }

    if (w <= 0 || h <= 0)
        return false;

    if (!this->set_pixel_per_char)
        this->pixel_per_char = (int)(1.0 * w / Terminal::columns() + 0.5);
    if (!this->set_pixel_per_line)
        this->pixel_per_line = (int)(1.0 * h / Terminal::lines() + 0.5);
    return true;
}
