# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Changed

- Identify as WinLtfs 1.2.0 instead of HPE StoreOpen 3.5.0 in tape labels, indexes, the cartridge memory
  (Application Vendor/Version), the `ltfs.software*` attributes, and startup logs.
- Default config file moved from `C:\ProgramData\HPE\LTFS\ltfs.conf` to `C:\ProgramData\WinLtfs\ltfs.conf`,
  so an installed HPE StoreOpen config (pointing at HPE's plugins) is never picked up by accident.

## [1.2.0] - 2026-09-25

### Added

- Read-only tape attributes (MAM) exposed through WinFsp, with MAM discovery and paged attribute queries.
- Inno Setup installer bundling WinFsp.
- WinLtfs version in the `ltfs`, `mkltfs`, `unltfs` and `ltfsck` startup logs.

### Fixed

- Target Windows 7 SP1 / Server 2008 R2 SP1.
- Block creation of the `$RECYCLE.BIN` folder on the mounted tape.
- Clear stale sense data before every SCSI command.
- `strcasestr` is now case-insensitive.

## [1.1.1] - 2026-09-25

### Fixed

- Ignore mount manager prefixes when getting the drive letter.

## [1.1.0] - 2026-09-20

### Added

- Drive label and icon for mounted tapes.
- LTO tape drive support matrix in the README.

### Fixed

- Link plugins with relative paths so the distribution works wherever it is extracted.

## [1.0.0] - 2026-09-16

Initial release.

[Unreleased]: https://github.com/rlaphoenix/WinLtfs/compare/v1.2.0...HEAD
[1.2.0]: https://github.com/rlaphoenix/WinLtfs/compare/v1.1.1...v1.2.0
[1.1.1]: https://github.com/rlaphoenix/WinLtfs/compare/v1.1.0...v1.1.1
[1.1.0]: https://github.com/rlaphoenix/WinLtfs/compare/v1.0.0...v1.1.0
[1.0.0]: https://github.com/rlaphoenix/WinLtfs/releases/tag/v1.0.0
