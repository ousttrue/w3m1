# w3m

w3m を改造して遊ぶ。

## TODO

だいぶ壊れてきた。ぼちぼち

* [ ] Document Charset [Change] FollowForm でページが遷移しない
* [ ] logger
* [x] unittest. catch2 導入
* [x] load中のメッセージ表示が壊れた。Sprintf に std::string 投げてた
* [ ] forward/back 壊れた
    * Read/WriteBufferCacheの不整合。後で直す
    * textbuffer が解放されている？
* [x] redirect が壊れた。文字列連結のミス
* [ ] tab壊れた
* [x] 遅くなった。なんか、元に戻った

* [ ] STL化(Tab, Buffer, Line, Anchor)
