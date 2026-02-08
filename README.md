# ZMK 薙刀式を改変したwinJIS向け3行新下駄版です。オリジナルと異なり一部3キー同時押しなど改変があります。

ZMK FirmwareでwinJIS向け3行新下駄かな入力を実現します。

薙刀式は大岡俊彦氏が考案されたかな入力方式です。

新下駄はkouy氏が考案されたかな入力方式です。

ZMKでの薙刀式実装はeswai氏が実装されており、こちらは改変させていただいたものになります。

本家の新下駄は数字キーまで同時押しが必要で少しコツが要り万人には簡単とは言いづらいこと、小さいキーボードでは実現ができないこと。

また本薙刀式32bit版では1キーの追加のみ可能なため、3行というのが丁度よいと思っています。

 げ、うぉに必要な追加キーとしてng SQTキーを、キーボードのngレイヤーのキーマップに追加してください。

http://oookaworks.seesaa.net/article/456099128.html

https://kouy.exblog.jp/13627994/

https://github.com/eswai/zmk-naginata

筆者Twitterアカウント:herm@PTclown

下記は2025/Oct/11　14時時点の現行のキーマップです。スピード性はあまりないかもしれませんが外来音はかなりカバーしていますのでタイパーでなければ必要十分かと。

https://pbs.twimg.com/media/G29AyECbwAIODD-?format=png&name=medium

3行で実装するため、面数を増やしております。

・IOとの3キー同時押し面を拝借させていただいています。https://github.com/kirameister/keyboard_layouts/blob/main/README.md

・親指先行シフト面（ng SPACEを先に押し下げながら、1キーを単打かつ連続シフト可能。小書きのぁぃぅぇぉゃゅょゎヶヵ、押しにくい ーげうぉ、外来音などを収録）

・ほぼ使用しないですが、ng SQT同時押し面（超マイナー外来音、スラングなどを収録）

※記号などは完全に新下駄と互換性がありません。薙刀式編集モードの？！・（）で個人的に十分と感じたためです。

その他、薙刀式のvm同時押しでEnterなどを取り入れています。

句読点には　、や。だけでなくSPACEもつけています（句読点変換であり、そのまま次の文字を打てば変換確定する）。また一部編集モードを追加しています。

## なんで作ったの？
　ZMK Charlieplexing仕様のキーボード同時打鍵判定において、google IMEや紅皿といったソフト側での対応では同時押しの成功率が低くなり実用に耐えなかったためです。（S, I, Oキーと他キーの同時押しにおいて）

 一方eswai様のzmk薙刀式やこちらのように新下駄に改変したものは動作OKでした。
 
 また細かいところでは下記があります。
 
 google IMEでは同じ使い勝手の編集モードの実装が難しい。
 
   また追加した　単打の　げ　が;などの化文字になることがある。（;をげとなるよう指定したら、げになったり;になったり）
   
   またタイムアウト設定が破損した場合、バックアップを再度入れてかつPCを再起動する必要がある（verupなどにより今後再発する可能性がある）。
 
 紅皿はノートPCでは電源に接続していないなどパフォーマンスが低いモード時には誤打が多くなる。
 　
  さらにソフトを起動していないといけない。Microsoft store版では再起動すると都度同時押しタイミング再設定やファイルの指定など必要で面倒。
  
  なぜか性能がよいデスクトップパソコンだと紅皿にて誤変換が起きる。（qwertyを使用していない筆者の環境だけ？）

また上記のようなソフトでは　エクスプローラー, Microsoft teams, DeepLなどでの日本語入力が不安定になり実用面で不安になるのが頻繁に確認されます。

キーボードで完結できるのは便利であるし、PCを共有しても問題がなく、かな入力の生存戦略としてソフトもいつまで使用できるかという心配を減らせることも大きい。

またQMKよりもZMKのほうが初心者の目線からすると2か所いじるだけで様々なキーボードへの実装が容易です。

またマイコンのフラッシュ（メモリ）容量もpromicroみたいにカツカツなことがZMKのマイコンはほぼないです。
 

## Github Actionsでwin用のbuildのみ考えています。それ以外は未検証のためサポートできかねます。

[zmk-config](https://zmk.dev/docs/customization)のwest.ymlに2か所を追加
```
manifest:
  remotes:
    - name: zmkfirmware
      url-base: https://github.com/zmkfirmware
    - name: eswai# オリジナルの薙刀式を実装したいときはこちらを追加ください。
      url-base: https://github.com/eswai # オリジナルの薙刀式を実装したいときはこちらを追加ください。
    - name: hamehame2 # win3行新下駄を実装したい時はこちらを追加ください。
      url-base: https://github.com/hamehame2 # win3行新下駄を実装したい時はこちらを追加ください。
  projects:
    - name: zmk
      remote: zmkfirmware
      revision: main
      import: app/west.yml
    - name: zmk-naginata # オリジナルの薙刀式を実装したいときはこちらを追加ください。
      remote: eswai # オリジナルの薙刀式を実装したいときはこちらを追加ください。
      revision: main # オリジナルの薙刀式を実装したいときはこちらを追加ください。
    - name: zmk-naginata # win3行新下駄を実装したい時はこちらを追加ください。
      remote: hamehame2 # win3行新下駄を実装したい時はこちらを追加ください。
      revision: main # win3行新下駄を実装したい時はこちらを追加ください。
  self:
    path: config
```

config/boards/your_keyboard/your_keyboard.keymapに薙刀式のコンボとレイヤーを追加
```
#include <behaviors/naginata.dtsi>

/ {

    macros {
        ng_on: ng_on {
            compatible = "zmk,behavior-macro";
            #binding-cells = <0>;
            bindings
                = <&macro_tap &kp LANGUAGE_1 &kp INTERNATIONAL_4 &to 1>
                ;
        };
        ng_off: ng_off {
            compatible = "zmk,behavior-macro";
            #binding-cells = <0>;
            bindings
                = <&macro_tap &kp LANGUAGE_2 &kp INTERNATIONAL_5 &to 0>
                ;
        };
    };

    combos {
        compatible = "zmk,combos";
        combo_ng_on {
            timeout-ms = <300>;
            key-positions = <15 16>; // H, Jのキー位置に書き換えてください
            bindings = <&ng_on>;
            layers = <0>; // デフォルトレイヤーが0の前提
        };
        combo_ng_off {
            timeout-ms = <300>;
            key-positions = <13 14>; // F, Gのキー位置に書き換えてください
            bindings = <&ng_off>;
            layers = <0 1>; // デフォルトレイヤーが0、薙刀式レイヤーが1の前提
        };
    };


    keymap {
        compatible = "zmk,keymap";

        default_layer {
            bindings = <
            // +----------+----------+----------+----------+----------+----------+----------+----------+----------+----------
                &kp K      &kp D      &kp N      &kp F      &kp Q      &kp J      &kp BSPC   &kp R      &kp U      &kp P
                &kp W      &kp I      &kp S      &kp A      &kp G      &kp Y      &kp E      &kp T      &kp H      &kp B
                &kp Z      &kp X      &kp V      &kp C      &kp L      &kp M      &kp O      &kp COMMA  &kp DOT    &kp SLASH
                                      &kp LCMD   &mo LOWER  &mt LSHFT SPACE  &mt LSHFT ENTER  &mo RAISE  &kp RCTRL
                >;
        };

        // 薙刀式に必要な32キーを定義してください。ng SQTがアット位置に相当します。（げ、うぉ）を打つのに必要です。筆者はSEMIの隣であるコロン位置にng SQTを置いています。キー数が足りない場合は親指キーを活用ください。
        // &kpの代わりに&ngを使います。通常はデフォルトレイヤーの次に配置してください(レイヤー番号1)。
        naginata_layer { 
            bindings = <
            // +----------+----------+----------+----------+----------+----------+----------+----------+----------+----------
                &ng Q      &ng W      &ng E      &ng R      &ng T      &ng Y      &ng U      &ng I      &ng O      &ng P
                &ng A      &ng S      &ng D      &ng F      &ng G      &ng H      &ng J      &ng K      &ng L      &ng SEMI
                &ng Z      &ng X      &ng C      &ng V      &ng B      &ng N      &ng M      &ng COMMA  &ng DOT    &ng SLASH
                                      &kp LSHIFT &mo LOWER  &ng SPACE  &ng SQT  &mo RAISE  &kp RCTRL
                >;
        };
 
// 以下省略　一般に拡張した抽象的表現であり、イメージがつかない人向けに実際に実装した例として最後にurlを記載していますので参考にしてください。
```


## ローカルでbuildする場合の例　おそらくhttpsのアドレスを変更したらいけると思いますが未検証です。

```
git clone https://github.com/eswai/zmk-naginata.git
cd zmk/app

rm -rf build
west build -b seeeduino_xiao_ble -- -DSHIELD=your_keyboard_left -DZMK_CONFIG="/Users/foo/zmk-config/config" -DZMK_EXTRA_MODULES="/Users/foo/zmk-naginata"
cp build/zephyr/zmk.uf2 ~/zmk_left.uf2

# 分割なら右手も
rm -rf build
west build -b seeeduino_xiao_ble -- -DSHIELD=your_keyboard_right -DZMK_CONFIG="/Users/foo/zmk-config/config" -DZMK_EXTRA_MODULES="/Users/foo/zmk-naginata"
cp build/zephyr/zmk.uf2 ~/zmk_right.uf2
```

## 編集モードへの対応
本家eswai様のほうではMacやLinux環境では実装が完了しているのですが、

winのみ、wicomposeによる実装たいへん面倒でが未完了のため、？！・（）以外の記号類を出せていません。

無改造だと編集モードが でぃ？！・くらいの動作で、他はwinの場合中身が空のマクロになっていた関係で内容をいじってしまっていますのでオリジナルの編集モードとも異なります。

nagifuncをいじってしまっているので、MacやLinuxの方は下記のほかに、nagifuncはeswai様のにしてもらう必要があります。

 そのためこちらをフォークしてもらいnagifuncを編集するか、eswai様のをフォークして代わりにこちらのキービヘイビア内容をコピペしてください。

Macでは下の対応をすることによって編集モードで記号入力を行うことができます。

* Macの「Unicode 16進数入力」を入力ソースに追加する
* Karabiner-Elementsをインストール
* unicode_hex_input_switcher.jsonをKarabiner-Elementsで有効にする


## 改変すると、こちらのフローが失敗するように見えますが形式上の呼び出しエラーに関するもので、キーボードへの実装と動作自体は問題なく動作するのを確認済みです。

下記はZaruBall V3の場合です。フローは成功しているのがお判りになると思います。具体例があった方初心者に参考になると思いますのでリンクになります。

https://github.com/hamehame2/zmk-config-ZaruBall/actions

https://github.com/hamehame2/zmk-config-ZaruBall/blob/ZaruBall-v3xgeta/config/west.yml

https://github.com/hamehame2/zmk-config-ZaruBall/blob/ZaruBall-v3xgeta/config/ZaruBall.keymap

下記ではOSがJISでkot様のJISシフトも導入しているキーボードですがかっこ（）「」や記号の？！％～において問題はありませんでした。

https://github.com/hamehame2/Jashine118bts/blob/main/config/west.yml

https://github.com/hamehame2/Jashine118bts/blob/main/config/keymap.keymap

//まだOSがUSレイアウトだったらとか、JISだけどzmkをJIS記号に置き換えたコードの場合はどうなるかは検証できておりません。

//→kot様のJISシフトは問題ありませんでした。

筆者は習い始めたばかりでタイピングが早くありません。タイパーを基準に考えておりません。

新配列は楽さを求めて導入すると思いますが、何が楽と考え何がつらい思うかは人によって異なると思います。

新下駄自身はそこそこの配置面を覚えれば、楽に早くうてるのが特徴です（特に拗音）。

個人的には外来音を2モーラで打つことや、変則的な同時押し、頻繁な親指シフト使用、特定指の使用率が高すぎる偏り、小指の遠い位置のキーが楽ではないと感じます。

QwertyのところでいうQ@位置の、げや長音"ー" が遠いのが私は苦でシフトを使用するように改変をしております。（本当は単打入れ替えをして　ーと。を入れ替えたい　もしくは。をシフト裏にしたいが、他の人が困るので単打の変更はしていません）

自作キーボードのBackSpace位置はキーマップで選ぶ仕様であり、親指に変えている人もいるくらいですので　げを@位置に固執する必要もないと思います。

親指先行シフト面のng SPACEや、ng SQTは　場所をどこに入れるかをユーザーが選べます。

そもそも入れないということも簡単にできるのでSKKIMEとかカーソル操作とかマウス操作とかIME文字設定を入れたい人にも悪影響はないと思います。

このためシフト面などは不要と思われるかたは各自のキーボード側のキーレイアウト側を変更ください。

筆者はあまりキーボードに詳しくないですが、eswai様にng SQTを足せる旨をご教示いただき本実装にこぎつけました。

この場をお借りして改めてeswai様に御礼申し上げます。またスレにてYuuki Umeta様にもサポートいただき有難うございました。

また新下駄を作成、公開いただいたkouy様にも感謝をしております。

 今後とも動画のuploadやSNS等で新下駄界隈を盛り上げていただき、ますます新下駄の発展を願っております。

本MDはZMK新下駄のZaruBall V3にて執筆しました。（英数部分は論理配列をDvorak系統のBoo配列を使用して入力しました）
