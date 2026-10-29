# Установщик в Smart Install Maker

Smart Install Maker не установлен на этом компьютере, поэтому файл его проекта
и итоговый установщик здесь нельзя скомпилировать. Эта папка содержит готовую
комплектацию и точные параметры проекта.

## 1. Подготовить файлы

Запустите:

```powershell
powershell -ExecutionPolicy Bypass -File .\installer\smart-install-maker\prepare_package.ps1
```

После этого в папке `package` будут:

- `BudgetTracker.exe`;
- `storage.config`;
- `vc_redist.x64.exe`.

## 2. Создать проект

В Smart Install Maker создайте новый проект со значениями:

| Поле | Значение |
|---|---|
| Название продукта | Финансовый помощник |
| Версия | 1.0.0 |
| Компания | Financial Assistant |
| Каталог установки | `%ProgramFiles%\Financial Assistant` |
| Имя установщика | `FinancialAssistantSetup.exe` |
| Язык | Russian |
| Минимальная Windows | Windows 10 |
| Требуемый .NET Framework | 4.8 |
| Права | Administrator |

## 3. Добавить файлы

Из папки `package` добавьте:

- `BudgetTracker.exe` → каталог приложения;
- `storage.config` → каталог приложения;
- `vc_redist.x64.exe` → временный каталог установщика.

## 4. Добавить действия

Перед первым запуском приложения выполните:

```text
vc_redist.x64.exe /install /quiet /norestart
```

Создайте ярлыки:

- меню «Пуск» → `BudgetTracker.exe`;
- рабочий стол → `BudgetTracker.exe` (как необязательный компонент).

После установки предложите запустить `BudgetTracker.exe`.

## 5. Удаление

Включите создание деинсталлятора. При удалении нужно удалить каталог программы,
но не удалять `%LocalAppData%\FinancialAssistant`, чтобы идентификатор клиента
и доступ к прежним данным сохранились после обновления.

## 6. Сборка из командной строки

Сохраните проект средствами Smart Install Maker, затем выполните:

```powershell
powershell -ExecutionPolicy Bypass `
  -File .\installer\smart-install-maker\build_smart_installer.ps1 `
  -SmartInstallMakerExe "C:\путь\к\Smart Install Maker.exe" `
  -ProjectFile "C:\путь\к\проекту"
```

Скрипт использует официальный параметр программы `/build`.

