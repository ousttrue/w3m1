# w3m

w3m を改造して遊ぶ。

## その１。 msys2 でとりあえあずビルド

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
> ./build/w3m
Wrong __data_start/_end pair
fish: './build/w3m' terminated by signal SIGABRT (Abort)
```

```
apt install libgc-dev libssl-dev ncurses-dev
```

## その３。mktable

```
$ gcc -I. -g -O2 -I./libwc -I/usr/include/openssl -DHAVE_CONFIG_H -DAUXBIN_DIR="/mingw64/libexec/w3m" -DCGIBIN_DIR="/mingw64/libexec/w3m/cgi-bin" -DHELP_DIR="/mingw64/share/w3m" -DETC_DIR="/mingw64/etc" -DCONF_DIR="/mingw64/etc/w3m" -DRC_DIR="~/.w3m" -DLOCALEDIR="/mingw64/share/locale" -o mktable mktable.c Str.c myctype.c hash.c -ldl -lm -lgc
> ./mktable
Wrong __data_start/_end pair
fish: './mktable' terminated by signal SIGABRT (Abort)
```

これも動かない。

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

WIP

## TODO

* mktable を python とかにする(Ubuntu-20.04だと死ぬ)
* c++ にする
* macro 減らす
* DEFUNマクロをluaとかに置き換えたい
* bohem-gc やめる(遠大)
