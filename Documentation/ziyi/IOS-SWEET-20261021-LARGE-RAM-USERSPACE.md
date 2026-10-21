# Большой RAM этап: настоящий Apple launchd и доставка его root

21 октября 2026. Пользователь попросил объединять изменения в крупные этапы.
**Последнее указание: продолжать до настоящего lock screen с часами;
смартфон готов.** После паузы «только билд» проверенные RAM-only опыты явно
возобновлены. До загрузки — source/config/artifact/placement checks и
проверка ожидаемого ziyi. Условия записи разделов остаются невыполненными.
Критерий следующего рубежа — исполнение настоящего Apple launchd и запуск им
нашего дочернего процесса. Изменения размеров отдельных пулов — диагностика,
а не самостоятельный пользовательский результат.

## Что уже работает на телефоне

Native XNU cb8d9246, HFSX md0 и собственный PID1 — PASS build12,
CRC25075121. Проверены два file reads, symlink lookup, 5s heartbeat;
полная запись420s сохранена. Это пока собственный static userspace.
Нового запуска Apple кода на телефоне в этом этапе ещё не было.

## Собран настоящий headless Apple root

[CI39516221989](https://github.com/TopPlayer254/ios-sweet/actions/runs/39516221989)
на commit106d1f023dd749379d72b53c38b033dc0dbdaf54 — **SUCCESS**.
Создан отдельный bare HFSX образ **3221225472bytes (3GiB)**:

- настоящие `/sbin/launchd` и `/usr/lib/dyld` из iPhone X16.7.16/20H392;
- все43 runtime cache files из SystemOS, **2738257920bytes**, все42 UUID;
- Unix ownership/modes, `/etc`, `/var`, `/tmp`, каталоги devfs и launchd;
- собственный static `/usr/libexec/ziyi-launchd-child` и один RunAtLoad job;
- отдельно собранные **собственные no-op fsck/mount fixtures**. Они печатают
  явную метку и завершаются0, не являются Apple fsck/mount и не выполняют
  mount/проверку разделов. Stock LaunchDaemons/GUI resources не включены.

Исходные OS и SystemOS проверены по manifest SHA384, ZIP CRC и независимому
перечитыванию; APFS монтировался readonly. Apple код в CI не исполнялся.
После построения HFS выполнены native fsck и readonly remount с повторными
hashes всех файлов, UID/GID0 и проверкой symlinks.

Root SHA256:
`3182c24764d970c2bfe7bec5c6f73f07b79aae602047ab9aae29f79756532006`.
Это **host-built probe**, не доказательство исполнения launchd на ziyi
и не полный root для PosterBoard.

Apple файлы не публикуются открыто. В CI artifact лежит gzip образ под
RSA3072-OAEP-SHA256 + AES256-GCM; private key создан и остаётся только на ПК.
Публичный ключ не позволяет расшифровать образ. Проверка RSA round-trip на ПК
пройдена. Encrypted bytes972412641, SHA256
`2fd63dac366e409a03eac1592690d250a33fbf734f5046a4a3921047b5ec8df4`.
Первый ZIP artifact12306714719 SHA256
`9542d7f27a0353ef2433c1c8a0ea642c75e409efd55050aa6e7a06ec17fb6ff5`.
GitHub connector ограничивает один download512MiB; отдельный CI делит только
уже проверенный ciphertext на400MiB chunks. Это транспорт, не новая сборка root.
Полная локальная GCM/decode/reread проверка **PASS**: все3221225472bytes
совпали по SHA256. Windows7z независимо перечитал все48 включённых файлов
непосредственно из HFS, каждый размер/SHA совпал с macOS report. Оба наших
Mach-O бинарника побайтно воспроизведены отдельной Windows/clang сборкой.
Локальные файлы: `Artifacts/ziyi/ios-sweet/20261021-headless-root-pc/`,
`20261021-headless-child-host/`; CI ciphertext transfer/ZIP CRC/API digest
проверены в `20261021-headless-root-transfer/`. Повторная transport copy-v2
также совпала; нового построения root при этом не было.

## Почему меняется RAM транспорт

Native cache CI39511379890 подтвердил43 hashes/UUID и независимый gzip
round-trip: raw2738257920, gzip971428136bytes.41hash независимо совпал с
Windows extraction; два LZBitmap/resource-fork файла прочитаны нативно macOS.
Текущие128MiB XNU arena и проверенная ABL staging доставка не вмещают root.
Расширение memSize само по себе не доставляет данные и не доказывает ownership.

Предлагается временный Linux loader: RAM boot → USB доставка → проверенный
native kexec handoff → XNU. Linux после handoff не должен оставаться хостом
или гипервизором. Stock kernel имеет KEXEC=n и storage drivers; он для этого
пути не используется без пересборки.

## Реализованный приёмник и кандидат ядра

`ports/ziyi-ios-sweet/linux-helper/` содержит собственные PID1, PC client,
протокол, тесты и CI builder из GPL ziyi pin24856283d18328b8473cd10991c15749b88d7bf9.
Источник5.10.81 старее сохранённого stock5.10.236; совместимость с телефоном
не предполагается только из наличия исходников. Отсутствующие optional Xiaomi
HWID/plaid исключены новыми пустыми build stubs в отдельном CI checkout.

Первый кандидат — **receive-only**. Протокол имеет HELLO nonce/PID/version,
sequence, CRC, bounded lengths/offsets и независимый полный SHA256 в RAM.
Принимаются только root≤3GiB, kernel payload≤128MiB и FDT≤2MiB в anonymous
mappings процесса. После SHA успешный объект переводится mprotect в readonly.
Нет произвольных путей, физических адресов, shell/exec, storage commands,
reboot или kexec RPC. Полный3GiB receive — целевой телефонный тест;16MiB
является уже пройденным CI regression test реального C server, не финальным
рубежом проекта. Также отвергаются bad CRC/magic/replay, overflow/overrun,
непоследовательные offsets, ранний END, неверный SHA и неизвестные команды.

CI собирает static AArch64 init без PT_INTERP/PT_DYNAMIC и builtin initramfs;
INITRAMFS_FORCE исключает внешний Android/vendor init. Final .config gate
требует BLOCK/MTD/SCSI/MMC/PSTORE/DEVMEM/MODULES/EFI/REMOTEPROC/WATCHDOG=n,
USB_DWC3_MSM/configfs/ACM и KEXEC=y; linked-symbol check исключает основные
storage пути. До успешной сборки и этих gates профиль остаётся кандидатом.

Сохранённая ziyi DTB подтверждает qcom,dwc-usb3-msm, snps,dwc3, peripheral,
high-speed, реальные PHY, clocks и SMMU. Эти данные не перенесены со sweet.
Первый USB receive не требует резервировать физическую5GiB арену: Linux
allocator владеет anonymous pages и учитывает свои firmware carveouts.

## Следующий большой аппаратный пакет

1. Завершить CI/config/symbol/ELF/source audits и локальное decrypt/root audit.
2. Упаковать отдельный RAM boot образ с проверенной stock ziyi DTB и только
   необходимыми delta для simplefb. Проверить ARM64 placement/boot header,
   отсутствие storage drivers и уникальный builtin init до fastboot boot.
3. На готовом ожидаемом ziyi одним опытом подтвердить наш PID1/USB nonce,
   передать **весь3GiB root** и получить совпадающий независимый RAM SHA.
   Сохранить Camo/USB transcript и evidence. Команды kexec ещё нет.
4. Собрать и проверить native handoff пакетом: сначала прежний XNU/HFS/PID1
   как контроль, затем этот Apple root и launchd. Повторная физическая копия
   kexec потребует отдельного memory budget/overlap анализа; наличие8GB DDR
   не является готовым доказательством. Большие memory hashes должны работать
   с проверенным cache/MMU lifecycle, а не с прежней медленной M-off петлёй.

**Apple launchd PASS** требует его штатного bootstrap output и исполнения
нашего job с pid>1/ppid1 и живым heartbeat. Подмена `/sbin/launchd` нашим init
или собственноручно нарисованное яблоко этим доказательством не являются.
Далее — базовые Apple services/graphics, PreBoard/PosterBoard process/render
proof и реальный lock screen с идущими часами. Их порядок не считается линейным.

Разделы не записывались. Storage authorization остаётся условной: обе GPT,
полные затронутые разделы/при необходимости userdata, независимые device
hashes и доказанный restore должны быть готовы до каких-либо записей.
