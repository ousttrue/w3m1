#pragma once
#include <unistd.h>

void sig_chld(int signo);

void stopDownload();
void download_action(struct parsed_tagarg *arg);
int checkDownloadList();
void addDownloadList(pid_t pid, char *url, char *save, char *lock, clen_t size);
int add_download_list();
void set_add_download_list(int add);
