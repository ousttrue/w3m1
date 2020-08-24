#pragma once
#include <memory>
struct URL;

#define MAX_IMAGE 1000
#define MAX_IMAGE_SIZE 2048

struct ImageCache
{
    char *url;
    URL *current;
    char *file;
    char *touch;
    pid_t pid;
    char loaded;
    int index;
    short width;
    short height;
};
using ImageCachePtr = std::shared_ptr<ImageCache>;

enum ImageFlags
{
    IMG_FLAG_SKIP = 1,
    IMG_FLAG_AUTO = 2,
};

enum ImageLoadFlags
{
    IMG_FLAG_UNLOADED = 0,
    IMG_FLAG_LOADED = 1,
    IMG_FLAG_ERROR = 2,
    IMG_FLAG_DONT_REMOVE = 4,
};

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
ImageCachePtr getImage(Image *image, URL *current, int flag);
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
