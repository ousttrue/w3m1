# w3m

w3m を改造して遊ぶ。

## その１。管理しやすいようにする

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

## TODO

* cmake にする
* mktable を python とかにする(今バージョンのUbuntuだと死ぬので)
* c++ にする
* macro 減らす
* bohem-gc やめる(遠大)
