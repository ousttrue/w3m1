#pragma once
#include "frontend/event.h"

// int prec_num();
// void set_prec_num(int n);
// inline int PREC_LIMIT() { return 10000; }

void ClearCurrentKeyData();
char *CurrentKeyData();
void SetMultiKey(int c);
int CurrentIsMultiKey();
int MultiKey(int c);
int PrevKey();
void SetPrevKey(int key);
void escKeyProc(char c);
void escbKeyProc(char c);
void MultiKeyProc(char c);
void DispatchKey(int c);
char *GetKeyData(int key);
void SetKeymap(std::string_view p, int lineno, int verbose);

void RegisterCommand(const char *name, const char *key, const char *description, Command command);
Command getFuncList(const std::string &name);

std::tuple<std::string_view, std::string> getQWord(std::string_view str);
std::tuple<std::string_view, std::string_view> getWord(std::string_view str);
void initKeymap(int force);

class CommandDispatcher
{
public:
    static CommandDispatcher &Instance()
    {
        static CommandDispatcher s_instance;
        return s_instance;
    }
    void ExecuteCommand(std::string_view data);
};
