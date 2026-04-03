# ZMK 薙刀式を改変したwinJIS向けメジロ式です。QMK版v2.6.1に寄せています。

ZMK FirmwareでwinJIS向けメジロ式の入力を実現します。

メジロ式v2はじーびす/ʐiːbɪs/@jeebis_iox氏が考案されたソクタイプ日本語速記かな入力方式です。

ZMKでの薙刀式実装はeswai氏が開発されており、こちらは改変させていただいたものになります。

本家のメジロ式はNKROのキーボードかつploverが必要と敷居が高い。

またQMK版は取っ付きにくかったりMejiro31が人気で手にしづらいなどがあること、また無線では実現ができないことがありました。

プロジェクト(モジュール)取り込み式ですのでキーボードを問わないのが利点ですが物理配列としてたとえばcvb位置にntkとして並べると横一直線となります。

　この場合、隣同士のcv(nt) vb(tk)は親指で打てるものの cb(nk)とcvb(ntk)の打鍵が厳しい事が考えられるため-ntk、-nk、ntk-、nk-を押したことにするキーを追加しています。

　　　具体的には左ntkはng Z, 左nkはng X, 右nkはng DOT, 右ntkはng SLASHをそれぞれ指定できます。

頻出する左の小指2キー同時押しがやりにくいよという人もいると思うので -S*を押したことにしたキーも追加しました。(ng SQTを指定する)

 なので必要に応じてng QWERTYに、ng SQTを足したキーマップを指定してください。※キーマップにz,x,DOT,SLASH,SQTがなくても動作します。

薙刀式同様に、始動キー終了キーそれぞれ２キーのコンボ組み合わせを指定したものでON/OFFを切り替えします。

コンボはonとoff２パターン必要でレイヤーと日英を切り替えます。

同じ押し指パターンのトグル切り替え方式ではなく、ONでは日本語(IMEON)かつメジロレイヤーに移動します、OFFでは英語(IMEOFF)でベースレイヤーにする1wayのto方式の切り替えです。

この指定キー、及びレイヤーは後述のようにユーザーがキーマップ上で指定する必要があります。（何故かというとFG, JKの位置にしたくてもキーボードによってはこれらの位置が変わってしまう為位置と行き来したいレイヤー番号を教えて上げる必要がある為です。）

zmkのエコシステムのため、プロジェクトurlや名前の入れ替えでキーマップはそのままでzmkメジロ式v2からzmk新下駄にそのまま変更できる互換性があります。

ただしは薙刀式にする場合はng SQTをキーマップから削除するか、フォークした薙刀式にng SQT定義を追加してください。最初からng SQTを使用しなければ3つのプロジェクトは同じキーマップで互換性があります。

## IMEONにすると送信が変になる方向け

　ローマ字のつづりとしてはIMEOFFでは送信が早くてもうまくいくのですが、IMEONの場合IME側の変換が追いつかなくて変な出力と見えることがあります。

　　この場合は#define MEJIRO_ROMA_KEY_DELAY_MS 25の数字部分の25がキー送信単位のディレイ時間(25msec)ですのでフォークしてこちらの数字を増減させて調整してください。

https://github.com/eswai/zmk-naginata

https://github.com/JEEBIS27/Plover_Mejiro

https://github.com/JEEBIS27/qmk_firmware/tree/master/keyboards/jeebis/mejiro31

https://note.com/jeebis_keyboard

筆者Twitterアカウント:herm@PTclown

下記はキーマップ例です。基本的にはなんでもいいですのでntkとか打ちやすいところにおいてください。ngキーは重複して配置や押しても問題はありません。

https://pbs.twimg.com/media/HEqrSdGbsAAcdJF?format=png&name=large

https://pbs.twimg.com/media/HEjukrsbEAAmpLA?format=png&name=large


## なんで作ったの？
　ZMK Charlieplexing仕様のキーボード同時打鍵判定や、24やNKRO設定しても成功率が低くなり実用に耐えなかったためです。

キーボードで完結できるのは便利であるし、PCを共有しても問題がなく、かな入力の生存戦略としてソフトもいつまで使用できるかという心配を減らせることも大きい。

またQMKよりもZMKのほうが初心者の目線からすると2ファイルをいじるだけで様々なキーボードへの実装が容易です。

またマイコンのフラッシュ（メモリ）容量もpromicroみたいにカツカツなことがZMKのマイコンはほぼないです。
 

## Github Actionsでwin用のbuildのみ考えています。それ以外は未検証のためサポートできかねます。

[zmk-config](https://zmk.dev/docs/customization)のwest.ymlに2か所を追加
```
manifest:
  remotes:
    - name: zmkfirmware
      url-base: https://github.com/zmkfirmware
    - name: hamehame2
      url-base: https://github.com/hamehame2
  projects:
    - name: zmk
      remote: zmkfirmware
      revision: v0.3 #v0.3と0.2で行けています。最新版などキーボード自体がそもそも安定しないため、まずは取り込み無しで問題ない状態から追加をしてください。場合によってはv指定が必要かもしれません。
      import: app/west.yml
    - name: zmk-behavior-mejiro
      remote: hamehame2
      revision: main
  self:
    path: config
```

config/boards/your_keyboard/your_keyboard.keymapに薙刀式のdstiインクルードとコンボとレイヤーと追加
```
#include <behaviors/naginata.dtsi>

/ {

    macros {
        ng_on: ng_on {
            compatible = "zmk,behavior-macro";
            #binding-cells = <0>;
            bindings = <&macro_tap &kp LANGUAGE_1 &to 1>; //レイヤー１にしたい場合の例です。windowsの日本語の場合LANG1でいいです。to 1でなく&to NAGINATAとかユーザー側で命名してあるレイヤー名でも大丈夫です。
        };
        ng_off: ng_off {
            compatible = "zmk,behavior-macro";
            #binding-cells = <0>;
            bindings = <&macro_tap &kp LANGUAGE_2 &to 0>; //レイヤー0が通常ベースのアルファベットレイヤーだと思いますが、必要に応じて変更してください。to 1でなく&to BASEなどユーザー側で命名してあるレイヤー名でも大丈夫です。
        };
    };

    combos {
        compatible = "zmk,combos";
        combo_ng_on {
            timeout-ms = <300>;
            key-positions = <15 16>; // 左上を0とした場合の起点にしたいキー位置２つに書き換えてください。要はマイコンから見たキー番号です。
            bindings = <&ng_on>;
            layers = <0>; // デフォルトレイヤーが0の前提。もしデフォルト以外のレイヤー2や3からでも移行したい場合は<0 2 3>のように増やしてください。
        };
        combo_ng_off {
            timeout-ms = <300>;
            key-positions = <13 14>; // 左上を０とした場合の起点にしたいキー位置２つに書き換えてください。要はマイコンから見たキー番号です。
            bindings = <&ng_off>;
            layers = <0 1>; // デフォルトレイヤーが0、メジロ式（薙刀式）レイヤーが1の前提。レイヤーが不一致していると、押しても反応しません。
        };
    };


    keymap {
        compatible = "zmk,keymap";

        default_layer {
            bindings = <
            // +----------+----------+----------+----------+----------+----------+----------+----------+----------+----------
                &kp Q      &kp W      &kp E      &kp R      &kp T      &kp Y      &kp U      &kp I      &kp O      &kp P
                &kp A      &kp S      &kp D      &kp F      &kp G      &kp H      &kp J      &kp K      &kp L      &kp SEMI   kp xxxx
                &kp Z      &kp X      &kp C      &kp V      &kp B      &kp N      &kp M      &kp COMMA  &kp DOT    &kp SLASH
                                      &kp LCMD   &mo LOWER  &mt LSHFT SPACE  &mt LSHFT ENTER  &mo RAISE  &kp RCTRL
                >;
        };

        // メジロ式に必要なng キーを定義してください。
        // &kpの代わりに&ngを使います。通常はデフォルトレイヤーの次に配置してください(レイヤー番号1)。
        naginata_layer { 
            bindings = <
            // +----------+----------+----------+----------+----------+----------+----------+----------+----------+----------
                &ng Q      &ng W      &ng E      &ng R      &ng T      &ng Y      &ng U      &ng I      &ng O      &ng P
                &ng A      &ng S      &ng D      &ng F      &ng G      &ng H      &ng J      &ng K      &ng L      &ng SEMI    &ng SQT
                &ng Z      &ng X      &ng C      &ng V      &ng B      &ng N      &ng M      &ng COMMA  &ng DOT    &ng SLASH
                >;
        };
 
// 以下省略　一般に拡張した抽象的表現であり、イメージがつかない人向けに実際に実装した例として最後にurlを記載していますので参考にしてください。
```


## ローカルでbuildする場合の例　おそらくhttpsのアドレスを変更したらいけると思いますが未検証です。

```
git clone https://github.com/hamehame2/zmk-behavior-mejiro.git
cd zmk/app

rm -rf build
west build -b seeeduino_xiao_ble -- -DSHIELD=your_keyboard_left -DZMK_CONFIG="/Users/foo/zmk-config/config" -DZMK_EXTRA_MODULES="/Users/foo/zmk-behavior-mejiro"
cp build/zephyr/zmk.uf2 ~/zmk_left.uf2

# 分割なら右手も
rm -rf build
west build -b seeeduino_xiao_ble -- -DSHIELD=your_keyboard_right -DZMK_CONFIG="/Users/foo/zmk-config/config" -DZMK_EXTRA_MODULES="/Users/foo/zmk-behavior-mejiro/"
cp build/zephyr/zmk.uf2 ~/zmk_right.uf2
```



## 改変すると、こちらのzmk-behavior-mejiroフローが失敗するように見えますが形式上の呼び出しエラーに関するもので、キーボードへの実装と動作自体は問題なく動作するのを確認済みです。

下記はZaruBall V3の場合です。具体例があった方初心者に参考になると思いますのでリンクになります。

https://github.com/hamehame2/zmk-config-ZaruBall/blob/v3x260218/config/west.yml

https://github.com/hamehame2/zmk-config-ZaruBall/blob/v3x260218/config/ZaruBall.keymap

下記ではOSがJISでkot様のJISシフトも導入しているキーボードですがかっこ（）「」や記号の？！％～において問題がないかは今後検証してまいります。→問題なさそうです。

https://github.com/hamehame2/Jashine118bts/blob/mejiro/config/west.yml

https://github.com/hamehame2/Jashine118bts/blob/mejiro/config/keymap.keymap

筆者は習い始めたばかりでタイピングが早くありません。タイパーを基準に考えておりません。

新配列は楽さを求めて導入すると思いますが、何が楽と考え何がつらい思うかは人によって異なると思います。

メジロ式はモーラあたり最速（省力化）でうてるのが特徴です。

負荷（荷重）ベースではどうなのか長距離走に向くのかはわかっておりませんので一番楽という表現は現時点では避けます。

個人的にはカタカナ語やネットミームなどは苦手かもしれない？と勝手に思っております。

IAUやS#-もできるのですが、今回は見送っています。

当然他の新規ストロークIDを作ってカタカナ語をやりやすくというのも考えられますが、じーびす様がすでに検討されているとのことでとくに追加IDによる変換は付与しません。

Mejiro31の数字レイヤーの方はキーボード側で実装もしくは、別のビヘイビアでやるほうが良いと思うので実装していません。Geminiレイヤーも同様に考えていません。

筆者はあまりキーボードに詳しくないですが、じーびす様にご教示いただき本実装にこぎつけました。

一部動作が異なる、verupに対応していないなどご不便をおかけするかもしれませんが、ご容赦ください。

まだすべてのストロークやコマンドのデバックができているわけではありません。

この場をお借りして改めてeswai様とじーびす様に御礼申し上げます。

今後ともますますメジロ式の発展を願っております。

versionがいろいろ出ることが今後予想されますので、過去の版は残して保管、最新を反映させるためにも下記は必要と思います。

最新版は２つファイル名違いで（src/behaviorの中に　behavior_naginata.c、ver名や日付など記載したの）を残して最新と保管の運用ができたらと思います。

src/behaviors/behavior_naginata.cが実行ファイルで、そちら以外は同じフォルダにあっても動作には取り込まれません。

herm@PTclown  2026/Mar/29
