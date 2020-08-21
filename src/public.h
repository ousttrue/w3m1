#pragma once
#include "dispatcher.h"
#include "frontend/tab.h"
#include "frontend/buffer.h"
#include "frontend/search.h"
#include "stream/http.h"
class w3mApp;

/* 
 * Command functions: These functions are called with a keystroke.
 */
int searchKeyNum();

void srch_nxtprv(int reverse);
int dispincsrch(int ch, Str buf, Lineprop *prop);

void isrch(SearchFunc func, const char* prompt);
void srch(SearchFunc func, const char* prompt);


void disp_srchresult(int result, const char* prompt, const char *str);
void shiftvisualpos(BufferPtr buf, int shift);

void cmd_loadfile(const char *fn);
void cmd_loadURL(std::string_view url, URL *current, HttpReferrerPolicy referer, FormList *request);
int handleMailto(const char *url);
void _movL(int n);
void _movD(int n);
void _movU(int n);
void _movR(int n);
int prev_nonnull_line(BufferPtr buf, LinePtr line);
int next_nonnull_line(BufferPtr buf, LinePtr line);
char *getCurWord(BufferPtr buf, int *spos, int *epos);
void prevChar(int *s, LinePtr l);
void nextChar(int *s, LinePtr l);
uint32_t getChar(char *p);
int is_wordchar(uint32_t c);
SearchResultTypes srchcore(const char * str, SearchFunc search);

void _goLine(std::string_view l);
int cur_real_linenumber(const BufferPtr &buf);
const char *MarkString();
void SetMarkString(const char *str);
void do_dump(w3mApp *w3m, BufferPtr buf);
void _followForm(bool submit);
void query_from_followform(Str *query, FormItemList *fi, int multipart);
// BufferPtr loadLink(const char *url, const char *target, HttpReferrerPolicy referer, FormList *request);
FormItemList *save_submit_formlist(FormItemList *src);
Str conv_form_encoding(Str val, FormItemList *fi, BufferPtr buf);
void bufferA();
// BufferPtr loadNormalBuf(BufferPtr buf, int renderframe);
void _nextA(int visited);
void _prevA(int visited);
void gotoLabel(std::string_view label);
int check_target();

void nextX(int d, int dy);
void nextY(int d);

void goURL0(const char* prompt, int relative);
void anchorMn(Anchor *(*menu_func)(const BufferPtr &), int go);
void _peekURL(int only_img);
Str currentURL(void);

void _docCSet(CharacterEncodingScheme charset);
int display_ok();
void invoke_browser(char *url);
void execdict(char *word);
char *GetWord(const BufferPtr &buf);
void tabURL0(TabPtr tab, const char* prompt, int relative);

char *convert_size3(clen_t size);
void resetPos(BufferPos *b);
void stopDownload();
void download_action(struct parsed_tagarg *arg);
int checkDownloadList();
void addDownloadList(pid_t pid, char *url, char *save, char *lock, clen_t size);
int add_download_list();
void set_add_download_list(int add);

void deleteFiles();
char *searchKeyData();

int sysm_process_mouse();
// int gpm_process_mouse(Gpm_Event *, void *);
void chkNMIDBuffer(const BufferPtr &buf);

void change_charset(struct parsed_tagarg *arg);
void follow_map(struct parsed_tagarg *arg);
void followForm();
void SigPipe(SIGNAL_ARG);

void intTrap(SIGNAL_ARG);
BufferPtr cookie_list_panel(void);
// void cmd_loadBuffer(BufferPtr buf, BufferProps prop, LinkBufferTypes linkid);
