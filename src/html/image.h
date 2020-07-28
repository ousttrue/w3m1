#pragma once



void initImage();
void clearImage();
void addImage(ImageCache *cache, int x, int y, int sx, int sy, int w, int h);
void drawImage();
void termImage();
ImageCache *getImage(Image *image, ParsedURL *current, int flag);
int getImageSize(ImageCache *cache);
char *xface2xpm(char *xface);
