# Changelog

All notable changes to this project are documented in this file.

## 1.1.0

### Added

- `-n, --lines N` option to copy only the last N lines of input, matching
  `tail -n` behavior.
- `-c, --chars N` option to copy only the last N characters of input.
- `-p, --print` option to also print the copied text to standard output.
- `-k, --keep-newline` option to preserve a trailing newline (stripped by
  default).
- `-h, --help` and `-v, --version` flags.
- Manual page (`man copy`).
- Makefile with `install` and `uninstall` targets for building from
  source on any distribution.

### Changed

- Input buffer now grows dynamically instead of using a fixed 16 MB cap.
  A 512 MB safety ceiling is enforced to prevent unbounded memory use;
  if reached, `copy` errors out and copies nothing rather than silently
  truncating input.
- `SIGPIPE` is now ignored and `EPIPE` from a crashed or early-exiting
  clipboard backend is handled gracefully instead of killing the process.

## 1.0.0

### Added

- Initial release.
- Pipes stdin to the clipboard, auto-detecting X11 (`xclip`, `xsel`),
  Wayland (`wl-copy`), WSL (`clip.exe`), macOS (`pbcopy`), and Termux
  (`termux-clipboard-set`) backends.
- Fixed 16 MB input buffer.
