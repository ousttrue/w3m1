# w3m

w3m を改造して遊ぶ。

## TODO

だいぶ壊れてきた。ぼちぼち

* [ ] Document Charset [Change] FollowForm でページが遷移しない
    * buffer->anchors.empty ?

* [x] plog
* [x] unittest. catch2 導入
* [x] load中のメッセージ表示が壊れた。Sprintf に std::string 投げてた
* [x] forward/back 壊れた
* [ ] Read/WriteBufferCacheの不整合。後で直す
* [x] Lineの保持する文字列が解放されている？。std::string になった
* [x] FormItem が解放されている？。GCを除去
* [x] redirect が壊れた。文字列連結のミス
* [ ] tab壊れた
* [ ] HtmlProcess reentrant
* [ ] STL化(Tab, Buffer, Line, Anchor, Form)
* [ ] Buffer::Reshape
* [ ] internal scheme "w3m:"
* [ ] local CGI
* [ ] stream cache => http cache

## Line
横方向の変数名。

* Cell, Column, Columns,
* Bytes, ByteLength,

len, size, pos, width は避ける。

## Group

* libwc / Str / symbol / entity / myctype / indep
* Line / terms
* Tab, Buffer
* Html
* Stream / URL / HTTP / file
* globals / rc / funcs
