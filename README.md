# copy

Pipe anything into your system clipboard, on any Linux distribution, WSL, or macOS.

```bash
echo hello | copy
cat notes.txt | copy
tail -f app.log | copy -n 20
```

`copy` reads standard input and writes it to the system clipboard,
automatically detecting whether to use an X11, Wayland, WSL, or macOS
clipboard backend. It has no runtime dependencies of its own; it
simply calls whichever backend is already installed on your system.

## Supported platforms

| Platform | Backend used | Notes |
|---|---|---|
| Linux (X11 session) | `xclip` or `xsel` | GNOME on Xorg, KDE, XFCE, i3, most traditional desktops |
| Linux (Wayland session) | `wl-copy` | GNOME on Wayland, Sway, Hyprland, most modern desktops |
| WSL (Windows Subsystem for Linux) | `clip.exe` | Requires `clip.exe` reachable on PATH, which is the default |
| macOS | `pbcopy` | Works if built from source; no `.deb` is provided for macOS |

Any Linux distribution is supported as long as one of the backend
tools above is installed — this includes Debian, Ubuntu, Arch, Fedora,
openSUSE, Alpine, and others, on both amd64 and arm64 (build from
source for non-amd64 architectures).

Not supported: Android/Termux, and any headless environment with no
display server or session to hold a clipboard.

## Features

- Automatic backend detection: X11 (`xclip`, `xsel`), Wayland (`wl-copy`),
  WSL (`clip.exe`), and macOS (`pbcopy`)
- `-n, --lines N` — copy only the last N lines of input, matching the
  behavior of `tail -n`
- `-c, --chars N` — copy only the last N characters of input
- `-p, --print` — also print the copied text to standard output, to
  verify what was copied
- `-k, --keep-newline` — keep a trailing newline (stripped by default)
- No fixed size limit: input is read into a dynamically growing buffer
  rather than a fixed-size cap, with a 512 MB safety ceiling to prevent
  unbounded memory use. If that ceiling is ever reached, `copy` exits
  with an error and copies nothing, rather than silently truncating data
- Clear, actionable error messages if no clipboard backend is installed
- Small C codebase with no external library dependencies

## Installation

### Debian, Ubuntu, Mint, Pop!_OS (amd64)

Download the latest `.deb` from the
[Releases](https://github.com/MaxEdgar/copy/releases) page, then:

```bash
sudo dpkg -i copy_1.2.0_amd64.deb
sudo apt-get install -f
```

The second command is only needed if `dpkg` reports a missing
clipboard backend; it will install `xclip` or an equivalent
automatically.

This installs the `copy` binary to `/usr/bin/copy` and its manual page
to `/usr/share/man/man1/copy.1.gz`.

### Build from source (any distribution with a C compiler)

`copy` is a single C file with no dependencies. This works identically
on Arch, Fedora, openSUSE, Alpine, or any other Linux distribution:

```bash
git clone https://github.com/MaxEdgar/copy.git
cd copy
make
sudo make install
```

By default this installs to `/usr/local/bin/copy` and
`/usr/local/share/man/man1/copy.1`. To install elsewhere:

```bash
sudo make install PREFIX=/usr
```

To remove:

```bash
sudo make uninstall
```

### Clipboard backend

`copy` calls an existing clipboard tool; it does not implement
clipboard access itself. Install one of the following depending on
your session type:

| Session | Package        | Install command                                            |
|---------|----------------|-------------------------------------------------------------|
| X11     | `xclip`        | `sudo apt install xclip` or `sudo pacman -S xclip`           |
| Wayland | `wl-clipboard` | `sudo apt install wl-clipboard` or `sudo pacman -S wl-clipboard` |

If no backend is found, `copy` will print the exact command to run
for your session.

## Usage

```
Usage: copy [OPTIONS]

Pipe stdin to the system clipboard. Auto-detects X11, Wayland, WSL,
and macOS clipboard backends.

Options:
  -n, --lines N       Only copy the last N lines of input (like tail -n)
  -c, --chars N       Only copy the last N characters of input
  -p, --print         Also print the copied text to stdout
  -k, --keep-newline  Keep a trailing newline if present (default: stripped)
  -h, --help          Show this help message and exit
  -v, --version       Show version information and exit
```

### Examples

Copy a short string:

```bash
echo hello | copy
```

Copy the contents of a file:

```bash
cat notes.txt | copy
```

Copy only the last 20 lines of a live log:

```bash
tail -f app.log | copy -n 20
```

Copy the last 500 characters of kernel messages, and print what was copied:

```bash
dmesg | copy -c 500 -p
```

## Manual page

Once installed, full documentation is available via:

```bash
man copy
```

## How it works

On startup, `copy` checks environment variables (`XDG_SESSION_TYPE`,
`WAYLAND_DISPLAY`, `DISPLAY`) and `/proc/version` to determine the
current session type, then looks for the corresponding clipboard tool
on `PATH`. If detection is inconclusive (for example, when run from
cron, `su`, or a script where environment variables are not
inherited), it falls back to trying every supported backend that is
installed, in a sensible order.

Input is read into a buffer that starts small and grows as needed, so
there is no meaningful size limit for ordinary use. If `-n` or `-c` is
given, the input is trimmed to the requested number of lines or
characters before being sent to the clipboard tool. A single trailing
newline is stripped by default, matching common expectations when
pasting into a text field; use `-k` to preserve it.

## Limitations

- Requires an active display/session with a clipboard to copy into.
  It cannot copy into a clipboard on a headless server with no display
  attached, because no such clipboard exists.
- Maximum input size is 512 MB, enforced as a safety ceiling against
  unbounded memory use, not a normal-use limit.

## Contributing

Issues and pull requests are welcome at
[github.com/MaxEdgar/copy](https://github.com/MaxEdgar/copy).

## License

MIT. See [LICENSE](LICENSE).

## Author

MaxEdgar
https://github.com/MaxEdgar

## Every star makes me really happy!
