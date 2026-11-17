# Aelthuvor — 獨立 Library OS 與 Application SDK

[English](README.md) · [简体中文](README.zh-CN.md)

Aelthuvor 是執行於 Liberums 上的 C11/i686 Ring 3 Library OS。
**核心保護資源，LibOS 管理資源。** 它不是核心的必要組成部分，其他 LibOS 可以完全不使用它。
授權條款為 GPL-3.0-or-later。

v1.0.0 是教學與開發基準：**Application API 1.0**，相容 **Liberums v1.0.0 / Kernel ABI 1.2**，
核心固定提交為 `7d5d574e8643c3645fa3ce2a126dcbd4ae43662f`。兩套版本系統彼此獨立。
Phase 6 完成 SDK 封裝、開發課程與維護交接。原始碼 Tag 不等於 GitHub Release；
託管資產的狀態以[實際發布頁](https://github.com/Evan-Windproof/Aelthuvor/releases)為準。

## 建置與開發

需要 Linux、支援 -m32/i686 的 GCC、GNU Binutils/Make、Python 3.12+。
目標為 freestanding ELF32，不需要 32 位元主機 libc。主機測試使用 ASan/UBSan；
整合測試需要 QEMU system i386 與支援 i386/Python 的 GDB。

```sh
make -j2
make profiles app-sdk apps independent-libos
make sdk-package
```

SDK 位於 build/app-sdk；壓縮檔位於 build/release/aelthuvor-sdk-1.0.0.tar.gz。
包含公開標頭、兩個靜態函式庫、四個 runtime、明確的啟動與進入點物件、連結腳本、建置規則、
來源紀錄、授權條款、範本、範例原始碼與教程。檔案中不含核心、虛擬機或無關的應用 ELF。
下載後先驗證 SHA256SUMS，再解壓縮：

```sh
export SDK=/absolute/path/to/aelthuvor-sdk-1.0.0
cp -R "$SDK/templates/application" my-app
make -C my-app AEL_SDK="$SDK"
make -C my-app AEL_SDK="$SDK" APP_POLICY=best
```

應用實作 ael_app_main，借用不透明的 Context，無須讀取私有原始碼或重新連結核心。
可選 APP_PROFILE=preempt|coop、APP_POLICY=first|best、APP_DEVICES=0|1；預設為 preempt/first/無裝置。
SDK 與專案路徑應避免空白。請參閱[十課教程](docs/application-development/README.md)、
[API 契約](docs/sdk-reference.md)與[建置指南](docs/build-guide.md)。

## 執行與機制

先在獨立的 Liberums 儲存庫建置固定版本核心。搶佔式應用的執行指令：

```sh
qemu-system-i386 -machine pc -accel tcg -cpu qemu32 -m 32M -smp 1 \
  -display none -serial stdio -monitor none -nic none -no-reboot -no-shutdown \
  -kernel /path/to/Liberums/build/liberums.elf -append multi-external \
  -initrd my-app/build/app.elf
```

合作式 ELF 使用 -append external。Console 需要 APP_DEVICES=1，以及可信任啟動設定授予的
multi-vga=1 multi-keyboard=1；連結裝置函式庫不會授予權限。新舊 Console 均保留
help/clear/about/status/memtest/exit。雲端驗證 QMP/PS2/IRQ1/VGA；GTK 與實體鍵盤仍需在本機人工驗證。
無顯示介面的 QEMU 輸出完成後仍會持續執行，可從另一個終端機結束程序。參見[GDB 除錯](docs/debugging-guide.md)。

First-fit/Best-fit 在 Page Pool 上管理對齊區塊、分割與合併。單次配置為 1..4096 位元組；
free 保留閒置 Arena，trim 釋放頁面並重試失敗的回復紀錄。閒置撤銷可以 ACK；
活躍或已失效的 Heap 採用 Fail-stop（96）；成功應用狀態下的洩漏或清理錯誤為 95。
check 無法鎖定下一次記憶體存取。

[Independent LibOS](reference/independent-libos/README.md) 是另一條路線，擁有自己的啟動、runtime、
syscall 與固定物件池，不連結 libael。它與 Ael 應用可在同一核心上以獨立 ELF/CR3 並存。

## 驗證與維護

```sh
make clean
make -j2
make profiles test sdk-tests app-sdk apps independent-libos
make package-tests
make integration LIBERUMS=../Liberums
make console-integration LIBERUMS=../Liberums
make phase4-integration LIBERUMS=../Liberums
make phase5-integration LIBERUMS=../Liberums
make phase6-integration LIBERUMS=../Liberums
```

保留 Phase 5 全部 27 個真實撤銷與審核 guest；Phase 6 驗證壓縮檔的外部使用者與全部課程原始碼。
歷史 ABI 1.1/1.2、舊 ael_main、新 ael_app_main、兩種策略與 Profile、可選 Console 均保留。
六個核心公開標頭保留原始來源提交與 SHA256。

[驗收](docs/phase6-validation.md) · [架構](docs/architecture.md) · [相容性](docs/compatibility.md) ·
[限制](docs/known-limitations.md) · [歷史](docs/history.md) · [維護交接](docs/maintainer-guide.md) ·
[更新紀錄](CHANGELOG.md) · [發布說明](RELEASE-NOTES.md)

正式 make release-package 要求乾淨且已提交的工作目錄，產出原始碼/SDK、Manifest、授權條款、說明與 SHA256SUMS。
歸檔中繼資料與 SDK 除錯路徑均已正規化，不承諾跨工具鏈 ELF 逐位元組一致。
產生的建置檔案不提交，原儲存庫由維護者接管。

這是教學與開發基準，不是正式環境安全或無限執行時間認證。單 CPU i686、靜態 ELF32、有限資源；
沒有透明指標復原、POSIX、網路、檔案系統、spawn/IPC、動態連結、SMP、執行緒或 GUI。
未進行小時/天級 soak 或實體鍵盤驗證。Phase 6 交接後停止自動開發，未來架構由維護者決定。
