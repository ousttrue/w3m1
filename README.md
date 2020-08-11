# w3m

w3m を改造して遊ぶ。

## TODO

だいぶ壊れてきた。ぼちぼち

* [ ] Document Charset [Change] FollowForm でページが遷移しない
* [ ] logger
* [x] unittest. catch2 導入
* [x] load中のメッセージ表示が壊れた。Sprintf に std::string 投げてた
* [ ] forward/back 壊れた
    * [ ] Read/WriteBufferCacheの不整合。後で直す
    * [x] Lineの保持する文字列が解放されている？。std::string になった
    * [ ] FormItem が解放されている？
* [x] redirect が壊れた。文字列連結のミス
* [ ] tab壊れた
* [ ] HtmlProcess reentrant
* [ ] STL化(Tab, Buffer, Line, Anchor)
* [ ] loadGeneralFileが改造困難。機能を大幅に落とす

## Group

* libwc / Str / symbol / entity / myctype / indep
* Line / terms
* Tab, Buffer
* Html
* Stream / URL / file
* globals / rc / funcs
