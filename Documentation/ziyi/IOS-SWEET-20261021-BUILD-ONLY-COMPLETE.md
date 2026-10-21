# Итог сборки: завершено, запуск приостановлен

21 октября 2026. **HOST_BUILD_COMPLETE_NOT_RUN**. Последнее указание
пользователя: «дособирай и пока стоп, без запуска». После этого указания
телефон не опрашивался, не загружался и не перезапускался. Новых опытов нет.

## Итоговый образ

`Artifacts/ziyi/ios-sweet/20261021-linux-usb-boot-v3/ziyi-ram-usb-build-only.img`

- Размер: **12550144 байта**.
- SHA-256: `a4bbbdc1f438276af464fefcac7bffaf08af5ca75ff43d886fca29057850abba`.
- Kernel Image SHA-256: `3a13d8ab19654c8cc70509964ab99b5d076cf3dec8c17f419d48c36597884bbe`.
- [CI39595655078](https://github.com/TopPlayer254/ios-sweet/actions/runs/39595655078): SUCCESS,
  source `fda9ab379fac02cb25d550e928481dc01fbe887c`.
- Назначение: собственный Linux RAM USB receiver и диагностика консоли.
  Apple root внутрь не включён, kexec RPC отсутствует. Это не образ готового iOS.

## Завершённые проверки на ПК

Оригинальный CI ZIP12343233464,15122078bytes, SHA-256
`6045dbe4d36a04d7abe60e4954073d68679d6a5a4ae99ba350b5ae91196223d7`
прошёл API identity/digest и ZIP CRC. Скачивание через коннектор теряло1582
байта в конце. Отдельный [CI39597212550](https://github.com/TopPlayer254/ios-sweet/actions/runs/39597212550)
проверил полный оригинал и выдал последние4096байт; перекрытие с локальным
префиксом совпало. Восстановленный архив прошёл полный исходный API SHA-256
и ZIP CRC. Это восстановление транспорта, не замена контрольной суммы.

Локальный fetch audit: `20261021-linux-usb-ci-v8/fetch-audit.json`.
Проверены10 source hashes, build artifact hashes, конфигурация/символы без
storage, модулей и виртуализации, три GPL backup/restore copies против
закреплённых исходников. Встроенный initramfs независимо извлечён из Image:
только dev, dev/console и собственный ziyi-ram-init, SHA-256
`074062bb86a84649d6b86bf0fa2723ed1bdb33d9616d0ce876decab73524d518`.

Package audit: `20261021-linux-usb-boot-v3/package-audit.json`.
Независимо проверены Android boot header2, размещение kernel-кандидата
0xa8000000, effective footprint24772608bytes, gzip и DTB roundtrip.
Все исходные DTB properties сохранены. Добавлены7 simple-framebuffer
properties и2 свойства framebuffer reserved-memory (reg/no-map), всего9.
43MiB framebuffer reservation исключена из Linux linear mapping.
Итоговый DTB SHA-256:
`310a317ae4f79362a4a0daddde4f2494d4f3264d0be84674ca86beb4c49469fc`.
Постоянная WC console mapping имеет ранние метки и защиту от обращения к
освобождённому early mapping. Аппаратная работоспособность исправления
**не проверена**; причина остановки предыдущего device2 ещё не установлена.

CI-v8, boot-v3 и отдельный device2-review запечатаны stream manifest с
независимым перечитыванием SHA-256. Существующие port/docs файлы перед
обновлением сохранены с verified backup и независимой restore copy.
Ключи и расшифрованные Apple assets остаются локально.

## Подтверждённый прогресс и пределы

Darwin/XNU, HFSX md0 и собственный PID1 с heartbeat доказаны на телефоне
ранее, build12. Реальные Apple launchd/dyld/cache собраны в3GiB HFSX probe
и проверены на ПК; этот root ещё не исполнялся на телефоне.

Linux device2 показал early framebuffer и late initcalls до
of_platform_sync_state_init (~0.6067s), затем USB_NOT_READY180s. Нижняя
BPF строка — старый кольцевой вывод, не доказательство BPF hang.
Свой Linux PID1 и передача большого RAM root пока не доказаны.

Apple launchd, логотип Apple, PreBoard, PosterBoard и SpringBoard ещё не
достигнуты. Следующий крупный рубеж после явного возобновления пользователем —
доставка полного headless Apple runtime и доказанный native handoff к launchd.
Бюджет RAM, динамические reservations, DMA shutdown и entry-контракт ещё
не закрыты; см. [native handoff](IOS-SWEET-20261021-NATIVE-HANDOFF.md).
Сейчас работа остановлена. Условия записи разделов не выполнены.
