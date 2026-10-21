# Исторический черновик RAM USB helper

**Заменён крупным этапом по просьбе пользователя:**
[полный3GiB root и настоящий Apple launchd](IOS-SWEET-20261021-LARGE-RAM-USERSPACE.md).
Приёмник и CI уже реализованы;16MiB ниже — ранний regression scope, а не
текущий конечный критерий аппаратного этапа. Исходный черновик сохранён как
контекст принятого решения.

21 октября 2026. **План/host profile, не готовый образ и не новый телефонный
результат.** Текущий аппаратный PASS — native XNU cb8d9246, настоящий HFSX
md0 и наш PID1: CRC25075121, reads/symlink/5s heartbeat. Разделы не менялись.

## Измеренный барьер

[macOS CI39511379890](https://github.com/TopPlayer254/ios-sweet/actions/runs/39511379890)
на commitdd6784680133b769163235d8634206fc152ba866 полностью проверил
официальный SystemOS по SHA384/ZIP CRC и независимому перечитыванию, затем
прочитал APFS readonly без исполнения Apple кода.43 runtime cache files,
42 subcache UUID, **2738257920bytes**. Gzip-level6 конкатенации:
**971428136bytes (~926.4MiB)**; decode/hash round-trip совпал.
Это больше альтернативного512MiB source window нынешнего shim и всей
128MiB kernel arena. Это gzip, не измерение всех возможных кодеков.

ZIP artifact содержал только report.json; API SHA256/ZIP CRC проверены.
41 file hash/size независимо совпал с Windows extraction; два ZBM-rsrc
dyldlinkedit файла Windows7z не прочитал. Нативные hashes этих двух файлов
есть в отчёте. Ни DMG, ни Apple binaries/cache не публиковались.
Локально: `Artifacts/ziyi/ios-sweet/20261021-native-cache-transport/`.

Сам cache ещё не является полным Apple root. Нужны filesystem image,
launchd/dyld, базовые paths/configs/resources, проверенная memory ownership
и загрузочный контракт. Успешный gzip round-trip ничего из этого не доказывает.

## Проверка кандидата Linux

Из сохранённого M0 active-slot boot_a.img извлечён embedded IKCONFIG,
без телефона и изменения образа. Linux5.10.236, kernel SHA256
`42c827b562da776539da12ef90e7755495b1ebd33fe16d6257e299a0530fb801`;
config SHA256 `aa19d5a2ad009afb5a8968e8a880790a4657c659041ac15c89e4b7ce6c0a04b6`.
KEXEC/KEXEC_FILE/DEVMEM выключены. USB gadget/configfs/ACM/FunctionFS
включены, но это не доказывает готовый самостоятельный USB путь без Android init.
Stock kernel также содержит UFS/MMC drivers: такой образ нельзя объявлять
storage-safe только на основании RAM boot или пустого ramdisk.

Имеется закреплённый GPL source ziyi:
[MiCode24856283d18328b8473cd10991c15749b88d7bf9](https://github.com/MiCode/Xiaomi_Kernel_OpenSource/tree/24856283d18328b8473cd10991c15749b88d7bf9).
Makefile5.10.81, build.config.msm.ziyi, gki_defconfig плюс
vendor/ziyi_GKI.config; compiler profile clang-r416183b. Source USB path
выбирает USB_DWC3_MSM=m и выключает generic USB_DWC3_QCOM. Нельзя заменить
это одним generic GKI switch. Source старее сохранённого stock kernel;
он кандидат, не побайтно воспроизводящая сборка установленной прошивки.
Прочитаны Git objects; существующий checkout/object store не исправлялся.

## Один следующий телефонный опыт

**Цель — собственный RAM PID1 и USB echo16MiB, без kexec и без Apple root.**
Он проверит только пригодность транспорта. Успех позволяет отдельно
планировать малый Linux→XNU handoff, затем большой root. Финальное XNU должно
работать нативно после выхода из Linux; VM внутри Android не является целью.

Перед телефоном:

1. Собрать отдельный host-only Linux profile из закреплённых исходников.
   Черновик: `ports/ziyi-ios-sweet/linux-helper/ram-helper.fragment`.
   Финальная .config должна содержать KEXEC=y, BLOCK=n, MTD=n, SCSI=n,
   MMC=n, PSTORE=n, DEVMEM=n и builtin зависимости нужного ziyi USB driver.
   Черновик не проверен olddefconfig/компиляцией и сам по себе не гарантирует
   эти состояния. Проверить linked symbols: UFS/MMC/MTD paths отсутствуют.
2. Собственный initramfs/PID1, без vendor/Android init. INITRAMFS_FORCE=y
   и уникальный rdinit=/ziyi-ram-init; подтвердить, что внешние vendor ramdisks
   не заменяют init. Только tmpfs/devtmpfs/proc/sys/configfs и CDC ACM.
   Никаких block-device opens, файловых систем телефона, update/log scripts.
3. Минимальный протокол только bounded echo: максимум64KiB за пакет,
   nonce/sequence/CRC, суммарно16MiB с PC SHA256. Без shell и команд записи
   памяти/разделов. Сам kexec syscall в первом тесте не вызывается.
4. Проверить ARM64 Image header/placement, stock ziyi DTB digest, выбранный
   Android boot format и фактический handoff; не переносить адреса sweet.
   Для вывода можно использовать simplefb на уже измеренном B8000000,
   stride4320/1080×2400, с выключенным DRM takeover и проверенной DTB delta.
   Одного старого изображения на экране недостаточно.
5. Проверить hash/ELF/initramfs/config inventory и source provenance на ПК,
   создать новый образ/каталог evidence. До изменений существующих файлов —
   scoped backup/restore validation. Boot только после готовности ожидаемого
   ziyi в fastboot, затем Camo плюс независимый USB transcript.

**PASS:** отвечает наш nonce/echo endpoint, полные16MiB и SHA256 совпадают,
PID1/протокол подтверждены; linked/config audit исключает storage paths.
**FAIL:** USB не появляется, неверный digest/sequence, panic или неподтверждённый
init — сохранить результат и вернуть телефон кнопками в fastboot.
Этот тест не доказывает kexec, Apple launchd либо UI.

## Что только после него

Первый handoff — прежний маленький XNU/HFS/PID1, чтобы отдельно проверить
cache/MMU/GIC/timer/framebuffer state после Linux. До большого Apple root
нужны доказанные reserved ranges/нормализация реальных DDR banks, загрузка
и hashes большого файла, достаточная XNU arena и проверенный cache lifecycle.
Stock KEXEC=n нельзя обойти простым boot arg; передача управления пока не
доказана. Kernel/driver prototype не следует считать готовым только по сборке.

Storage остаётся отдельной условной веткой: полные GPT/разделы/при необходимости
userdata, независимые device hashes и установленный restore нужны до записи.
Текущие локальные копии исходников/boot samples не являются полным backup
телефона. Linux RAM-helper не активирует разрешение на storage автоматически.
