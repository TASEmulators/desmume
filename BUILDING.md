# Building DeSmuME on Linux (Ubuntu)

## 1. Install dependencies

```bash
sudo apt update
sudo apt install -y \
    build-essential git meson ninja-build \
    libglib2.0-dev libsdl2-dev libpcap-dev zlib1g-dev \
    libgtk-3-dev libx11-dev
```

**Optional packages** (auto-detected at configure time):

| apt package           | Feature                          |
|-----------------------|----------------------------------|
| `libopenal-dev`       | OpenAL microphone input          |
| `libasound2-dev`      | ALSA microphone input (fallback) |
| `libsoundtouch-dev`   | High-quality audio resampling    |
| `libagg-dev`          | HUD/OSD rendering                |
| `libgl-dev`           | Desktop OpenGL rendering         |

## 2. Get the source

```bash
git clone https://github.com/TASEmulators/desmume.git
cd desmume
```

## 3. Build (Meson — GTK3 + CLI, recommended)

```bash
cd desmume/src/frontend/posix
meson setup build --buildtype=release
ninja -C build
```

Binaries after a successful build:
- `build/gtk/desmume` — GTK3 graphical frontend
- `build/cli/desmume-cli` — command-line frontend

## 4. Install / Uninstall

```bash
sudo ninja -C build install    # installs to /usr/local by default
sudo ninja -C build uninstall
```

To install to `/usr` instead of `/usr/local`:

```bash
meson setup build --buildtype=release --prefix=/usr
sudo ninja -C build install
```

## 5. Optional build flags

Pass `-Doption=true` to `meson setup`, e.g. `meson setup build -Dopengl=true`.

| Flag                    | Default | Description                        |
|-------------------------|---------|------------------------------------|
| `-Dopengl=true`         | false   | Desktop OpenGL rendering           |
| `-Dopengles=true`       | false   | OpenGL ES rendering                |
| `-Dglx=true`            | false   | GLX context (requires OpenGL)      |
| `-Degl=true`            | false   | EGL context                        |
| `-Dopenal=true`         | false   | OpenAL microphone input            |
| `-Dwifi=true`           | false   | Experimental Wi-Fi support         |
| `-Dgdb-stub=true`       | false   | GDB stub for debugging             |
| `-Dfrontend-gtk2=true`  | false   | GTK2 frontend (legacy)             |

## 6. Legacy build (Autotools — GTK2)

Use this only if you need the GTK2 frontend. Extra dependencies:

```bash
sudo apt install -y autoconf automake libgtk2.0-dev
```

```bash
cd desmume/src/frontend/posix
autoreconf -i   # or: ./autogen.sh
./configure
make -j$(nproc)
```

---

More information: `desmume/README.LIN` · [Wiki](https://wiki.desmume.org/index.php?title=Installing_DeSmuME_from_source) · [Forum](https://forums.desmume.org)
