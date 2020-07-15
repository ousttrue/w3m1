# w3m

w3m を改造して遊ぶ。

## その１。管理しやすいようにする

最近のUbuntuとかだとビルドできなかった。
しかし、msys2 ならわりと簡単にビルドできることを発見。

```
$ pacman -S make gcc libgc-devel openssl-devel ncurses-devel
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
