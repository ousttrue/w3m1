#pragma once
#include "command_dispatcher.h"
#include "frontend/tab.h"
#include "frontend/buffer.h"

#include "stream/http.h"
class w3mApp;

/* 
 * Command functions: These functions are called with a keystroke.
 */
void shiftvisualpos(BufferPtr buf, int shift);

void cmd_loadfile(const char *fn);
void cmd_loadURL(std::string_view url, URL *current, HttpReferrerPolicy referer, FormPtr request);
int handleMailto(const char *url);
int prev_nonnull_line(BufferPtr buf, LinePtr line);
int next_nonnull_line(BufferPtr buf, LinePtr line);
char *getCurWord(BufferPtr buf, int *spos, int *epos);
void prevChar(int *s, LinePtr l);
void nextChar(int *s, LinePtr l);
uint32_t getChar(char *p);
int is_wordchar(uint32_t c);


int cur_real_linenumber(const BufferPtr &buf);
const char *MarkString();
void SetMarkString(const char *str);
void do_dump(w3mApp *w3m, BufferPtr buf);
void _followForm(bool submit);
void query_from_followform(Str *query, FormItemPtr fi, int multipart);
// BufferPtr loadLink(const char *url, const char *target, HttpReferrerPolicy referer, FormList *request);
FormItemPtr save_submit_formlist(FormItemPtr src);
Str conv_form_encoding(std::string_view val, FormItemPtr fi, BufferPtr buf);
void bufferA(w3mApp *w3m, const CommandContext &context);
// BufferPtr loadNormalBuf(BufferPtr buf, int renderframe);
void _nextA(int visited, int n);
void _prevA(int visited, int n);
void gotoLabel(std::string_view label);
int check_target();

void nextX(int d, int dy, int n);
void nextY(int d, int n);

void goURL0(std::string_view url, std::string_view prompt, int relative);
void anchorMn(AnchorPtr (*menu_func)(const BufferPtr &), int go);
void _peekURL(int only_img, int n);
Str currentURL(void);

void _docCSet(CharacterEncodingScheme charset);
int display_ok();
void invoke_browser(char *url, std::string_view browser, int prec_num);
void execdict(char *word);
char *GetWord(const BufferPtr &buf);
void tabURL0(TabPtr tab, std::string_view url, const char *prompt, int relative);


void deleteFiles();
