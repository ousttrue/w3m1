# w3m

w3m を改造して遊ぶ。

## TODO

だいぶ壊れてきた。ぼちぼち

* [ ] logger
* [ ] gtest
* [x] load中のメッセージ表示が壊れた。Sprintf に std::string 投げてた
* [ ] prev/forward 壊れた。Read/WriteBufferCacheの不整合。後で直す
* [x] redirect が壊れた。文字列連結のミス
* [ ] tab壊れた
* [ ] 遅くなった
* [ ] std::string_view(std::string), nullptr に弱い(長さを得ようとしてstrlen時)

## msys2 でとりあえずビルド

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

## Ubuntu build

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

## `Wrong __data_start/_end pair` 解決

https://hitkey.nekokan.dyndns.info/diary2004.php#D200424

によると、stack size の制限が原因らしい。

```
$ ulimit -s
8192
```

WSL でこれを変えるには・・・。

https://github.com/microsoft/WSL/issues/633

無理。

`WSL2` ならできる？

```
> wsl -l -v
  NAME            STATE           VERSION
* Ubuntu-20.04    Running         1
```

やってみる。

```
$ ulimit -s unlimited
> ulimit -s
unlimited
```

できた。

```
$ w3m
Wrong __data_start/_end pair
```

うーむ。

```
$ ulimit -s 81920
> ulimit -s
81920
```

動いた。
8192KB では足りなく、 unlimted では多すぎるらしい。難儀な。

* WSL Ubuntu-20.04 で `apt install w3m` 動くようになった
* w3m-0.5.3 のビルド。`mktable` (GC) 使っているが落ちなくなったのでビルドできるようになった。ビルド結果も動く。

## main.cpp

とりあえず `main.c` を `main.cpp` にリネームして全部、 `extern "C"` に入れた。
`extern C` の領域を減らしたい。

* 暗黙関数定義を禁止。 `-Werror=implicit-function-declaration`
* 暗黙のint変換を禁止。 `-Werror=int-conversion`

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

    // TODO:
    if(dirty)
    {
        displayBuffer();
    }
}
```

## DEFUN

```
Key
    KeyMap
        DEFUN
    Menu
        DEFUN
MouseAction
    ActionMap
        DEFUN
    Menu
        DEFUN
Alarm
    DEFUN
```

Key, Mouse, Alarm を入力に DEFUN に Dispatch する。
ディスパッチするときに、
コマンド名 => `hashTable の index` => 関数ポインタ という風になっていて、 `hashTable の index(hash.h)` がよくわからなかったのだけど、

```c++
typedef void (*Command)();
std::unordered_map<std::string, Command> g_commandMap;
```

で置き換えることができた。
コードを整理する。

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

* bzero, bcopy, memcpy
* const char*

## proto.h

すべての関数定義が投入されているので、各ソースのヘッダに分配する。
DEFUN 由来の `void xxx(void)` という関数は、 `commands.h` に移ったので、
`proto.h` から削除する。
適当に `file.h` ほかに分配して削除した。

## C++ 化

一個ずつ拡張子を `.cpp` に変えて `#include` を `extern "C"` で囲ってコンパイルを通した。
で、まとめて `extern "C"` を削除した。
libwc を除いてうまく C++ 化できたようだ。
これで、メソッド生やしたりかなり自由に変更できる。
とりあえず、`struct tag` まわりの `typedef` を取り除く。
然る後に、前方宣言を活用してヘッダの分割を推進。

## モジュールに分割

機能ごとにモジュールに分割する。

* Term
    * 低レベル描画
        * tputs termio/terminfo
    * キーボード入力
    * マウス入力
    * リサイズイベント
    * SIGNALハンドリング
* UI
    * 高レベル描画
    * Tab
    * Buffer
    * Message
    * Menu
    * Keymap
* IO
    * IStream
        * polymorphism化
        * http
            * cookie
            * redirect
            * referer
        * https
        * ftp
    * Compression
    * LocalCGI
* String
    * 文字コード
    * URL
    * html parse / term rendering

## TabBuffer, Buffer

`CurrentTab`, `FirstTab`, `LastTab`, `CurrentBuf`, `FirstBuf` マクロを関数化する。

`FirstBuffer` => `GetCurrentTab()->GetFirstBuffer()`, `GetCurrentTab()->SetFirstBuffer(BufferPtr buf)` というふうにする。
Setter は隠蔽する。
Tabは必ず1以上。
  * CurrentTab の null check 無くせる？

TabBuffer の双方向リンクリストを `std::list<std::shared_ptr<TabBuffer>>` に置き換えた。
Buffer のリンクリストを Tab のメソッド化して、 Buffer::nextBuffer への直接アクセスを封じてからこれも、
`std::list` にする。


## Str

```c
struct _Str
{
    char *ptr;
    int length;
    int area_size;
};
using Str = _Str *;
```

ptr が boehmGC 管理になる。
`libwc` 広範囲で使っているが、メンバーを private 化して、関数をメンバー化すれば
同じインタフェースの他の実装に置き換えることはできそう。
`std::shared_ptr<_Str>` など。
ptrを取得して中身を書き換えるものをメソッド化して、ptr を private にしたい。

## HtmlParser

* loadHTMLBuffer
* loadHTMLStream
* HTMLlineproc0

と呼び出して、ここで token 化して解析する。
