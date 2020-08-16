#pragma once
#include "url.h"

int openSocket(const char *hostname,
               const char *remoteport_name, unsigned short remoteport_num);
int openSocket(const URL &url);
