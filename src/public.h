#pragma once
#include "dispatcher.h"
#include "tab.h"

struct DownloadList
{
    pid_t pid;
    char *url;
    char *save;
    char *lock;
    clen_t size;
    time_t time;
    int running;
    int err;
    DownloadList *next;
    DownloadList *prev;
};

global DownloadList *FirstDL init(NULL);
global DownloadList *LastDL init(NULL);

/* 
 * Command functions: These functions are called with a keystroke.
 */
int searchKeyNum();
void nscroll(int n);
void srch_nxtprv(int reverse);
int dispincsrch(int ch, Str buf, Lineprop *prop);
void isrch(int (*func)(BufferPtr , char *), const char* prompt);
void srch(int (*func)(BufferPtr , char *), const char* prompt);
void clear_mark(Line *l);
void disp_srchresult(int result, const char* prompt, char *str);
void shiftvisualpos(BufferPtr buf, int shift);

void cmd_loadfile(char *fn);
void cmd_loadURL(char *url, ParsedURL *current, char *referer, FormList *request);
int handleMailto(const char *url);
void _movL(int n);
void _movD(int n);
void _movU(int n);
void _movR(int n);
int prev_nonnull_line(Line *line);
int next_nonnull_line(Line *line);
char *getCurWord(BufferPtr buf, int *spos, int *epos);
void prevChar(int *s, Line *l);
void nextChar(int *s, Line *l);
uint32_t getChar(char *p);
int is_wordchar(uint32_t c);
int srchcore(char * str, int (*func)(BufferPtr , char *));
void _quitfm(int confirm);
void _goLine(char *l);
int cur_real_linenumber(BufferPtr buf);
char *inputLineHist(const char* prompt, const char *def_str, int flag, Hist *hist);
char *inputStrHist(const char* prompt, char *def_str, Hist *hist);
char *inputLine(const char* prompt, char *def_str, int flag);
char *MarkString();
void SetMarkString(char *str);
void do_dump(BufferPtr buf);
void _followForm(int submit);
void query_from_followform(Str *query, FormItemList *fi, int multipart);
BufferPtr loadLink(const char *url, const char *target, const char *referer, FormList *request);
FormItemList *save_submit_formlist(FormItemList *src);
Str conv_form_encoding(Str val, FormItemList *fi, BufferPtr buf);
void bufferA();
BufferPtr loadNormalBuf(BufferPtr buf, int renderframe);
void _nextA(int visited);
void _prevA(int visited);
void gotoLabel(const char *label);
int check_target();
void set_check_target(int);
void nextX(int d, int dy);
void nextY(int d);
int checkBackBuffer(TabPtr tab, BufferPtr buf);
void goURL0(const char* prompt, int relative);
void anchorMn(Anchor *(*menu_func)(BufferPtr ), int go);
void _peekURL(int only_img);
Str currentURL(void);
void repBuffer(BufferPtr oldbuf, BufferPtr buf);
void _docCSet(wc_ces charset);
int display_ok();
void invoke_browser(char *url);
void execdict(char *word);
char *GetWord(BufferPtr buf);
AlarmEvent *DefaultAlarm();
AlarmEvent *CurrentAlarm();
void SetCurrentAlarm(AlarmEvent *);
void SigAlarm(SIGNAL_ARG);
void tabURL0(TabPtr tab, const char* prompt, int relative);
BufferPtr DownloadListBuffer();
char *convert_size3(clen_t size);
void resetPos(BufferPos *b);
void save_buffer_position(BufferPtr buf);
void stopDownload();
void download_action(struct parsed_tagarg *arg);
int checkDownloadList();
void addDownloadList(pid_t pid, char *url, char *save, char *lock, clen_t size);
int add_download_list();
void set_add_download_list(int add);
AlarmEvent *setAlarmEvent(AlarmEvent *event, int sec, short status, Command cmd, void *data);
void w3m_exit(int i);
void deleteFiles();
char *searchKeyData();

int sysm_process_mouse();
// int gpm_process_mouse(Gpm_Event *, void *);
void chkNMIDBuffer(BufferPtr buf);
void chkURLBuffer(BufferPtr buf);
void change_charset(struct parsed_tagarg *arg);
void follow_map(struct parsed_tagarg *arg);
void followForm();
void SigPipe(SIGNAL_ARG);
void resize_screen();
int need_resize_screen();
void resize_hook(SIGNAL_ARG);
void saveBufferInfo();
void pushEvent(Command cmd, void *data);
int ProcessEvent();
Str checkType(Str s, Lineprop **oprop, Linecolor **ocolor);
void intTrap(SIGNAL_ARG);
