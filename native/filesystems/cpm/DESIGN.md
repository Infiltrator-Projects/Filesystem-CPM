# CPM — native filesystem design

## Format scope

CP/M disk profiles; no universal magic-based identity. This is a design contract, not a claim of implemented capabilities.

## On-disk structures and ownership

Explicit disk definitions, sector skew, reserved tracks, allocation-block width, directory extent numbering, user areas and filename attributes. The single native C/C++ engine owns these format semantics; consumers provide bounded Common V2 host services, never competing parsers.

## Integrity and mutation invariants

Require a validated profile before interpreting ambiguous media. Check allocation references, extent chains and record counts, and avoid mixing user areas. Mutation must preserve extent and allocation accounting; no assumed journal. All arithmetic, offsets and lengths are checked before I/O. Writable operations require exclusive access, explicit ordering, flush and interruption/recovery evidence. Read-only operations leave the medium unchanged.

## Common V2 boundary

Only `infiltrator_filesystem_api_v2` is public. Target the genuine PROBE/MOUNT/UNMOUNT/STAT/LIST/READ operations first; negotiate actual supported operation masks. VOLUME, METADATA and DEFRAG operations (including MAP, PLAN, EXECUTE, VERIFY, RECOVER) are individually qualified or refused. Test Media uses the same provider's qualified format and namespace operations. No FUSE, legacy ABI, private driver or kernel proxy. Hosted and import-free freestanding profiles share the format engine, with generic environment-specific host services.

## Qualification

Use independent cpmtools-generated media for multiple profiles; test extent rollover, deleted entries, duplicate blocks and ambiguous geometry. Source baseline lipro-cpm4l/cpmtools e534e20c15973a9559e981efb498a102020e5db7. Compare file bytes against an independent oracle, hash unchanged read-only images, inject short I/O, corruption and interruption; validate exact published module separately in BOOT and kernel. Use disposable media only. No new independent binary or test pass is claimed.

## Pinned source-level migration evidence

[Original editable source](https://github.com/Infiltrator-Projects/Filesystems/blob/101026021d9088d80b7f0f472d9061f0e2c688a3/native/filesystems/cpm/userspace/cpmfs.c) (blob 5de3cff97a…). The imported cpmtools code accounts for 32-byte directory records, reserves the directory's allocation blocks, derives sector/track addressing from secLength, sectrk, boottrk and blksiz, and recognizes deleted record status 0xe5. Preserve diskdefs profile semantics, including skew, extent numbering and user-area identity. Reject ambiguous profile matches instead of assuming a universal on-media magic. Qualify file lookup and extent ordering across actual profiles before modifying directory entries.

## Common V2 engineering and release acceptance

- A valid CORE descriptor with a real PROBE is mandatory. Every optional operation has its own capability bit and non-null qualified callback; unavailable operations have neither. Recognised format, unsupported feature, corruption, host I/O failure and stale handles remain distinct outcomes.
- Use Common's sector-aligned bounded byte helpers. Validate partition extent, logical/physical sector geometry and maximum transfer before reading. Serialize overlapping read/modify/write sectors; request explicit flush barriers. Never infer exclusivity from flags alone; require real exclusive unmounted ownership before formatting or raw mutation. Require the Common entropy service when the format needs a unique identity.
- Qualify hosted and import-free freestanding profiles separately; enforce module identity and trust, exact export set and actual packaged bytes. BOOT and kernel are generic loaders; BOOT uses the same file in `/EFI/InfiltratorOS/BOOT/drivers_x64/`, with the installed canonical copy in `/System/Filesystems/`. No per-format .ko/.efi, FUSE, proxy, legacy ABI, external formatter or consumer-side parsing substitutes for the engine.
- Test Media must format and populate through actual V2 FORMAT/CREATE/WRITE operations; an independent formatter/reader may be a test oracle only, never production fallback. Defragmentation maps, plans, execution, verification and recovery require independent proof of ownership, data preservation and interruption safety. Where not implemented they remain absent.
- New build and implementation code is native C/C++ and shell as appropriate, no Python. The generic amd64 DEB and CPU-native source-downloading RUN are the only manually published release assets; GitHub generates source archives. RUN fetches exact versioned source and pinned Common dependency, verifies integrity, compiles natively and installs the selected package; no source archive or DEB embedded. Release immutable exact-main qualification only. No release exists yet.

## Source and migration

[Central source and historical design, pinned revision](https://github.com/Infiltrator-Projects/Filesystems/tree/101026021d9088d80b7f0f472d9061f0e2c688a3/native/filesystems/cpm). Adapt the retained implementation without erasing inherited notices or licences. New project-authored code uses GPL-3.0-or-later. See [migration status](../../../MIGRATION_STATUS.md), [philosophy](../../../docs/PHILOSOPHY.md) and [Common V2 contract](https://github.com/Infiltrator-Projects/Libraries/blob/main/docs/FILESYSTEM_COMMON_ABI_V2.md). Source migration is a separate step.
