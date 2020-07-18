import pathlib
import re
from typing import List
HERE = pathlib.Path(__file__).absolute().parent


class Func:
    def __init__(self, name, key, description):
        self.name = name
        self.key = key
        self.description = description
        self.body = []

    def __str__(self) -> str:
        return f'{self.name}({self.key}): {len(self.body)}lines, {self.description}'

    def declare(self) -> str:
        return f'void {self.name}();'

    def define(self) -> str:
        declare = self.declare()
        declare = declare[0:len(declare) - 1]
        lines = [declare] + self.body
        return '\n'.join(lines)


def main(path: pathlib.Path):
    compiled = re.compile(r'DEFUN\((\w+),\s*(.*?),\s*"([^"]+)"\)')
    print(path)
    current = Func('', '', '')
    funcs: List[Func] = [current]
    for l in path.read_text().split('\n'):
        if not l.strip():
            continue
        m = compiled.match(l)
        if m:
            current = Func(m.group(1), m.group(2), m.group(3))
            funcs.append(current)
        else:
            if current:
                current.body.append(l)
    return funcs


def header(path: pathlib.Path, funcs: List[Func]):
    with path.open('w') as w:
        w.write('''#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

''')
        for f in funcs:
            if not f.name:
                continue
            w.write(f.declare())
            w.write('\n')

        w.write('''
#ifdef __cplusplus
}
#endif
''')
    print(path)


def body(path: pathlib.Path, funcs: List[Func]):
    with path.open('w') as w:
        w.write('''
#include "commands.h"

extern "C"
{
''')
        for f in funcs:
            w.write('\n')
            if not f.name:
                w.write('\n'.join(f.body))
                w.write('\n')
            else:
                w.write(f.define())
                w.write('\n')
        
        w.write("}\n")
    print(path)


def register(path: pathlib.Path, funcs: List[Func]):
    with path.open('w') as w:
        w.write('''#pragma once

#include "dispatcher.h"          
#include "commands.h"


void register_commands()
{
''')
        for f in funcs:
            if not f.name:
                continue
            w.write(f'  RegisterCommand("{f.name}", "{f.key}", "{f.description}", &{f.name});\n')

        w.write('''
}
''')
    print(path)


if __name__ == '__main__':
    funcs = main(HERE / 'defun.c')
    # for f in funcs:
    #     print(f)
    header(HERE / 'commands.h', funcs)
    body(HERE / 'commands.cpp', funcs)
    register(HERE / 'register_commands.h', funcs)
