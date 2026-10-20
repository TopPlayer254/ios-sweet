# ziyi: Darwin и первый PID1 в RAM

20 октября 2026. **S2X build9 PASS, повторено на полном source-CI build10.** XNU нашёл md0,
смонтировал mockfs и выполнил статический диагностический init как PID1.
Это собственный тестовый userspace, пока без Apple launchd/графического UI.
Все текущие эксперименты — fastboot boot, без записей в разделы.

## Доказательство

Запись180s: `Artifacts/ziyi/ios-sweet/IOS-SWEET-20261020-S2X-device7/camo-run.mkv`.
Кадры155/175s показывают:

- `BSD root: md0, major 1, minor 0`;
- `load_init_program: attempting to load /sbin/launchd`;
- `*** ZIYI userspace: init running as PID 1 ***`;
- 6 и10 строк `ZIYI init: still alive` соответственно: ещё4 сообщения за20s,
  согласуется с select timeout5s. Видимого panic нет.

Mockfs отдаёт raw static Mach-O как /sbin/launchd. Наличие этого пути
не означает, что запущен Apple launchd. Source init открывает только
/dev/console, вызывает getpid/write/select; ELF/динамических библиотек нет.
Arm64 Mach-O/entry/16K segments и четыре4K страницы ad-hoc SHA256 подписи
проверены независимо на ПК. Принятие exec/signature, user-mode переход,
devfs console и syscall/timeouts теперь проверены и на телефоне.

| Закрепление | Значение |
|---|---|
| XNU diagnostic derivative SHA256 | `17547e3f2d4046b60117516e4a1cc1cb265a09a454f0c050113659761b874999` |
| X image SHA256 | `9405734a7c451f8e7733d7f7c7c3df3ac91f501801564ca7a200e300de1b1457` |
| X preentry CRC32 | `22583188` |
| P image SHA256 | `130678ea7040bbdf6c7eb561d73c275abbdf2129f7e15ff7def81756f748aeb5` |
| P CRC32 | `a4551bb5` |
| Payload SHA256 | `f0f362a4bd9648b0b2e491f283b98fc7b7666d6cd4a1cd34d37f696f42c95b37` |
| Aggregate11 segment/BSS SHA256 | `8485621dacbf23f3e806a92fd303302cb9e99126c504405876a8dde25c18f8c9` |
| ADT SHA256 | `6d0998f555b43c6396d60eea90a489b43308914dc464a0374d965b4d53275844` |
| boot_args SHA256 | `dea5fde9e01d23579a307078fec28785fc9ed85c1a841170f062c82a280c824f` |
| Diagnostic PID1 SHA256 / bytes | `e15eeec89141b44382edc4420b8b883bdc30dc7cdd38569623fd5e8c0e6b2b24` /16638 |
| RAMDisk PA / extent | `0x8020c0000` /32768bytes |
| ADT / boot_args / top PA | `0x8020c8000` / `0x8020cc000` / `0x8020d0000` |

Оба preentry snapshots: 5/5 CRC-valid packets,416bytes, flags63, stage4,
все8 checks PASS; kernel/ADT/boot_args хешируются на самом телефоне до entry.
P и X evidence/build manifests сохранены и повторно сверены. Поздние записи
не заменяют старые snapshots. Полный хронологический разбор —
[S2 report](IOS-SWEET-20261020-S2.md).

## Что понадобилось исправить

1. Bounded FDT parser: несколько qcom msm-id pairs и реальный ABL initrd
   relocation. Ошибочные ранние attempts остановились до kernel copy.
2. Early-fbcon mapping: активные~10MiB вместо полного43MiB splash reservation;
   сам аппаратный reserved range сохранён.
3. Необязательная запись CNTP_CTL_EL0 вызывала undefined instruction на EL1.
   Убрана только эта precaution; успешный CNTV enable сохранён.
4. SweetCPU personality искал qcom,kryo470, тогда как реальный CPU ADT —
   qcom,sm7450. Исправлен kernel matching. Подтверждены CPU1/1 и self-SGI1.
5. Rootless md0 panic устранён добавлением32K RAMDisk с диагностическим PID1.

Телефонный kernel — явно закреплённый derivative исходного CI Mach-O:
один NOP и matching string data delta. Отдельная полная исходная пересборка
с теми же двумя fixes уже выполнена:
[CI39475233522](https://github.com/TopPlayer254/ios-sweet/actions/runs/39475233522),
commit `08b7f85b9899ae7ff5e839232a764b57cf01b89c`.
CI kernel SHA `cb8d9246ce087ac472de727e4ee4a42b894eac6a2b5a8168fca2296cb40465ec`.
API ZIP digests/CRC/SHA256SUMS,12 source files, Mach-O и десять no-I/O provider
stubs проверены; compiled CPU match и CNTV без CNTP подтверждены.
Этот точный новый CI binary затем прошёл build10 на телефоне:

| Полный source-CI повтор | Значение |
|---|---|
| P image SHA / CRC | `4a84b7f98ffa1c27da124bc9d575e5bbe0a2c561046c0a8e6905a37af8157268` / `8c79c0c7` |
| X image SHA / CRC | `9ce4e7941d721aac1a1c02783d510cb8e34b473b3053c44c591bb94c79b939da` / `907f3943` |
| Payload SHA | `6f97ccf54b62557dbd632c168057c7c35b61f817e20c3abed5e5c7493cf3a6a5` |
| Aggregate11segments/BSS SHA | `7af5dc1fb0d78d0d2c55fb71add27d0c50dec7c77c6774c3cb713be9d4f5c409` |
| Heartbeat155/175s | 7/11 сообщений, ещё4 за20s, без видимого panic |

ADT/boot_args/init/layout bytes совпали с build9 точно. P/X build10 и
device8 camera/snapshots/result/manifests заморожены. Актуальная аппаратная
база теперь — полный CI kernel; diagnostic derivative сохранён отдельно.

## Минимальный следующий эксперимент

Подготовить минимальный **Apple** root для iPhone X16.7.16/20H392:
из официального IPSW получить OS/SystemOS, проверить manifest digests,
разобрать root/cache layout и зависимости launchd/dyld до boot. Сначала
критерий Apple launchd и его базовые сервисы; затем IOSurface/IOMobileFramebuffer
и lock screen. Донорские topaz graphics patches применять по наблюдаемой
несовместимости, не переносить чужие MMIO/UFS defaults целиком.

**Обновление21 октября:** OS/SystemOS/AppOS полностью прошли manifest SHA384/
ZIP CRC и независимое перечитывание. APFS/Mach-O audit нашёл plain arm64
launchd/dyld и PreBoard/PosterBoard, но полный runtime cache2.55GiB не входит
в текущую128MiB арену. Подготовлен отдельный32MiB HFS diagnostic RAM root CI:
следующий опыт проверит реальное дерево files/symlinks нашим PID1 до Apple
runtime. CI39507288859 fsck/readonly reread и PC artifact checks PASS;
S2P build11/device9 подтвердил32MiB HFS root в RAM, CRC1241a774,
все11segments/ADT/args SHA совпали. HFS mount на телефоне ещё не доказан.
[Новый аудит и минимальный опыт](IOS-SWEET-20261021-APPLE-USERLAND.md).
Старые partial files сохранены. Это не резерв для полного partition backup. Работа с GPT/UFS
по-прежнему требует выполнения отдельных
условий backup/identity/hashes/restore; сейчас они не выполнены.

## Крупные точки

| Точка | Текущее положение |
|---|---|
| Darwin/XNU + собственный PID1 | Достигнуто в RAM: MMU/VM/pmap, CPU/IRQ, BSD/root/exec/syscalls/heartbeat |
| Apple boot logo | Не подтверждён; вручную нарисованный логотип не засчитывается |
| Apple launchd / PreBoard | Ещё не запускались; следующий большой блок — настоящий root/cache/runtime |
| PosterBoard / lock screen | Ещё не запускались; после Apple services нужны graphics/IOSurface и процесс/UI proof |
| SpringBoard home screen | Ещё не запускался; дальний GUI рубеж |

Проценты/сроки не выводятся из количества патчей: оставшиеся Apple runtime и
графические несовместимости ещё не измерены. PreBoard/PosterBoard/SpringBoard
не предполагаются обязательной линейной последовательностью.
