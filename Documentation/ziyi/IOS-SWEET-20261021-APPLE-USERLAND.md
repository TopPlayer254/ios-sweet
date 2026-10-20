# ziyi: аудит Apple userspace и следующий RAM root

21 октября 2026, Europe/Moscow. Телефон в этом аудите не использовался.
Аппаратная база остаётся S2X build10: полный source-CI XNU cb8d9246,
md0/mockfs, собственный диагностический PID1 и пятисекундный heartbeat.
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

Наличие PosterBoard.app не доказывает запуск lock screen. LC_LOAD_DYLIB также
показывает, почему копирования одного119K executable недостаточно. Реальные
часы/экран и исполнение PosterBoard проверяются отдельно.

## Минимальный следующий опыт: HFS root в RAM

Текущий mockfs отдаёт всё устройство как один Mach-O. Он доказал PID1, но
не обслуживает настоящее дерево root с dyld, libraries, plists и symlinks.
В закреплённой серии patch0026 встроен Apple OSS HFS; mockfs_check_macho
отклоняет filesystem image, позволяя следующему filesystem попытаться mountroot.
Регистрация HFS уже видна в телефоном логе; само HFS mount ещё не доказано.

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
Фактические FDT B4E30000, payload B5031000..B7FFEDD0, размер50126544bytes;
оба диапазона не пересекаются, окна FD/stack/framebuffer не затронуты.
Entry был compiled out. Это доказанная доставка HFS bytes в RAM;
HFS mount и запуск нового init ещё требуют S2X.

После HFS proof следующий опыт должен изолировать Apple dyld/runtime до GUI.
Не заменять PID1 на launchd без реального root/dependency/diagnostic плана.
Upstream mkiosroot.sh делает большой HFS root для userdata; он прочитан,
но не выполнялся. UFS и persistent logging остаются исключены; GPT/разделы
не читались и не записывались. Условия полного backup/restore перед storage
пока не выполнены.

## Расстояние до крупных рубежей

Darwin + собственный PID1 достигнуты и повторены на полном source-CI kernel.
Следующий барьер — настоящее root tree и Apple dynamic runtime. Apple boot logo,
Apple launchd, PreBoard, PosterBoard/живой lock screen и SpringBoard пока не
достигнуты. Порядок UI процессов не предполагается строго линейным; проценты
и сроки до GUI по текущим данным не обоснованы.
