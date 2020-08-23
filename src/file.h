#pragma once
#include "stream/input_stream.h"
#include "frontend/buffer.h"
#include "frontend/event.h"

struct HttpRequest;
struct Hist;

// char *guess_save_name(BufferPtr buf, std::string_view file);

Str convertLine(URLSchemeTypes scheme, Str line, int mode, CharacterEncodingScheme *charset, CharacterEncodingScheme doc_charset);
const char *guess_filename(std::string_view file);
int is_boundary(unsigned char *, unsigned char *);
int is_blank_line(char *line, int indent);
int getMetaRefreshParam(const char *q, Str *refresh_uri);

char *convert_size(clen_t size, int usefloat);
char *convert_size2(clen_t size1, clen_t size2, int usefloat);
void showProgress(clen_t *linelen, clen_t *trbyte, long long content_length);

BufferPtr loadImageBuffer(const URL &url, const InputStreamPtr &stream);
void saveBuffer(BufferPtr buf, FILE *f, int cont);
void saveBufferBody(BufferPtr buf, FILE *f, int cont);
BufferPtr getshell(char *cmd);
BufferPtr getpipe(char *cmd);
BufferPtr openPagerBuffer(const InputStreamPtr &stream, CharacterEncodingScheme content_charset);
BufferPtr openGeneralPagerBuffer(const InputStreamPtr &stream, CharacterEncodingScheme content_charset = WC_CES_UTF_8);
LinePtr getNextPage(BufferPtr buf, int plen);



int checkSaveFile(const InputStreamPtr &stream, char *path);
int checkOverWrite(const char *path);
const char *inputAnswer(const char *prompt);
int matchattr(char *p, const char *attr, int len, Str *value);

// char *checkHeader(BufferPtr buf, const char *field);

void addChar(char c, Lineprop mode = P_UNKNOWN);

void addMChar(char *c, Lineprop mode, size_t len);

BufferPtr message_list_panel(void);

char *lastFileName(const char *path);
const char *mydirname(const char *s);

Str unescape_spaces(Str s);
double log_like(int x);
struct table *newTable(void);

void print_item(struct table *t, int row, int col, int width, Str buf);

void check_rowcol(struct table *tbl, struct table_mode *mode);
int minimum_length(char *line);
void pushTable(struct table *, struct table *);


void preFormUpdateBuffer(const BufferPtr &buf);



void form_write_data(FILE *f, char *boundary, char *name, char *value);
void form_write_from_file(FILE *f, char *boundary, char *name,
                          char *filename, char *file);
int getMapXY(BufferPtr buf, const AnchorPtr a, int *x, int *y);
BufferPtr page_info_panel(const BufferPtr &buf);
struct frame_body *newFrame(struct parsed_tag *tag, BufferPtr buf);
struct frameset *newFrameSet(struct parsed_tag *tag);
void deleteFrame(struct frame_body *b);
void deleteFrameSet(struct frameset *f);
struct frameset *copyFrameSet(struct frameset *of);
void pushFrameTree(struct frameset_queue **fqpp, struct frameset *fs,
                   BufferPtr buf);
struct frameset *popFrameTree(struct frameset_queue **fqpp);
BufferPtr renderFrame(BufferPtr Cbuf, int force_reload);
union frameset_element *search_frame(struct frameset *fset, char *name);

MySignalHandler reset_exit(SIGNAL_ARG);
MySignalHandler error_dump(SIGNAL_ARG);

char *acceptableMimeTypes();

BufferPtr link_list_panel(const BufferPtr &buf);






char *libFile(char *base);
char *helpFile(char *base);
Str localCookie(void);
FILE *openSecretFile(const char *fname);
void loadPreForm(void);
const char *last_modified(const BufferPtr &buf);

void myExec(const char *command);
void mySystem(char *command, int background);
Str myExtCommand(const char *cmd, const char *arg, int redirect);
Str myEditor(const char *cmd, const char *file, int line);

enum TmpFileTypes
{
    TMPF_DFL = 0,
    TMPF_SRC = 1,
    TMPF_FRAME = 2,
    TMPF_CACHE = 3,
    TMPF_COOKIE = 4,
    MAX_TMPF_TYPE = 5,
};
Str tmpfname(TmpFileTypes type, const char *ext);

int setModtime(char *path, time_t modtime);
