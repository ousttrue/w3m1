#pragma once
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
    ImageCache *cache;
};

void initImage();
void clearImage();
void addImage(ImageCache *cache, int x, int y, int sx, int sy, int w, int h);
void drawImage();
void termImage();
ImageCache *getImage(Image *image, URL *current, int flag);
int getImageSize(ImageCache *cache);
char *xface2xpm(char *xface);
