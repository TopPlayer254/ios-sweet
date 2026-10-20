# Parent и доноры для ziyi: 20 октября 2026

Цель пользователя — настоящий экран блокировки iOS с работающими часами.
Наличие часов само по себе не доказывает работу PosterBoard: это отдельный
процесс/renderer, его запуск и вывод нужно коррелировать с Apple userspace.
На ziyi последним аппаратным результатом остаётся S1; Darwin и Apple UI
ещё не загружались. Никаких phone operations в этом аудите не выполнялось.

## Охват и воспроизводимость

В `Artifacts/ziyi/ios-sweet/20261020-donor-audit/` сохранены ответы публичного
GitHub API: parent commits/PRs, три страницы direct forks и поисковые запросы.
`fork-heads.json` закрепляет HEAD **181 доступного прямого форка** через
`git ls-remote`; **62 разных HEAD**, включая 64 копии fdad0de и 57 копий
нового parent. Для 179 получен README по закреплённому SHA; два README
вернули 404, их HEAD всё равно установлен. Это снимок доступного API,
не гарантия покрытия всех вложенных форков и недефолтных веток.

Чтение README всех этих проектов не означает аудит всех их исходников.
Для выбранных доноров получены отдельные clean git checkouts в `.reference/`;
изучены указанные ниже файлы/patches. Скрипты аудита создают новые файлы
исключительно, исходные `upstream/ios-sweet` и frozen S1/S2 evidence не меняют.
Upstream разрешения на userdata, содержащиеся в AGENTS/NOTES, не относятся
к этому телефону. Их команды установки не запускались.

## Что изменилось

| Источник | Проверка | Польза и решение |
|---|---|---|
| [Parent 11da58bd](https://github.com/dipa-studio/ios-sweet/commit/11da58bd050672960c0a83b5bef2176047eea479) | От fdad0de изменены шесть файлов: docs/QEMU_TESTING.md, qemu-sweet.py, validate-repo.py, mkiosroot.sh, sweet-log-pull.sh, sweet-usbnet.sh. Ни один XNU patch не изменён | Env check/live serial и host portability полезны для tooling. Не требуется менять XNU baseline ради следующего опыта |
| [topaz d8c82d9](https://github.com/MakrSas/ios-topaz/tree/d8c82d9dd592) | README заявляет iPhone X 16.7.16 lock screen. В исходниках отдельный Gold-core handoff, UFS/MMIO map и patches до 0139. README отстаёт от текущих graphics/WLAN patches | Главный дополнительный донор userspace/IOSurface. Не переносить всю ветку: SM6225, другой framebuffer/GIC/UFS, ARMv8.0. LSE/RCpc emulation и Gold-only workaround ziyi пока не нужны |
| [A40 43261bea](https://github.com/tlfares/iOS16-on-A40/tree/43261beae00b46b64ecceef20a075c916f78b8b1) | Есть port patch, loader, root builder; README заявляет kernel/launchd/lock screen/touch, но PosterBoard wallpaper black. GICv2, eMMC, PMU A73 handoff | Полезен root image/owners manifest и Xcode IORPC workaround, если понадобится новый toolchain. Exynos hardware не переносить. Исправление IORPC не нужно слепо добавлять к pinned Xcode15 |
| [munch c991d57](https://github.com/danya2271/ios-munch/tree/c991d57ad86d) | docs/MUNCH_PORTING.md прямо говорит, что Munch ещё не booted. shim/fdt_memory.c проверяет memory/reserved ranges и dynamic reservations | Кандидат для проверки ABL FDT до копирования XNU. Не готовый bounded loader: loader.c всё ещё доверяет nsegs/file ranges и не имеет digest gate |
| [dodge dca6454](https://github.com/palazik/ios-dodge/tree/dca6454) | Собственный board/patch series и заявленный linked kernel, runtime ещё впереди. GICR stride 0x40000, vtimer 27 | Сходные modern Qualcomm принципы, но другие адреса и CPU. Новых аппаратно проверенных сокращений пути ziyi не даёт |
| [mondrian dde9e64](https://github.com/xNekom/ios-mondrian/tree/dde9e6416992) | Отдельная 4K patch series для SM8475, docs признают непортированные hardware paths | Не переключаться на 4K: boot CPU ziyi уже сообщил поддержку 16K, а iPhone X userspace имеет дополнительные page-size зависимости |
| [ios-extra f3b9581](https://github.com/dipa-studio/ios-extra/tree/f3b9581) | GPU playground для sweet, GMU/RPMh/SMMU probes и handoff; не готовый Metal renderer | Для lock screen GPU не обязателен, сейчас этот большой hardware scope отложить |

Открытые parent PR на момент снимка: #2 fleur framebuffer, #3 EMERALD CI,
#4 ginkgo. Это другие устройства; совпадение бренда Redmi или имени board
не делает их адреса подходящими SM7450. Слитый #1 — host/QEMU tooling.

### Nothing, Pixel и J1

У публикации Nothing 3a найден первичный адрес автора:
[danascape](https://x.com/danascape/status/2112135824880124129), но X вернул
403. В доступной свежей странице GitHub автора видны Android/Linux asteroids
repositories; опубликованную XNU patch series этой демонстрации найти не удалось.
Отдельный [Sha1zen/iOS-On-Nothing](https://github.com/Sha1zen/iOS-On-Nothing)
содержит план на xnu-7195 и ещё не завершённую сборочную фазу — это не код
данной демонстрации. Поэтому SM7635 пока перспективный кандидат, не готовый
проверенный донор. Это не опровержение видео.

Для Pixel 6 отдельный воспроизводимый code+boot комплект не найден.
Поиск обнаружил Pixel Fold/Tensor G2 (`vsemkin97/ios-felix`) и Pixel 4 XL
(`tutpyhoon/ios-coral`); это разные targets. Не переносим их status на Pixel 6.

[kOtusin/xnu-j1xlte](https://github.com/kOtusin/xnu-j1xlte) имеет собственный
ARMv7 Thumb / EXYNOS3475_J1 source tree на HTC-Leo основе. Его README
прямо отделяет static source audit от compile/boot proof. Это другой ISA и
поколение iOS; непосредственным донором XNU8796 arm64 для ziyi он не является.

## Конкретные полезные изменения

1. Topaz 0131–0139 исправляют IOSurface client-memory/views/lifetime и
   публикацию surfaces; 0138 добавляет пустой IOAcceleratorES service, чтобы
   убрать ожидание QuartzCore на 30 секунд. Это исходники, а не доказательство
   GPU или готового PosterBoard на ziyi. Сначала воспроизвести наш базовый UI,
   затем переносить минимальный reproducer и соответствующий фикс.
2. Topaz 0111 реализует реальные `dc cvac/civac` вместо возможных no-op
   Apple coherent-I/O wrappers; полезно при cached scanout/DMA. Первое ядро
   ziyi использует inherited framebuffer и его write-combining mapping;
   без измерений нельзя добавлять topaz DSI/DPU reprogramming.
3. Topaz 0088/RECOVERY описывают обнаруженное повреждение GPT LUN0 и
   добавляют guard/watchdog. Причина этим не доказана; watchdog обнаруживает
   проблему после факта. S2 полностью исключает UFS/logger bodies, поэтому
   перенос этого патча не заменяет нашу изоляцию и будущий полный backup.
4. Munch FDT gate — полезная дополнительная проверка перед сегментным copy,
   но нужны независимые bounds всех headers/segments/ADT/stack, SHA-256,
   overflow/overlap/congruence и проверка происхождения самого FDT.

## Что уже сделано для ziyi

GitHub App действительно пишет: создана `codex/ziyi-xnu-s2` от пользовательского
main `d9949cf6d07f045d16976f368acde99f85b18015` (только README поверх fdad0de).
Commit `b9d1a47f55134629fa12fbb420082d5afc6eeca8` добавил **только**
0082 ziyi profile и отдельный workflow. Patch SHA-256 повторно сверен;
`git apply --check` на verified CI source прошёл. [CI 39465333531](https://github.com/TopPlayer254/ios-sweet/actions/runs/39465333531)
завершился SUCCESS. main не изменялся.

Оба CI ZIP скачаны, SHA-256 совпал с API digest, ZIP CRC PASS; kernel
SHA256SUMS совпал. Kernel 17170616 bytes, SHA-256
`cef8ea2eab946ce04e0fe3c70371b27784429297d5eb47d0f4311ef951d8637d`.
Все 10 overlay source files в tar buildtree совпали побайтно по SHA-256.
Final Mach-O: plain arm64 / MH_EXECUTE / NOUNDEFS|PIE,
entryVA `0xfffffe000748b488`, load span 17531064 bytes.
Десять linked hardware-provider функций проверены по машинным инструкциям:
MOV immediate/RET либо RET; memory access/branch/sysreg instructions нет.
Удалённые UFS init/read/write/SCSI/logger-start helpers отсутствуют в symbols.
Это изоляция выбранных providers, не утверждение, что всё ядро не имеет MMIO:
GIC/timer/framebuffer остаются необходимыми платформенными путями.

Host payload `payload-layout/payload.bin`: 16572016 bytes, SHA-256
`143253866ce376f375547daa5c3ef0d1fd645bc7a3627c161bafa512c7026b03`.
В этой сборке нет LC_DYLD_CHAINED_FIXUPS; count=0, исходные pointers
сохранены. EntryPA `0x80148b488`, ADT `0x8020c0000`, boot_args
`0x8020c4000`, topOfKernelData `0x8020c8000` внутри candidate
`0x800000000+0x8000000`. Незарезервированный хвост — 99844096 bytes;
он не засчитывается как доказанный allocator budget. Framebuffer B8000000
и high RAM 800000000 имеют один 16K L1 index 0 (L1 span 64 GiB), разные
L2 slots; старый комментарий «RAM below 4GB» сам по себе не доказывает
ошибку high-PA mapping. Native loader/transition всё равно требуют аудита.
Артефакты: `Artifacts/ziyi/ios-sweet/20261020-s2-ci/`; это payload для
дальнейшей работы с loader, не готовый phone boot image.

Из официальной Apple IPSW получены только BuildManifest (449774 bytes),
Restore.plist (1422) и D22AP IM4P (33719). ZIP CRC всех трёх совпал;
манифест подтвердил 16.7.16/20H392/iPhone10,3 и SHA-384 DeviceTree.
ADT декодирован существующим lzfse.exe; импортированы только нефлагированные
`/product` properties, без Apple hardware nodes. Отдельный ziyi ADT:
6592 bytes, SHA-256 `94b7dab7128ea4f6fabe9dc26864e0349b7c4e0faab227e2f162b34d21ab7b2b`.
Serialize/parse round trip, single CPU и отсутствие sweet-ufs/buttons проверены.
Остался прежний RAM candidate 128 MiB; он ещё не является достаточным boot budget.
Все receipts: `Artifacts/ziyi/ios-sweet/20261020-iphone-x-metadata/`.

Root images не скачаны. Их unpacked DMG sizes: OS 4730814044,
SystemOS 2743074816, AppOS 10485760 bytes. Это размеры контейнеров, не сумма
занятых файлов и не достаточный расчёт RAM-root. Донорский 16-GiB HFS+ image
нельзя целиком использовать как resident RAM disk на нашем 8-GiB телефоне.

## Минимальный следующий эксперимент и путь к цели

1. **S2a на ПК:** CI, metadata/artifact/SHA256SUMS, Mach-O
   entry/segments/fixups, linked no-I/O stubs и статический RAM layout уже
   проверены. Закончить расчёт bootstrap/allocator reserve. Не грузить при нехватке
   128 MiB; расширение window требует аудита реального ziyi bank.
2. **S2b RAM boot:** bounded loader на проверенной основе S1, один CPU,
   только ziyi GIC/timer/framebuffer, без UFS/USB/touch/persistent log.
   Первый результат — ранний XNU/arm_init и работающий timer/exception path;
   сохранить camera/RAM evidence. Затем маленький md0 root с настоящим
   PID1/heartbeat, чтобы проверить BSD/exec до большого GUI userspace.
3. **Перед GUI:** подготовить iPhone X root на ПК, воспроизвести launchd/dyld
   и необходимые IOKit/HFS/AMFI/IOSurface services. Для userspace разрешено
   рассмотреть UFS root, но только после выполненных backup/restore gates
   ниже; перенос всей topaz hardware series не нужен. Touch/GPU/WLAN не входят
   в минимальный lock-screen критерий.
4. **Целевой результат:** Apple backboardd/SpringBoard и связанные сервисы
   реально исполняются на ziyi, экран блокировки обновляется, часы меняются
   на непрерывной записи. Отдельно фиксируются запуск/вывод PosterBoard и
   наличие wallpaper; рисунок часов или Apple logo из shim не засчитывается.

### Условное разрешение на запись от пользователя

На этом этапе пользователь разрешил запись **только когда мы уже близки
к запуску userspace**, при сохранённой partition table и проверенном
восстановлении. До этого текущие эксперименты RAM-only. Перед записью нужны
обе GPT (headers+entries, primary/backup), полные affected partitions,
включая userdata для root/logger, точные identity/sizes/LUN/slot, SHA-256
off-device copies, независимый on-device read/hash и установленный доступный
restore path. GPT не содержит файлов и не заменяет userdata backup.
Не обещаем абсолютную восстановимость до проверки фактических копий и пути
восстановления. Сейчас partition backups/restore для этой операции ещё нет,
storage authorization gates не выполнены.

Локально на C: и E: свободно примерно 6,3/4,6 GB. Этого недостаточно
для полного donor-root workflow и полного userdata backup, поэтому большие
Apple root images пока не скачивались. До storage stage потребуется носитель
с достаточным местом под полные partition images и отдельную проверку restore.

Перед обновлением существующих локальных документов проверен backup
`Artifacts/ziyi/ios-sweet/20261020-before-donor-update`: 8 files / 281413 bytes,
SHA-256/source stability/inventory и независимые restore copies — PASS.
Перед внесением CI результата повторно создан/проверен отдельный backup
`20261020-before-ci-result-update`: 9 files / 301153 bytes. Отдельно сохранена
первая версия Mach-O audit script перед исправлением поддержки file-only
`__CTF` metadata (vmsize=0); неуспешный первый audit output сохранён отдельно.
