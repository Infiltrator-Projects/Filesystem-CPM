# Filesystem-CPM

Native Common V2 CP/M Plus read-only development provider. The first supported profile is linear 180 KiB Amstrad PCW180 geometry explicitly identified by the documented JCE CP/M/DSK boot-spec extension. Unmarked raw images are not autodetected because CP/M has no universal magic and external disk geometry is not available in the existing Common V2 mount contract.

The engine validates directory records and actual block ownership, groups physical extents, reads exact last-record byte counts, provides CORE probe/mount/stat/list/read and measures VOLUME free space. User areas 00–15 are exposed as logical directories; these are not hierarchical on-disk directories. Physical fragmentation is supported; password-protected, sparse/random-record, alternate geometry and unqualified character profiles are refused. Stable exclusive read-only host access is required. No write, format, defragmentation, BOOT or kernel qualification is claimed.

The native and import-free implementations are tested against populated fixtures and independent cpmtools-created media with a documented boot-spec extension applied in reserved sectors. cpmtools is a test oracle, not a runtime dependency. The published v0.0.1 documentation release remains current pending runtime qualification.

[Manual](docs/CPM.pdf) · [Technical reference](docs/CPM-TECHNICAL-REFERENCE.md) · [Migration status](MIGRATION_STATUS.md) · [Native design](native/filesystems/cpm/DESIGN.md) · [Common V2](https://github.com/Infiltrator-Projects/Libraries)
