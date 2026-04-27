# FIDEt (Mini C IDE на GTK4)

Це просте IDE для C на **GTK4**: відкриття папки проєкту, редагування файлів, збереження, компіляція та запуск.

> ⚠️ Важливо: застосунок під час роботи викликає `gcc` для компіляції файлів. Тому `gcc` має бути встановлений і доступний у `PATH`.

---

## 1. Що потрібно перед стартом

- Git
- GCC (компілятор C)
- GTK4 (заголовки + бібліотеки для розробки)
- pkg-config

---

## 2. Linux — повна інструкція

### 2.1. Встановіть залежності

#### Ubuntu / Debian
```bash
sudo apt update
sudo apt install -y build-essential pkg-config libgtk-4-dev git
```

#### Fedora
```bash
sudo dnf install -y gcc pkgconf-pkg-config gtk4-devel git
```

#### Arch Linux
```bash
sudo pacman -S --needed base-devel pkgconf gtk4 git
```

### 2.2. Клонування репозиторію

```bash
git clone https://github.com/fand1l/FIDEt.git
cd FIDEt
```

### 2.3. Перевірка, що GTK4 бачиться системою

```bash
pkg-config --modversion gtk4
```

Якщо бачите версію (наприклад `4.12.5`) — все добре.

### 2.4. Компіляція застосунку

Виконуйте з кореня репозиторію:

```bash
mkdir -p builds/x86/Linux
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror \
  main.c ui.c build.c file_ops.c \
  $(pkg-config --cflags gtk4) \
  $(pkg-config --libs gtk4) \
  -o builds/x86/Linux/mini_ide
```

### 2.5. Запуск

```bash
./builds/x86/Linux/mini_ide
```

> Рекомендується запускати саме з кореня репозиторію, щоб гарантовано підхопився `style.css`.

### 2.6. Готовий Linux-білд у репозиторії

У репозиторії вже присутній готовий бінарник:

```text
builds/x86/Linux/mini_ide
```

Його можна запускати одразу (за наявності GTK4 runtime у системі).

### 2.7. Як користуватись після запуску

1. Натисніть **Open**.
2. Виберіть папку з вашими C-файлами.
3. Клікніть на `.c` файл у списку зліва.
4. Натисніть **Compile** (тільки компіляція) або **Run** (компіляція + запуск).

---

## 3. Windows — повна інструкція (рекомендовано через MSYS2)

Найпростіший і найнадійніший шлях для GTK4 на Windows — **MSYS2 + MinGW-w64**.

### 3.1. Встановіть MSYS2

1. Завантажте інсталятор: https://www.msys2.org/
2. Встановіть у стандартну папку (зазвичай `C:\msys64`).
3. Відкрийте **MSYS2 UCRT64** (саме цей ярлик).

### 3.2. Оновіть систему в MSYS2

У вікні **MSYS2 UCRT64**:

```bash
pacman -Syu
```

Якщо термінал попросить перезапуск — закрийте його, відкрийте **MSYS2 UCRT64** знову і повторіть:

```bash
pacman -Syu
```

Продовжуйте, доки оновлення повністю не завершиться.

### 3.3. Встановіть потрібні пакети

```bash
pacman -S --needed \
  mingw-w64-ucrt-x86_64-toolchain \
  mingw-w64-ucrt-x86_64-gtk4 \
  mingw-w64-ucrt-x86_64-pkgconf \
  git
```

Під час встановлення toolchain може спитати групу пакетів — натисніть **Enter** для встановлення всіх за замовчуванням.

### 3.4. Клонування репозиторію

```bash
git clone https://github.com/fand1l/FIDEt.git
cd FIDEt
```

### 3.5. Перевірка GTK4

```bash
pkg-config --modversion gtk4
```

### 3.6. Компіляція

```bash
mkdir -p builds/x86/Windows
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror \
  main.c ui.c build.c file_ops.c \
  $(pkg-config --cflags gtk4) \
  $(pkg-config --libs gtk4) \
  -o builds/x86/Windows/mini_ide.exe
```

### 3.7. Запуск

```bash
./builds/x86/Windows/mini_ide.exe
```

### 3.8. Якщо подвійний клік не запускає EXE

Запускайте з **MSYS2 UCRT64** терміналу:

```bash
cd /шлях/до/FIDEt
./builds/x86/Windows/mini_ide.exe
```

### 3.9. Примітка про папку Windows

Структура папок для Windows вже є:

```text
builds/x86/Windows/
```

Але файл `mini_ide.exe` може бути відсутній, якщо ви ще не збирали проєкт у Windows-середовищі.

---

## 4. Типові помилки та рішення

### Помилка: `Package gtk4 was not found in the pkg-config search path`

Причина: GTK4 dev-пакети не встановлені або ви у неправильному shell.

Що робити:
- Linux: перевстановіть `libgtk-4-dev` / `gtk4-devel` / `gtk4` (залежно від дистрибутива).
- Windows: використовуйте саме **MSYS2 UCRT64**, перевстановіть `mingw-w64-ucrt-x86_64-gtk4` та `mingw-w64-ucrt-x86_64-pkgconf`.

### Помилка: `fatal error: gtk/gtk.h: No such file or directory`

Причина: GCC не отримав флаги GTK4.

Що робити:
- Компілюйте тільки командою з `$(pkg-config --cflags --libs gtk4)`.

### Кнопки Compile/Run у застосунку не працюють

Причина: всередині IDE викликається `gcc`, але його немає в `PATH`.

Що робити:
- Перевірте у тому ж терміналі:
  ```bash
  gcc --version
  ```
- Якщо команда не знайдена — встановіть GCC і перезапустіть термінал.

### Не підтягуються стилі інтерфейсу

Причина: файл `style.css` не знайдений через запуск не з кореня проєкту.

Що робити:
- Спочатку перейдіть у корінь репозиторію (`FIDEt`), і вже **звідти** запускайте бінарник за відносним шляхом:
  - Linux:
    ```bash
    cd FIDEt
    ./builds/x86/Linux/mini_ide
    ```
  - Windows:
    ```bash
    cd FIDEt
    ./builds/x86/Windows/mini_ide.exe
    ```
- Не запускайте з каталогу `builds/...`, інакше `style.css` може не знайтися.

---

## 5. Актуальна структура білдів

```text
builds/
└── x86/
  ├── Linux/
  │   └── mini_ide
  └── Windows/
    └── mini_ide.exe   (створюється після збірки у Windows)
```