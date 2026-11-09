# テスト

## 一括実行

```sh
python3 scripts/run-tests.py            # すべて
python3 scripts/run-tests.py --kind cpp --sanitize   # C++ を ASan/UBSan 付きで
python3 scripts/run-tests.py deadzone bank           # ファイル名の部分一致で選択
python3 scripts/run-tests.py --list
```

SteamVR・導入済みリリース・利用者の設定には一切触れません。一時ディレクトリでビルドし、終了時に削除します。

| 種別 | 対象 | 実行環境 |
|---|---|---|
| `cpp` | `src/**/*-test.cpp`（各ファイルが独立した実行ファイル） | Linux の g++（C++17）。x86-64・Frame ARM64 のどちらでも可 |
| `py` | `scripts/test-*.py`（unittest、外部プロセスはモック） | Python 3.9 以上 |
| `sh` | `scripts/test-*.sh` | 下記の実機フィクスチャが必要。既定ではSKIP |

- C++ テストは `assert` で判定するため、`-UNDEBUG -O0 -Werror` で必ずビルドします。`NDEBUG` 付きでビルドすると判定が消えるので、手でビルドする場合も注意してください。
- 機能フラグで分岐するヘッダを使うテストは、先頭行の `// test-flags: -D...` でフラグを指定します。
- `test-frame-release-bootstrap.sh` は、staged release の tarball を `--bootstrap-archive` で渡したときだけ実行します。`$HOME` を一時ディレクトリに差し替えるので、実際の導入には触れません。

## 前提

- Python: 配布スクリプトは 3.11 以上が必要です（`launch-frame-release.py` の `hashlib.file_digest`。ほかに `Path.is_relative_to` 3.9、`unlink(missing_ok=True)` 3.8）。Frame の Python は 3.14。
- ビルド: `scripts/build-frame-finger.sh` は Frame 上で `/opt/steamvr` の OpenVR ヘッダと `libopenvr_api`、`pkg-config cairo` を使います。テストだけなら不要です。

## 回帰基準（v0.1.0, 2026-11-06, Frame aarch64 / g++ 15.2.1 / Python 3.14.3）

- `run-tests.py`: PASS=29（C++ 26, Python 3）, SKIP=1。`--kind cpp --sanitize` でも C++ 26 本すべて PASS。
- `FRAME_FINGER_RELEASE=1 FRAME_FINGER_RUNTIME=november-beta sh scripts/build-frame-finger.sh` の3バイナリが、導入済み v0.1.0 とバイト一致:
  - `driver_frame_finger_standalone.so` `d3da77a2c7e4c5535d856c106364d6fb5df3ddcc4a0a2b13c289d002382f84bf`
  - `frame-finger-settings-beta` `4fc38539487551bef015e861032a692afa3dfbaf063d34192aec7d4c098d2866`
  - `frame-release-dashboard` `f7c3769ed4d1ac6a4dd707091d55be8a2299f7c13c95acacb1ac190c88c5770a`
- 構造検証は `FRAME_COMPAT_STRUCTURAL_AUDIT=1` を付けたビルドだけで有効になります（既定はOFFで、上の一致は既定ビルドのもの）。

## 特性テスト

現在の挙動を固定するテストです。失敗した場合は、挙動の変更が意図したものかを確認し、意図した変更ならテストも一緒に更新します。

- `finger-deadzone-characterization-test.cpp`
  - 学習の最初のサンプルがそのまま基準値になる（初回外れ値の影響が学習窓全体に残る）
  - `dt > 0.25` のサンプルは捨てられる。最初のサンプルも同じ
  - 300ms 間隔の疎な更新では学習が進まず、50ms 間隔では進む（FingerBank 経由）
  - 手動モード出力のビット単位ダイジェスト（`0xb8f5ab0f953a0230`、`-O0`/`-O2` で一致）。Phase 4 でも変えない
- `runtime-compat-check-test.cpp` / `runtime-compat-check-nov-test.cpp`（両プロファイル）
  - 既知判定は v0.1.0 と同じ条件だけで決まる。追加の構造所見は診断扱いで、判定を変えない
  - 既知ハッシュの組なのに配置・指紋が違えば拒否し、構造検証には回さない
  - 未知ハッシュは、構造チェックをすべて通り、左右8指がそれぞれ1つずつ識別できたときだけ「構造検証合格」。どの条件が欠けても拒否
  - 合成メモリ配置は、Frame の実 vrserver の `/proc/<pid>/maps` で確認した配置に合わせている（2026-11-06、November beta）
- `frame-settings-compat-test.cpp`
  - v0.1.0 が書き出す `preferences.txt` のバイト列
  - 4096 バイトは受け付け、4097 バイトは拒否（文字列・ファイルの両方）

## ドライバ結合テスト（SteamVR 不要）

- `src/diagnostics/runtime-facts-test.cpp`: 実プロセスに対する事実収集（`/proc/self/maps`、RELRO 上のテーブル、読めないポインタで失敗側に倒れること、sha256sum 出力の解析、ログ行の書式）
- `src/diagnostics/driver-unknown-build-test.cpp`: 構造検証付きのリリース版ドライバ本体を、偽の駆動コンテキストで Init/RunFrame する。テストプロセスは vrserver ではないので未知ビルド扱いになり、判定ログがちょうど1行出ること、入力テーブルが書き換わらず関数も呼ばれないこと、観測・補正の経路が動かないこと、構造検証に合格する前は backend を辿らないこと、再 Init で最初からやり直すことを確認する

## 実機確認の記録

- 2026-11-06, Frame, SteamVR November beta（vrserver `430c259a…`）。構造検証付きビルド（リリースID `5d9d69aa…`、ドライバ `067f3308…`）を通常の stage で導入し、SteamVR を再起動して補正を ON/OFF した。
  - 既知ゲートを通過した直後に `FrameCompat: known_gate_pass verdict=known reason=none structural=none profile=0 table_rva=0x5ce620 entry_rva=0x77960 target_rva=0x9f788 slot_delta=-24,-48,-16` が出た。新しい判定関数も既知と判定し、追加した構造チェックにも所見はなかった。
  - 補正は利用者の目視で正常に動作した。
  - 確認後、退避しておいた v0.1.0 のリリースフォルダ（`29d2851c…`）を stage し直して元に戻した。
- 未確認: 本物の vrserver 内での8指の数え上げ（未知ビルドの経路でしか動かない）、実際の未知ビルドでの判定結果。
- 注意: 公開版 v0.1.0 の `.py` と `LICENSE` は CRLF 改行で配布されている（`OPENVR-LICENSE` だけ LF）。Windows の作業ツリーからパッケージを作ると改行が変わってリリースIDが変わるので、Frame への転送には `git -c core.autocrlf=false archive` を使う。

## Phase 2 and 3 groundwork

The observation policy is a pure, clock-injected state machine tested with simulated calls. It requires structural pass and explicit consent, counts three successful finite samples for each of eight distinct handles, requires separate activation, rejects a changed receiver or identity and enforces a 15-second observation deadline and a single trial per instance. It is not connected to a live hook.

Build with FRAME_COMPAT_STRUCTURAL_AUDIT=1 and FRAME_COMPAT_OBSERVATION_UI=1 to include diagnostic UI and bounded slot restoration. The UI shows the read-only structural verdict or rejection code and reports an unconfirmed restoration. It does not send observation or activation requests. The native driver retains the existing known-build gate; the installer and agent do not yet admit unknown builds. No live SteamVR changes were made.

On Frame ARM64, the test runner reported PASS=32 and SKIP=1 (bootstrap fixture not supplied). Both build configurations compiled; default release binaries matched the three v0.1.0 SHA-256 values. On-device diagnostic rendering and runtime restoration remain unverified.

## Phase 2–3 integrated experimental path

This supersedes the groundwork status above. The normal release build is unchanged. To build the experimental variant:

```sh
FRAME_FINGER_RELEASE=1 FRAME_FINGER_RUNTIME=november-beta \
FRAME_COMPAT_STRUCTURAL_AUDIT=1 FRAME_COMPAT_OBSERVATION_UI=1 \
sh scripts/build-frame-finger.sh experimental
```

The extracted experimental package can be staged with `sh install.sh --dashboard --driver --experimental`. This flag permits registration on an unknown runtime only for a package that contains the experimental driver and matching UI. It does not arm observation or correction. The normal package and default install remain fail-closed; the public bootstrap has not been changed.

On an unknown build, the native driver first runs Phase 1 read-only checks. The VR UI offers a two-click experimental consent step only after a positive structural result. The native driver rechecks runtime hashes, executable cv mapping, structure and eight unique input identities, and revalidates the request and UI heartbeat before attaching. The observer is limited to one attachment attempt per loaded module; it forwards values unchanged until three successful samples per finger have been seen within 15 seconds. The user must separately press enable after verification; a pre-verification activation request is rejected. Authorization is scoped to the UI session and short-lived heartbeat and census. Close, stop, timeout, ownership change, invalid sample or failed original call revoke correction and lead to bounded restoration. A foreign slot value is not overwritten. The UI displays progress, refusal code and restart guidance when restoration cannot be confirmed.

Validation: ARM64 build passed for both variants. Full suite on the integrated variant: 33 pass, bootstrap fixture skipped. After adding the final pre-attach checks and installer test, the five relevant focused tests passed, including the native pass-through callback and mocked installer registry. Default release SHA-256 values still match v0.1.0 for all three binaries. No driver was registered or loaded, no application was replaced and SteamVR was not restarted.

Live validation remains open: (1) known-build regression with experimental flags; (2) actual unknown build read-only classification, with no hook before consent; (3) eight-finger progress and explicit ON/OFF on a structurally compatible unknown build; (4) timeout, close and disconnect without correction, then restored slot; (5) Japanese/English text and simulated restoration-failure warning. Do not intentionally corrupt the real vtable to test failure display.

Known-build live regression: the Phase 2–3 experimental package (driver SHA-256 109a26d3aa250b1337290e1a80b25ad35d114063a3a0c0f71dcf33346eee7676) was staged with preferences preserved and correction initially OFF. After the user restarted Frame SteamVR, vrserver PID 42808 loaded the trial; the native log reported known_gate_pass / verdict=known and the status detected all eight finger paths. The user confirmed that correction worked after enabling it. This exercises the known-build route only; the unknown-build observation route has not run on hardware. The prior release and preferences are backed up in ~/ffc-dev/backup/phase23-live-rgxxahhn.

## Phase 4: Dynamic learning

The old characterization reproduced the sparse-event problem: a cached 0.30 seed plus the 0.02 margin gave a 0.32 deadzone; 300 ms updates were ignored, so an input of 0.315 mapped to 0. The production finger bank now gathers up to 63 actual successful events within the existing window, including sparse updates. Four 0.10 samples produce a 0.12 deadzone and map 0.315 to approximately 0.222. A noisy sequence 0.90, 0.60, 0.61 uses median 0.61 rather than the first sample. Fewer than three valid samples keep the previous baseline. Cached values do not count. Each completed attempt emits one diagnostic line and a bounded Japanese/English UI result. The existing legacy primitive remains for the diagnostic paths and characterization. Manual-output digest remains 0xb8f5ab0f953a0230; preferences format is unchanged.

Frame ARM64: 35 tests passed including isolated bootstrap. Experimental build passed. Live learning and counters remain unverified. No deployment, registry change, restart or publication.

## Phase 5: Maintenance and performance

Run the synthetic single-thread forwarding benchmark with g++ -O2 -std=c++17 -pthread scripts/bench-frame-finger-bank.cpp -o bench-bank, then ./bench-bank. One million calls per case, mock original callback, no SteamVR. ARM64 Frame: 76.8–88.1 ns/call for the last matching row and 25.3–42.8 ns/call for unrelated input (three runs). WSL x86-64: 32.6–34.7 ns/call and 23.7–24.0 ns/call respectively (two successful runs). This does not measure live latency or contention; no evidence to justify replacing ownership or synchronization.

The release state already uses the account home; remaining absolute paths serve the historical diagnostic installation. No XDG migration or broader purge was introduced, so existing paths and settings remain readable. The installer's HOME override remains useful for isolated tests. The shipped build selects the correction path via macros; the bounded and supervised sources are also covered by tests. No speculative source relocation was made.

Clock audit: legacy file requests use wall seconds at both sender and freshness validation. Once accepted, monitor duration is measured with the monotonic clock. UI heartbeats, experimental commands and input authorization deadlines use monotonic milliseconds on both ends. Wall-clock status timestamps are for display and freshness only, not for authorizing a hook. No mixed-domain comparison was found in these routes.

Phase 4 live validation: the user ran LEARN on right pinky (row 7) in dynamic mode on the known November SteamVR build, vrserver PID 55030. The driver recorded 63 accepted samples (collection cap), result=applied; live learned mask was 128 with correction ON. This verifies the application of a learned baseline, not its visual effectiveness or the live insufficient-sample case. The latter passed synthetic tests. Phase 2–3 restore files remain in ~/ffc-dev/backup/phase45-live-2t3tnyhs.

After the right-pinky learning trial, the user reported that the learned correction appeared effective. Treat this as a subjective observation on one finger, not a measured before/after comparison or verification of all eight fingers.

Learning-feedback live regression: on vrserver PID 62004, the user saw the Japanese applied message after LEARN. Request and result IDs matched for right pinky (row 7), with 63 accepted samples and learned mask 128. Correction was ON at verification. Live insufficient-sample behavior remains unverified; the parser and baseline retention were tested synthetically. The prior installation is backed up in ~/ffc-dev/backup/feedback-live-40dylhq4.

Known-only release candidate live check: generation 92e91e28820abf5bf940dd8aa8005c9423697c8ce37b7809da7f3871356aad06 loaded in vrserver PID 66998 after a normal restart. Eight inputs were detected and correction began OFF; right-pinky dynamic settings were preserved. The user saw the Japanese learning-applied message. The request and result matched (row 7, 63 samples, applied), learned mask was 128 and correction was ON. This check used the candidate without experimental unknown-build observation. Live insufficient-sample display is still unverified.

Public v0.2.0 reproducibility check: Frame downloaded the source archive, ARM64 runtime archive, bootstrap and SHA256SUMS anonymously from GitHub. All three listed hashes matched. Rebuilding the attached source with FRAME_FINGER_RELEASE=1 FRAME_FINGER_RUNTIME=november-beta reproduced all three shipped binaries byte-for-byte; the four shipped Python scripts also matched the attached sources byte-for-byte. The installed release and SteamVR registry were not touched.

## 2026-11-08 unknown runtime startup readiness

Runtime hashes: vrserver `fd0857d05ba7ecfd186c588c7b9412724a725ddc1aae90c79743efdb4b66a678`, cv `5ea5fcb5b104ca49d937b154c8567d232bcc0ec4887cc78134e5336f623b171d`. Experimental driver loaded and rejected with `finger_missing`, mask `0xff`, after 12 startup polls. Left controller activation occurred about 7 seconds after that verdict. Other structural checks passed; no hook was installed. This is a startup-readiness failure, not proof that the new runtime supports correction. Extend only the read-only readiness census to at most 120 one-second polls. Structural mismatches still reject immediately; the one-attachment observation limit and correction gates are unchanged. Live validation of the extended wait remains pending. Backup: `~/ffc-dev/backup/unknown-nov08-f_j57e_g`.

Validation: extended-readiness experimental ARM64 build succeeded; all 31 C++ tests passed, including unknown-build rejection with untouched vtable and experimental forwarding. Candidate archive SHA-256: `d4696f90780e0b797d7f88a4f20ba4d8b7612895e5dd460add0b7ad30ef02d32`. Live installation of this revision is pending approval.

User requested unlimited readiness waiting instead of the proposed 120-second cap. The read-only census now waits without a deadline for missing or unstable controller inputs, at most once per second; the counter saturates instead of overflowing. Structural errors still reject immediately. The single attachment attempt, 15-second post-attachment observation deadline, explicit consent and separate activation are unchanged. This supersedes the 120-poll proposal above.

Unlimited-readiness revision: all 31 C++ tests passed and experimental ARM64 build succeeded. Added strict waiting-status parser tests and Japanese/English waiting UI. User approved deployment; selected generation `9791cfd1c9e2e2fc23796b6752c1b6f52324d872690b03289bfb9382a1006ec8`, driver SHA-256 `48a4752f96e2aaa1cef1c4c69cb35f7c50d55273b729ea833eaf42dd16b08451`, correction OFF. Backup: `~/ffc-dev/backup/unlimited-nov08-t75kj7q7`. Awaiting normal Frame restart; live verification pending.

Live validation of unknown build (2026-11-08): generation `9791cfd1...` loaded in vrserver PID 14690. Read-only structural census passed after 6 polls, with eight unique fingers (missing=0, duplicate=0), table RVA `0x5cf620`; no fixed profile was added. User explicitly started observation in the VR UI, the driver attached pass-through observation, then status reached `verified 255`. User separately enabled correction; fresh status was `active 255` and `FRAME_BETA_STATUS_1 ... ON 255 255 0`. The user confirmed correction worked in VRChat. This validates structural classification, real eight-finger observation and activation on this specific unknown runtime. It does not establish compatibility with arbitrary future builds, long-duration stability, disconnect/close restoration, or restoration-failure UI. Correction remains active at the user's request; no stop/disconnect was induced during this check. Public v0.2.0 remains known-build-only; this experimental revision has not been published.

## v0.3.0 public candidate

User requested the same ordinary ON/OFF operation for unknown builds and persistence of that setting. The UI now consumes its existing saved requested flag and automatically sequences Observe then Activate only after fresh native structural/verified status. OFF never initiates observation or activation; the native controller remains the final authority. The new pure UI-flow test covers saved ON, saved OFF, waiting, rejection, duplicate suppression, missing/stale-verification prerequisites and write-failure suppression. The native driver binary is unchanged from the live-tested unlimited-readiness version (`48a4752f96e2aaa1cef1c4c69cb35f7c50d55273b729ea833eaf42dd16b08451`). The new ordinary button flow and saved-ON startup were not separately exercised in VR during packaging.

ARM64 validation: 36 C++/Python test executables passed; the shell fixture initially skipped without an archive, then passed with the generated archive. Bootstrap now accepts the explicit --experimental install flag and rejects unknown flags or experimental registration without --driver. Isolated bootstrap test leaves the real installation and registry untouched. Release build succeeded. Full archive SHA-256: `ecd33b3af262b19c1191db1c216dab9e1584c259d6a610ee09535949baccd0ba`. Manual correction/settings compatibility tests remain passing. The installed app has not been replaced during publication work.

Published app-v0.3.0 preview from public commit `d83739ceabdc5d4e3f93cf9b113b859318a88ffc` on 2026-11-08. The final runtime package was rebuilt from that exact public source; both license documents were packaged with source-tree line endings, so its final SHA-256 supersedes the preliminary candidate hash above: `a882ae1b1b32cdcff8dc9fa8c15865a68ba098d06341bb90eb08dafa311ef343`. Source archive SHA-256 `561c388acd88a9ab5b66d628e3a85afa483abbee2416d301febe15178c89d043`; bootstrap `211b4690d99166c77d1f4472d1066dd0943108ad0c25603bef9992f3f06c7c86`. All nine runtime files match the exact tagged source/rebuild. Three focused ASan/UBSan executables and the final bootstrap fixture passed. After publication, anonymous downloads matched all four uploaded digests, automatic source ZIP contained implementation files, and isolated staging/check/purge passed. Verification files: `~/ffc-dev/v030-public-jl9b8d2i`. The live installed generation remains unchanged by this release work.

v0.3.0 published artifact live deployment: user approved replacing the installed candidate with the actual GitHub runtime archive. Selected generation `dfa3df8608454da169ec16ce28f3af4899e8c1d2a299e8caaa519844b527f8fd`; prior installation and full settings-ui state backed up in `~/ffc-dev/backup/public-v030-q5sy5hk0`. Driver bytes were identical, so its disk inode was preserved. Preferences and language were unchanged. After the user normally restarted Frame SteamVR, PID 21070 loaded the public app. Saved OFF was retained and no observation started until the ordinary correction ON click. Structural census passed with eight unique identities; automatic observation then reached active 255, native status ON 255 255 0. The user confirmed correction worked in VRChat. preferences.txt now contains `FRAME_FINGER_PROFILE_1 1`, confirming ON persistence. This supersedes the packaging-time limitation about the new ordinary-ON button flow. Saved-ON automatic startup after another restart remains unverified live; no extra restart was induced.

## v0.3.1 startup timeout fix candidate (2026-11-09)

The user reported Unsupported: timeout at startup with saved ON. Live PID 5072 passed structure/census and attached automatically at 20:51:15, then rejected timeout at 20:51:30. This shows missing timely samples, not a structural incompatibility; the logs do not identify which fingers were missing. Deterministic reproduction runs the new idle-start regression against the previous header and fails `idle.check(..., hour)` as expected.

Remove only the observation input-collection deadline. Observation remains pass-through until three successful finite [0,1] samples with a stable receiver have arrived for every one of the eight identified fingers. No partial activation. Native heartbeat/session/identity checks, invalid-value/result rejection, one attachment and bounded restoration remain unchanged. Progress now says input waiting/correction OFF, rather than treating silence as an unsupported runtime. Preferences and the ordinary ON/OFF flow are unchanged. Reason::Timeout remains parseable for historical status compatibility but is no longer emitted for observation silence.

Tests simulate an hour of no input followed by sparse per-finger events over a day; partial evidence cannot activate. Stop/restoration still works after 24 hours, and bad result/value, session loss, identity change and clock reversal still reject after long waits. ARM64: 36 C++/Python executables pass; isolated bootstrap staging/check/purge passes; four focused ASan/UBSan executables pass. The old-header regression fails as expected, then passes with the fix. Build/package succeeds. Candidate runtime archive SHA-256 `f9d3cd5fa794df39a4adf2acb64288cb2236c4c0a973ca297d269f9edc3c773f`, driver `6196c396cfe7969ba752157798e2caf0727544374c9657d5ce4cbffd4f23f072`. Live replacement and restart remain pending approval; no installed files changed and no publication performed.

User approved deployment of the v0.3.1 startup fix. Selected generation `c9585567efa1e2f6669307ba6494c487fb250eeebcf1a71f0c10d95a24e80958`, replacing public v0.3.0 generation `dfa3df86...`. Archive and all payload digests verified; launcher check passed. Stable driver replaced atomically, preferences and language unchanged, saved requested flag remains ON. Backup: `~/ffc-dev/backup/v031-startup-fix-4wn_4s3a`. Awaiting user normal Frame restart; this is deployment verification, not evidence the new driver has loaded or that startup silence works live. No SteamVR restart performed by the agent.

## v0.3.1 route-verification revision (2026-11-09)

The first startup-timeout fix loaded after the user's restart (PID 9750), with saved ON preserved, fresh active 255/native ON 255 255 0 and no startup timeout. Exact sample-arrival timing during the requested idle period was not captured. The user then requested removal of deliberate finger preparation, distinguishing route verification from explicit baseline learning.

Superseding the all-eight/three-samples runtime gate: structural census still requires eight unique finger identities, but a single successful finite normalized ordinary call now verifies the common scalar route. Native activation remains separate and follows the saved/requested ON. Correction is independently gated per component/receiver until that finger's own first raw success. No value change, open-hand pose or all-finger event barrier. Each first ordinary call keeps its original arguments; no synthetic command, retained-value replay or firmware/calibration operation. Receiver priming uses the bank with mapping and learning explicitly disabled, so the next ordinary call may map without a second preparation event. Known-build callers use the existing default mapping behavior. Invalid values/results, receiver/session/identity changes, lease expiry and slot restoration remain checked.

Tests: constant value input for one finger activates/corrects without any other finger events; a newly seen finger's first call is raw, its next may map. Original receiver/component/offset and exact call count are checked; error return preserved; unknown handles and expired lease pass through; invalid value revokes permission. Bank test proves raw observation neither maps nor collects learning samples even if a control profile requests them. Manual digest and existing learning tests pass. Full ARM64 suite: 36 executables PASS (shell fixture separately PASS); focused ASan/UBSan four executables PASS, then expanded bank test PASS again. Experimental build/package succeeded. Candidate archive `cf87ac46d557552fd382a2ef1fb8c89b27f37766e2c27cf46302614025455b09`, driver `3adbe4c29d4ae7dc9bb1b14cbb86ec5c0825af3a00f86de126769243984fd605`. Installed files unchanged by this revision; awaiting live replacement consent. No publication performed.

User approved deployment of the route-verification revision. Verified all payload hashes and selected generation `fc38b61bbb771c7cdd3b0df0636caab4a6ba0283e54fd1c6681c35a928ccbc47`, previous `c9585567...`. Stable driver replaced atomically; launcher check passed. Preferences and language preserved byte-for-byte, saved correction ON retained. Backup: `~/ffc-dev/backup/v031-route-vrefvszb`. Awaiting user normal Frame restart and natural-use validation; no preparatory finger movement requested. No SteamVR restart or publication performed by the agent.

Live route-verification revision: after normal restart and opening the app without requested finger preparation, PID 14749 reported fresh `active 255` and `FRAME_BETA_STATUS_1 ... ON 255 255 0`. Selected generation `fc38b61b...`; saved ON retained. Structural census passed after seven polls with no missing/duplicate identities, then automatic observation attached. No timeout/rejection in this startup. This confirms native activation and all eight witnessed inputs during ordinary use; visual correction quality is not inferred from status and remains for user confirmation. No additional input command, restart or publication performed.
