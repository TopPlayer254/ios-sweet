# ziyi: HFS RAM root работает; аудит Apple userspace

21 октября 2026, Europe/Moscow. Apple images исследованы только на ПК;
отдельные RAM-only телефонные опыты с нашим HFS root описаны ниже.
Аппаратная база — полный source-CI XNU cb8d9246. Build10 доказал mockfs/PID1;
**build12 теперь смонтировал настоящий HFSX root на md0**, запустил собственный
PID1 и подтвердил чтение двух файлов, symlink lookup и пятисекундный heartbeat.
Apple launchd и графические процессы на ziyi ещё не исполнялись.

## Проверенные входные данные

Официальный IPSW iPhone10,3/iPhone10,6, iOS16.7.16 /20H392, выбранная
identity D22AP. Полный URL, BuildManifest digests и ZIP offsets сохранены
в `20261020-iphone-x-metadata/donor-metadata.json` и download receipts.

| Компонент | Файл | Размер, байт | ZIP CRC32 |
|---|---|---:|---|
| OS | 087-86698-044.dmg | 4 730 814 044 | 66ccd39f |
| Cryptex1,SystemOS | 087-86593-047.dmg | 2 743 074 816 | 7f8fabcd |
| Cryptex1,AppOS | 087-86715-047.dmg | 10 485 760 | ed284220 |

Все три полных образа прошли manifest SHA-384 и ZIP CRC32, включая независимое
перечитывание после скачивания. Аудитор затем повторил полную проверку перед
APFS listing/extraction и проверил неизменность размера/mtime после чтения.
Рабочая папка: `Artifacts/ziyi/ios-sweet/20261020-ios16-userland-images-v3/`.
Прерванные v1/v2 partial files сохранены отдельно; они не считаются образами.

7-Zip25.01 прочитал UDIF/APFS на Windows без монтирования. Сохранены полные
listings с правами, UID/GID, ссылками и compression metadata. Извлечены только
шесть выбранных executable для разбора Mach-O; это аналитические копии,
не bootable Unix rootfs. Отчёт и входы заморожены в
`Artifacts/ziyi/ios-sweet/20261021-apple-userland-audit-v2/`.
Первый audit сохранён отдельно: в нём искались неверные CoreServices paths
PreBoard/PosterBoard; второй использует фактический `/Applications`.

## Что установлено по настоящим бинарникам

| Файл | Байт | CPU / subtype | Прямые зависимости |
|---|---:|---|---|
| /sbin/launchd | 541 600 | arm64 /0 | libSystem.B.dylib, libobjc.A.dylib; interpreter /usr/lib/dyld |
| /usr/lib/dyld | 922 912 | arm64 /0 | MH_DYLINKER, без LC_LOAD_DYLIB |
| /Applications/PreBoard.app/PreBoard | 169 344 | arm64 /0 | UIKit, BackBoardServices, FrontBoard, Foundation и др. |
| /Applications/PosterBoard.app/PosterBoard | 119 520 | arm64 /0 | PosterBoard/PosterKit/PaperBoardUI, UIKit, Swift runtime и др. |
| /System/Library/CoreServices/SpringBoard.app/SpringBoard | 112 960 | arm64 /0 | SpringBoard.framework, Foundation, objc, libSystem |
| /usr/libexec/backboardd | 1 184 848 | arm64 /0 | IOMobileFramebuffer, IOSurface, IOKit, QuartzCore, Metal и др. |

Все шесть — plain arm64; замены arm64e ISA для этих выбранных файлов не нужны.
Это не доказывает совместимость syscall/runtime/driver ABI. Полные load-command
inventories, SHA256, segment bounds и signature-blob hashes в OS-macho.json.
Доверие Apple CMS и все CodeDirectory slots этим аудитором не проверялись;
целостность исходного образа проверена manifest digest.

В OS inventory нет самостоятельных libSystem/objc с необходимым runtime.
Библиотеки следует искать в SystemOS shared cache. Основной arm64 cache:
43 файла (base и42 subcaches), **2 738 257 920 байт**, исключая .symbols
и отдельный DriverKit cache. Это около2,55GiB, значительно больше всей
проверенной kernel/root арены128MiB. Полный RAM-root с этим cache в текущем
layout невозможен; необходим отдельный проверенный путь к runtime: уменьшение
набора/извлечение с корректными fixups либо новый обоснованный memory/storage
layout. Одна лишь виртуальная mmap длина не равна необходимой физической RAM,
но RAM-backed файл всё равно должен где-то хранить свои байты.

Это дополнительно проверено по индексу самого cache:2710 images,42 subcaches,
platform2/iOS; libSystem, libobjc, UIKit и PosterBoard.framework действительно
перечислены. Header layout разобран по закреплённому Apple dyld1066.10,
commit3e5ba1a2d04c3a221eab66b99e0e0dd90a470150,
`cache-builder/dyld_cache_format.h`. Наличие библиотеки в индексе не означает
готовность standalone extraction/rebase. Отчёт:
`Artifacts/ziyi/ios-sweet/20261021-dyld-cache-index/audit.json`.

Проверен и [Apple dsc_extractor.cpp на том же commit](https://github.com/apple-oss-distributions/dyld/blob/3e5ba1a2d04c3a221eab66b99e0e0dd90a470150/other-tools/dsc_extractor.cpp):
в optimize_loadcommands он обнуляет rebase/bind/weak/lazy info и exports trie,
прямо ориентируясь на lldb symbols. Поэтому его обычный output нельзя считать
готовыми standalone runtime dylibs. Уменьшенный root потребует проверенной
реконструкции runtime metadata либо другого способа использовать shared cache;
простой extract/copy не снимает этот барьер. Исходник/hash сохранены локально
в `20261021-apple-runtime-source/`; код extractor не исполнялся.

Наличие PosterBoard.app не доказывает запуск lock screen. LC_LOAD_DYLIB также
показывает, почему копирования одного119K executable недостаточно. Реальные
часы/экран и исполнение PosterBoard проверяются отдельно.

## Пройденный опыт: HFS root в RAM

Изначальный mockfs отдаёт всё устройство как один Mach-O. Он доказал PID1, но
не обслуживает настоящее дерево root с dyld, libraries, plists и symlinks.
В закреплённой серии patch0026 встроен Apple OSS HFS; mockfs_check_macho
отклоняет filesystem image, позволяя следующему filesystem попытаться mountroot.
Регистрация HFS была видна ранее; build12 теперь доказал и mount/root exec.

Подготовлен отдельный маленький диагностический root, **без Apple assets**:

1. macOS CI создаёт новый bare32MiB non-journaled HFSX image без partition map.
   В него помещается наш static init как /sbin/launchd, два sentinel files,
   /dev и symlink /var -> private/var. Монтируется только новый файл на runner.
2. fsck_hfs -f -n и повторное readonly mount проверяют содержимое, владельца
   root:wheel, executable mode0755 и symlink. Init ad-hoc CodeDirectory:
   все четыре SHA256 pages проверяются независимо.
3. После получения CI artifact на ПК повторить ZIP digest/CRC, image SHA,
   HFS header/catalog/extent и byte-for-byte init/sentinel checks.
   Затем построить новый RAM payload на неизменном cb8d9246 kernel,
   доказать placement/budget и пройти guarded native loader/optical audits.
4. Первый телефонный запуск — только S2P: RAM copy и11 segment/ADT/args hashes,
   entry отключён. Учитывать выросший payload и длительность uncached hashing.
5. Только после S2P — S2X того же содержимого. PASS: HFS mount md0,
   `ZIYI HFS proof PASS: PID1, two file reads, symlink lookup`, затем heartbeat.
   FAIL/panic сохраняется; heartbeat без proof PASS не считается HFS успехом.

Исходники: `ports/ziyi-ios-sweet/ramroot/`. Init16642bytes, SHA256
`fa702fc5daa1e85405638ef9faa73936968d8379b2c98edb608f8f89965213f8`.
Локально compile/Mach-O/no-relocations/CodeDirectory audit PASS. Первый CI
остановился на CRLF-vs-LF source hash до image creation; исправление закрепляет
канонические Git bytes wrapper. Последующие host-only итерации исправили
hdiutil create и fsck invocation; исходники и прошлые backup versions сохранены.

**CI39507288859 PASS**, commit8b8c3a1b85657bf2238d13bf915cc77f7016a4c4:
native fsck на readonly raw attachment и повторное readonly mount прошли.
Artifact ZIP SHA256 `cde2e6c21cfa04c7008f34d9e8843fa9e03b0436f6ff9ba958db48ab026685e1`,
image SHA256 `b69a1ae3b0df915c4a267338bafaf8d519b2c5d612a5f403471327dd93b4fa43`.
На ПК API digest/ZIP CRC, HFS header32MiB/4K blocks,7z test и read-back
init/двух sentinel совпали; init byte-identical локальной сборке. Все три
опубликованных source files независимо совпали с fresh clone закреплённого commit.

Payload SHA256 `37c110baaa9038b634cc2b2f91398110d5211bf2f9670e662ac31aaeb2080123`,
50126544bytes. Все10 kernel segments побайтно сохранены; HFS root по
PA8020c0000+2000000, ADT8040c0000, args8040c4000, top8040c8000.
Из исходных128MiB после top остаётся66289664bytes: бюджет размещения доказан,
достаточность для нового HFS runtime проверяется отдельно.
P image SHA `9e447ad56ca89f72288d0d796baa1ce1c339895db5294019cf65206be33f705c`;
X image SHA `87723e5a3af5e379730544676996058d08823c4642e946f2858fb9fdeb2fb39d`.
Оба прошли guarded native loader/full-arena, malformed input и optical audits;
P не содержит indirect entry, X содержит единственный проверенный BLR.
**S2P-device9 PASS:**5/5 optical packets,416bytes, CRC `1241a774`, flags63,
stage4. Payload/aggregate11segments/ADT/boot_args совпали с host SHA256;
BRK, context/canary/FDT/source bounds прошли. Uncached load/hash214.471s.
Фактические FDT B4E30000, payload B5031000..B7FFEED0, размер50126544bytes;
оба диапазона не пересекаются, окна FD/stack/framebuffer не затронуты.
Entry был compiled out. Это доказанная доставка HFS bytes в RAM;
HFS mount и запуск нового init ещё требуют успешного S2X.

### S2X build11: предел mbuf bootstrap, до HFS

X preentry CRC `83724eda` PASS, все11segment/ADT/args SHA совпали. После
entry XNU прошёл VM/IOKit/CPU1/1/self-SGI1/BSD MAC и регистрацию HFS, затем
panic в `bsd/kern/uipc_mbuf.c:1743`:
`m_total(MC_BIGCL) == 0 && m_minlimit(MC_BIGCL) != 0`.
Кадры270/330/415s и полная420s запись заморожены в S2X-device9. HFS mount
не достигнут, новый init не исполнялся. Это не ошибка CRC/root delivery.

Из buildtree именно CI39475233522 извлечены unix_startup.c/uipc_mbuf.c/
mbuf.h/param.c/arm_vm_init.c, хеши и расчёт сохранены в `20261021-mbuf-audit/`.
arm_vm_init задаёт sane_size после вычитания ранних allocations. Уже после
top доступно не более66289664bytes (<64MiB). bsd_mbuf_cluster_reserve
автоматически увеличивает пул только при sane_size >64MiB либо явном ncl.
Без override остаётся medium CONFIG_NMBCLUSTERS=256 (512KiB); при16K pages
mbuf_table_init округляет минимум4K BIGCL до нуля. Полученный panic согласуется
с этим конкретным порогом; это source-derived объяснение, не измерение всех
внутренних kernel variables.

### Build12: одно дополнение boot args

Добавлено ровно ` mbuf_pool=2`; ядро, HFS root, ADT, placement и все сегменты
побайтно сохранены. При2MiB получается1024 2K clusters, BIGCL minimum4.
1MiB недостаточно:512clusters тоже дают нулевой minimum. Этот параметр
изолирует bootstrap condition; networking и реальная нагрузка не тестируются.

Payload SHA `08284932baac0d6be14540994dfd48e20e94d0b3f4965d0bc5561c3d0ffde407`;
X image SHA `c71e2cc2b2c05bdd24ab621eb25518b1d71588a85daab781ae37224d7692b1b4`.
Host native loader/full arena/negative-input и optical audits PASS.
Повтор использует завершённый P build11 для неизменных kernel/root bytes:
реальный runner проверяет old image/proof, оба args SHA, идентичность всех
bytes вне CommandLine и только точное добавление mbuf_pool=2 для двух pinned
payload hashes. Новый X самостоятельно проверяет новые args до entry.
Пять отрицательных guard cases (иной pool, bytes вне command, stale image,
изменённый loaded digest, иной payload) отвергнуты до mocked device gate.
Это не новый P build12.

**X build12/device10 HFS PID1 PASS.** Preentry CRC `25075121`, все11segment/
ADT/new args hashes совпали; loaded/ADT digest идентичны P11. XNU вывел
`mbinit: done [2 MB total pool size, (1/0) split]`, нашёл md0 и смонтировал
ZIYIRAMRoot. Затем exec собственного /sbin/launchd дал:
`ZIYI HFS proof PASS: PID1, two file reads, symlink lookup`.
Кадры300/320s содержат6/10 heartbeat сообщений: ещё4 за20s, panic не виден.
Это настоящий HFS driver/vnode/directory/symlink/read/exec path в RAM,
но /sbin/launchd всё ещё наш диагностический init, не Apple launchd.

## Минимальный следующий эксперимент

HFS root теперь не является неподтверждённой предпосылкой. Следующий шаг на
ПК — unpack только43 runtime cache files без .symbols, сверить размеры/UUID
с cache index и измерить сжатый transport budget с независимым round-trip.
Так можно оценить RAM-only доставку настоящего cache; размер2.55GiB нельзя
просто добавить в текущий payload/128MiB arena. Места будущего большого root,
memory ownership, ABL staging и download limits требуют отдельного доказательства.
Это измерение не запускает Apple код и не требует разделов телефона.

Windows 7z25.01 извлёк41/43 файла, но .28.dyldlinkedit и .42.dyldlinkedit
используют ZBM-rsrc/LZBITMAP и были отвергнуты как Unsupported Method.
Partial outputs сохранены отдельно с failure.json; полного gzip замера на
них нет. Подготовлен macOS read-only CI: native APFS decompression,
все42 UUID,43file hashes, gzip-level6 sizing и decode/hash round-trip.
Публикуется только JSON report, без Apple image/cache assets.

После выбора реального runtime/root layout — минимальный Apple dyld/launchd
опыт с явным критерием entry/runtime, затем базовые Apple services и graphics.
Не считать extraction-for-lldb готовым dynamic runtime и не менять UFS профиль
только ради обхода RAM budget.

После HFS proof следующий опыт должен изолировать Apple dyld/runtime до GUI.
Не заменять PID1 на launchd без реального root/dependency/diagnostic плана.
Upstream mkiosroot.sh делает большой HFS root для userdata; он прочитан,
но не выполнялся. UFS и persistent logging остаются исключены; GPT/разделы
не читались и не записывались. Условия полного backup/restore перед storage
пока не выполнены.

## Расстояние до крупных рубежей

Darwin + собственный PID1 достигнуты и повторены на полном source-CI kernel;
настоящее HFS root tree теперь тоже доказано. Следующий барьер — Apple dynamic runtime. Apple boot logo,
Apple launchd, PreBoard, PosterBoard/живой lock screen и SpringBoard пока не
достигнуты. Порядок UI процессов не предполагается строго линейным; проценты
и сроки до GUI по текущим данным не обоснованы.
