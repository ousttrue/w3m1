#pragma once
struct ParsedURL;

struct ImageCache
{
    char *url;
    ParsedURL *current;
    char *file;
    char *touch;
    pid_t pid;
    char loaded;
    int index;
    short width;
    short height;
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
ImageCache *getImage(Image *image, ParsedURL *current, int flag);
int getImageSize(ImageCache *cache);
char *xface2xpm(char *xface);
