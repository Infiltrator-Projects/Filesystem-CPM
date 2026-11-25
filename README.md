# Filesystem-CPM

Independent CP/M filesystem provider for InfiltratorOS. Target: one native C/C++ engine and one distributable `CPM.so`.

**Status: documentation only; source extraction pending.** [Central source](https://github.com/Infiltrator-Projects/Filesystems/tree/101026021d9088d80b7f0f472d9061f0e2c688a3/native/filesystems/cpm). Original cpmtools userspace source remains centrally, pinned at e534e20c15973a9559e981efb498a102020e5db7. No native module qualified. No independent driver or release exists.

## Common V2

Use only `infiltrator_filesystem_api_v2` from [Libraries](https://github.com/Infiltrator-Projects/Libraries). Own explicit diskdefs profiles, sector skew, directory extents, allocation blocks, user areas and filename attributes in the same engine for hosted and freestanding execution. No FUSE, separate per-format kernel/EFI driver or legacy ABI.

## Build and install

Planned: `/System/Filesystems/CPM.so` and `/System/Resources/Documentation/Filesystem-CPM.pdf`. Generic amd64 .deb, source-downloading CPU-native .run and GitHub-generated source ZIP only. No build or installer exists yet.

See [migration status](MIGRATION_STATUS.md) and [engineering philosophy](docs/PHILOSOPHY.md).
