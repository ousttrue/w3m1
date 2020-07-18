# w3m

w3m を改造して遊ぶ。

## その１。 msys2 でとりあえずビルド

最近のUbuntuとかだとビルドできなかった。
しかし、msys2 ならわりと簡単にビルドできることを発見。

```
$ pacman -S make gcc libgc-devel openssl-devel ncurses-devel
$ x86_64-pc-msys-gcc --version
x86_64-pc-msys-gcc (GCC) 9.3.0
Copyright (C) 2019 Free Software Foundation, Inc.
This is free software; see the source for copying conditions.  There is NO
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
$ ./configure
```

手直し
```diff
// main.c
-    orig_GC_warn_proc = GC_set_warn_proc(wrap_GC_warn_proc);
+    /*orig_GC_warn_proc =*/ GC_set_warn_proc(wrap_GC_warn_proc);
```

```
// config.h
//#define USE_BINMODE_STREAM 1
//#define USE_EGD
```

```
$ make
$ ./w3m www.google.com // 動いた
```

## その２。 Ubuntu build

`config.h` や `functable.c` などの生成されるソースをコミットして、
`CMakeLists.txt` で `Ubuntu-20.04` でビルドできるようにした。
動かないけど。

```
# apt install gcc g++ make gdb
# apt install libgc-dev libssl-dev ncurses-dev
```

```
> ./build/w3m
Wrong __data_start/_end pair
fish: './build/w3m' terminated by signal SIGABRT (Abort)
```

```
$ gcc -I. -g -O2 -I./libwc -I/usr/include/openssl -DHAVE_CONFIG_H -DAUXBIN_DIR="/mingw64/libexec/w3m" -DCGIBIN_DIR="/mingw64/libexec/w3m/cgi-bin" -DHELP_DIR="/mingw64/share/w3m" -DETC_DIR="/mingw64/etc" -DCONF_DIR="/mingw64/etc/w3m" -DRC_DIR="~/.w3m" -DLOCALEDIR="/mingw64/share/locale" -o mktable mktable.c Str.c myctype.c hash.c -ldl -lm -lgc
> ./mktable
Wrong __data_start/_end pair
fish: './mktable' terminated by signal SIGABRT (Abort)
```

これも動かないが、gdb で動かすと動いてしまった。
w3mも動いた。
とりあえず Debug できる(しかできない)。

## その３。main.cpp

とりあえず `main.c` を `main.cpp` にリネームして全部、 `extern "C"` に入れた。
`extern C` の領域を減らしたい。
暗黙関数定義を禁止。 `-Werror=implicit-function-declaration`
暗黙のint変換を寄進。 `-Werror=int-conversion`
まず、DEFUN(1200行から6000行くらい？) を `defun.c` と `public.c` に分離する。
`main.cpp` が 1800行くらいになった。

### mainloop

だいたいこんな感じ

```c
while(true)
{
    // ここで入力をブロックする
    {
        do
        {
            if (need_resize_screen())
                resize_screen();
        } while (sleep_till_anykey(1, 0) <= 0);
    }
    auto c = getch();
    keyPressEventProc(c); // keymap に応じた関数の実行
}
```

## fm.h

グローバル変数減らす。

## データ構造

MouseAction
    ActionMap
        DEFUN
    Menu
        Defun
Key
    KeyMap
        DEFUN
    Menu
        Defun

## コード生成。mktable

| ファイル     | 生成方法          | 入力           | 備考                                                   |
|--------------|-------------------|----------------|--------------------------------------------------------|
| config.h     | configure         |                | 各種 #define など                                      |
| entity.h     | Makefile(mktable) | entity.tab     | ./mktable 100 entity.tab > entity.h                    |
| funcname.tab | Makefile(awk)     | main.c, menu.c |                                                        |
| funcname.c   | Makefile(awk)     | funcname.tab   | sort funcname.tab ｜ awk -f funcname0.awk > funcname.c |
| funcname1.h  | Makefile(awk)     | funcname.tab   |                                                        |
| funcname2.h  | Makefile(awk)     | funcname.tab   |                                                        |
| functable.c  | Makefile(mktable) | funcname.tab   |                                                        |
| tagtable.c   | Makefile(mktable) | funcname.tab   |                                                        |

hash.h, hash.c を std::unordered_map とかに置き換えたい・・・。

## TODO

* macro 減らす
* source と header が対応するようにする
    * fm.h 分解
    * proto.h 分解
* c++ にする
    * 各 struct を class化してなるべくカプセル化する
* DEFUNマクロをluaとかに置き換えたい
* bohem-gc やめる(遠大)

