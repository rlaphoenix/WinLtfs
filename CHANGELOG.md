# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [1.2.0] - 2026-09-25

### Added

- Read-only tape attributes (MAM) exposed through WinFsp, with MAM discovery and paged attribute queries.
- Inno Setup installer bundling WinFsp.
- WinLtfs version in the `ltfs`, `mkltfs` and `ltfsck` startup logs and the generated `ltfs.conf`.

### Changed

- Attribute request operations now use SCSI service action numbering (0 reads a value, 1 lists IDs).
- Attribute value and list pages only read the bytes they need.

### Removed

- Python tools and test scripts.

### Fixed

- Target Windows 7 SP1 / Server 2008 R2 SP1.
- Block creation of the `$RECYCLE.BIN` folder on the mounted tape.
- Reject MAM queries when the backend lacks `read_mam`.
- Return `LTFS_NO_XATTR` for bad attribute IDs.
- Truncate MAM values to the buffer size like a real drive.
- Treat deleted MAM attributes as missing.
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
