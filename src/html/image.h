#pragma once
#include <memory>
struct URL;

#define MAX_IMAGE 1000
#define MAX_IMAGE_SIZE 2048

enum ImageCacheStatus
{
    IMG_FLAG_UNLOADED = 0,
    IMG_FLAG_LOADED = 1,
    IMG_FLAG_ERROR = 2,
    IMG_FLAG_DONT_REMOVE = 4,
};

struct ImageCache
{
    char *url;
    URL *current;
    char *file;
    char *touch;
    pid_t pid;
    ImageCacheStatus loaded;
    int index;
    short width;
    short height;
};
using ImageCachePtr = std::shared_ptr<ImageCache>;

struct Image
{
    char *url;
    const char *ext;
    short width;
    short height;
    short xoffset;
    short yoffset;
    short y;
    short rows;
    char *map;
    char ismap;
    int touch;
    ImageCachePtr cache;
};

void addImage(ImageCachePtr cache, int x, int y, int sx, int sy, int w, int h);
void drawImage();

enum ImageFlags
{
    IMG_FLAG_NONE = 0,
    IMG_FLAG_SKIP = 1,
    IMG_FLAG_AUTO = 2,
};
ImageCachePtr getImage(Image *image, URL *current, ImageFlags flag);
int getImageSize(ImageCachePtr cache);
char *xface2xpm(char *xface);

class ImageManager
{
    ImageManager();
    ~ImageManager();
    ImageManager(const ImageManager &) = delete;
    ImageManager &operator=(const ImageManager &) = delete;

public:
    static ImageManager &Instance()
    {
        static ImageManager s_instance;
        return s_instance;
    }
    void initImage();
    void clearImage();
};

struct Buffer;
using BufferPtr = std::shared_ptr<Buffer>;
void deleteImage(Buffer *buf);
void getAllImage(const BufferPtr &buf);

enum ImageLoadFlags
{
    IMG_FLAG_START = 0,
    IMG_FLAG_STOP = 1,
    IMG_FLAG_NEXT = 2,
};
void loadImage(BufferPtr buf, ImageLoadFlags flag);
