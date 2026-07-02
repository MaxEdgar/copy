<div align="center">

# copy

<p>
<img alt="License MIT" src="https://img.shields.io/badge/license-MIT-blue.svg">
<img alt="Language C" src="https://img.shields.io/badge/language-C-00599C.svg">
<img alt="Platform" src="https://img.shields.io/badge/platform-Linux%20%7C%20WSL%20%7C%20macOS-lightgrey.svg">
<img alt="Version" src="https://img.shields.io/badge/version-1.3.0-informational.svg">
<a href="https://github.com/MaxEdgar/copy/actions/workflows/test.yml"><img alt="Build status" src="https://github.com/MaxEdgar/copy/actions/workflows/test.yml/badge.svg"></a>
</p>

<p><em>Pipe anything into your system clipboard, from the terminal.</em></p>

</div>

`copy` reads standard input and writes it to the system clipboard,
automatically detecting whether to use an X11, Wayland, WSL, or macOS
clipboard backend. It has no runtime dependencies of its own; it
simply calls whichever backend is already installed on your system.

```bash
echo hello | copy
cat notes.txt | copy
tail -f app.log | copy -n 20
```

## Table of contents

* [Quick install](#quick-install)
* [Supported platforms](#supported-platforms)
* [Features](#features)
* [Installation](#installation)
* [Usage](#usage)
* [Manual page](#manual-page)
* [How it works](#how-it-works)
* [Limitations](#limitations)
* [Contributing](#contributing)
* [License](#license)

## Quick install

This single command downloads and runs the install script, which
detects your OS and architecture, fetches the latest release, and
installs the `copy` binary to `/usr/local/bin`.

```bash
curl -fsSL https://raw.githubusercontent.com/MaxEdgar/copy/main/install.sh | bash
```

The script is safe to run more than once. Running it again simply
updates an existing installation to the latest release. For every
other platform, or if you would rather see exactly what each step
does, use the full instructions under [Installation](#installation).

## Supported platforms

| Platform | Backend used | Notes |
|:---|:---|:---|
| Linux, X11 session | `xclip` or `xsel` | GNOME on Xorg, KDE, XFCE, i3, most traditional desktops |
| Linux, Wayland session | `wl-copy` | GNOME on Wayland, Sway, Hyprland, most modern desktops |
| WSL | `clip.exe` | Requires `clip.exe` reachable on PATH, which is the default |
| macOS | `pbcopy` | Build from source; no packaged release for macOS yet |

Any Linux distribution is supported as long as one of the backend
tools above is installed. This includes Debian, Ubuntu, Arch, Fedora,
openSUSE, and Alpine, on both amd64 and arm64 architectures. Non amd64
architectures currently require a source build.

Not supported: Android or Termux, and any headless environment with
no display server or session to hold a clipboard.

## Features

* Automatic backend detection across X11, Wayland, WSL, and macOS
* `-n`, `--lines N` copies only the last N lines of input, matching
  the behavior of `tail -n`
* `-c`, `--chars N` copies only the last N characters of input
* `-p`, `--print` also prints the copied text to standard output, to
  verify what was copied
* `-k`, `--keep-newline` keeps a trailing newline if present, which is
  stripped by default
* No fixed size cap. Input is read into a dynamically growing buffer
  with a 512 MB safety ceiling, so nothing is ever silently truncated
* Clear, actionable error messages when no clipboard backend is found
* Small C codebase with zero external library dependencies

## Installation

### Prebuilt binary, on Linux (amd64 or arm64) or macOS (arm64)

Download the binary for your platform from the
[releases page](https://github.com/MaxEdgar/copy/releases), then run:

```bash
chmod +x copy-linux-amd64
sudo mv copy-linux-amd64 /usr/local/bin/copy
```

Substitute `copy-linux-arm64` or `copy-macos-arm64` for the binary
name if you are on a different platform. Each binary is also
available as a `.tar.gz` archive, and a `SHA256SUMS` file is provided
alongside the assets to verify your download.

This installs the `copy` binary to `/usr/local/bin/copy`. You will
still need a clipboard backend installed, as described below.

### Build from source, on any distribution with a C compiler

`copy` is a single C file with no dependencies. This works identically
on Arch, Fedora, openSUSE, Alpine, or any other Linux distribution.

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
your session type.

| Session | Package | Install command |
|:---|:---|:---|
| X11 | `xclip` | `sudo apt install xclip` or `sudo pacman -S xclip` |
| Wayland | `wl-clipboard` | `sudo apt install wl-clipboard` or `sudo pacman -S wl-clipboard` |

If no backend is found, `copy` prints the exact command to run for
your session.

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

Copy a short string.

```bash
echo hello | copy
```

Copy the contents of a file.

```bash
cat notes.txt | copy
```

Copy only the last twenty lines of a live log.

```bash
tail -f app.log | copy -n 20
```

Copy the last five hundred characters of kernel messages, and print
what was copied.

```bash
dmesg | copy -c 500 -p
```

## Manual page

Once installed, full documentation is available through the standard
manual page system.

```bash
man copy
```

## How it works

On startup, `copy` checks environment variables, `XDG_SESSION_TYPE`,
`WAYLAND_DISPLAY`, and `DISPLAY`, along with `/proc/version`, to
determine the current session type, then looks for the corresponding
clipboard tool on `PATH`. If detection is inconclusive, for example
when run from cron, `su`, or a script where environment variables are
not inherited, it falls back to trying every supported backend that is
installed, in a sensible order.

Input is read into a buffer that starts small and grows as needed, so
there is no meaningful size limit for ordinary use. If `-n` or `-c` is
given, the input is trimmed to the requested number of lines or
characters before being sent to the clipboard tool. A single trailing
newline is stripped by default, matching common expectations when
pasting into a text field. Use `-k` to preserve it.

## Limitations

* Requires an active display or session with a clipboard to copy into.
  It cannot copy into a clipboard on a headless server with no display
  attached, because no such clipboard exists there.
* Maximum input size is 512 MB, enforced as a safety ceiling against
  unbounded memory use, not a normal use limit.

## Contributing

Issues and pull requests are welcome at
[github.com/MaxEdgar/copy](https://github.com/MaxEdgar/copy).

## License

Released under the MIT license. See [LICENSE](LICENSE) for the full
text.

<div align="center">

Built by [MaxEdgar](https://github.com/MaxEdgar)

</div>
