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
