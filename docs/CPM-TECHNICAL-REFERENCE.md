# CPM: technical reference

## Origins and format families

Digital Research CP/M separates its machine-independent BDOS file operations from BIOS-specific storage access. CP/M 1.x, 2.2 and CP/M Plus use related but not identical disk and directory conventions. There is no universal boot-sector signature, fixed floppy geometry or self-describing superblock. A reader requires a trustworthy disk profile from a documented machine, image container or externally selected disk definition. Treating a directory that merely looks plausible as proof of format is unsafe. A writable mount needs unambiguous geometry; the provider must not guess among compatible profiles.

## Three different units

A CP/M logical record is always 128 bytes; the physical sector can be larger and contains several logical records. An allocation block is an independently chosen power-of-two multiple of 128 bytes. A track may contain multiple physical sectors with interleave or skew. An image may store sectors in logical, physical or container-defined order. Translate logical records into the proper track and sector, apply the selected skew and head order, then map to host bytes. Do not apply physical skew twice to images already in logical order. Reserved tracks contain boot/system material outside the allocation area.

## Disk parameter block (DPB)

The CP/M 2.2 DPB is external to ordinary disk contents. Offsets within the 15-byte DPB: SPT at 0 (16-bit count of 128-byte records per track); BSH at 2; BLM at 3; EXM at 4; DSM at 5 (16-bit final allocation block number); DRM at 7 (16-bit final directory slot); AL0 and AL1 at 9 and 10; CKS at 11 (16-bit checksum-vector length); OFF at 13 (16-bit reserved-track count). Multi-byte fields are little-endian. Allocation block bytes = 128*2^BSH; BLM should equal 2^BSH-1. DSM+1 is block count and DRM+1 is directory slot count. AL0/AL1 reserve the earliest directory blocks, high bit first. CKS is a BIOS/BDOS change-detection parameter, not a persistent allocation checksum.

The retained cpmtools decoder derives sector and track addressing from secLength, sectrk, boottrk and blksiz. Skew and user-area identity belong to the selected disk profile.

## Layout and block-address formula

For a simple linear image with no header or skew, allocation area starts at OFF*SPT*128 bytes; block b begins at that offset plus b*block_bytes. Directory begins at allocation block zero and occupies at least ceil((DRM+1)*32/block_bytes) blocks. The AL0/AL1 mask may reserve additional directory blocks. Check that all required directory bytes fit, that allocation block addresses do not exceed DSM and that the total mapped sectors fit the source image. Real profiles may require container offsets, explicit boot-sector counts, side ordering or track skew. Do not mistake the allocation area for the first payload block.

## Directory record and names

Every directory record is 32 bytes. Byte 0 is status or user number; bytes 1..8 hold padded filename; 9..11 hold extension; 12 is EX, 13 S1 or byte-count field according to revision, 14 S2, 15 RC, 16..31 allocation pointers. Mask attribute bits before comparing the native 7-bit name. CP/M 2.2 typically uses user areas 0..15 and 0xE5 for deleted/unused entries. The extension's high bits encode read-only, system and, on later systems, archive status. Names are space-padded 8.3 identities, normally uppercase. User area is part of the file key, not a hierarchical directory or a distinct user permission object.

## Logical and physical extents

A logical extent represents 128 records = 16384 bytes. The 16-byte allocation array contains sixteen 8-bit block numbers when DSM is at most 255, or eight little-endian 16-bit block numbers for larger ordinary profiles. Its byte capacity can hold one or several logical extents. EXM masks the low logical extent bits contained in one physical directory entry; let e = 32*(S2 masked for the admitted revision)+ (EX masked to its low five bits). Physical-entry ordinal = floor(e/(EXM+1)); first logical extent = e-(e mod (EXM+1)). The occupied record count in that entry is (e mod (EXM+1))*128+RC, where RC ranges from zero through 128. CP/M 2.2 and Plus differ in maximum extent numbers; use revision-specific high-bit rules. Do not confuse physical directory entries with contiguous disk extents.

## Worked address and length example

Consider a simple CP/M 2.2 profile with 1 KiB blocks, 8-bit pointers, EXM=0, 26 records per track, two reserved tracks, 64 directory slots. Data area offset is 6656 bytes; directory consumes 2048 bytes in blocks 0 and 1. File entries with EX=0, S2=0, RC=128 and EX=1, S2=0, RC=24 describe a file of 152 records or 19456 bytes at record granularity. In the second entry, pointer 5 denotes offset 6656+5120=11776 in a linear logical image. Its third pointer, if 9, denotes offset 15872. A real physical-sector image may need a profile-specific skew transformation before applying these byte positions. Do not infer the exact last byte of a binary file from CP/M 2.2 record counts.

## Reading, sparse records and metadata

Group live entries by user area and canonical masked name, order by physical extent ordinal, reject conflicting duplicates and impossible record counts, and map each logical allocation block to its pointer. A zero allocation pointer represents unallocated storage, not data block zero. CP/M random-record access can have holes; preserve the original missing-record semantics instead of silently treating them as allocated zero data. File size is derived from the highest valid extent and its record count; CP/M Plus can supply last-record byte information, depending on revision. A text EOF marker such as 0x1A is a convention, not a universal binary length delimiter. Preserve unknown attribute flags and metadata record types.

## Allocation, directory capacity and writes

Ordinary CP/M rebuilds the allocation vector by scanning directory pointers rather than reading a persistent free-block bitmap. Mark directory-reserved blocks before processing live files; reject duplicate ownership, out-of-range pointers and references into reserved blocks. Directory slots and free blocks are independent capacity limits. To create or grow a file, allocate suitable blocks and physical directory extent entries, write record data, then publish coherent directory metadata; ordinary CP/M offers no general transaction journal. Exact native update and recovery ordering must be specified and tested for the chosen revision. Rename all extent records consistently. Erasing a file changes status to 0xE5; old bytes and pointers may remain until reused.

## CP/M Plus and variants

CP/M Plus extends the directory with label, password and timestamp records; compatible systems sometimes use the same status values with different semantics. Status 0x20 and 0x21 are not ordinary file-user values in the relevant Plus profiles. Timestamp records can describe preceding directory slots, and their placement is therefore significant during compaction. A profile must define whether S1 is reserved or stores byte-count information, the exact extent masks, maximum name/user values and timestamp format. Do not treat passwords as ordinary file metadata or silently discard an unknown protection record. A generic reader should refuse unrecognized directory types for writes.

## Forensics, format and validation

A formatter requires an exact media profile, reserved-track policy, initial directory state, allocation count and directory reservation; it must not invent a boot image. Reconstruct a read-only ownership map from live entries, then compare with independent tools. Deleted entries are evidence only: recovery is possible only if all needed pointers and data remain intact and unclaimed. A defragmenter must relocate actual allocation blocks and update every owning extent without violating file record order or timestamp-slot rules. Test empty files, 128/129-record transitions, EXM>0, 8- and 16-bit pointers, multiple users, duplicate names, sparse records, full directory, reserved blocks, skewed and unskewed images, deleted records, corrupt extents and interrupted writes.

## Implementation and qualification boundary

The independent provider uses Common V2: https://github.com/Infiltrator-Projects/Libraries/blob/main/docs/FILESYSTEM_COMMON_ABI_V2.md . Negotiate actual operation masks and media profiles. Distinguish unsupported, corrupt, I/O and stale-handle results. Require bounded host I/O, exclusive unmounted ownership and durable flush before mutation. Test Media and Defragmenter consume the same engine, not separate format parsers. Hosted and freestanding binaries, relocation, recovery, BOOT, kernel integration and packages require separate exact-source evidence. Current support belongs to the repository README and qualification records. The intended package paths are /System/Filesystems/ and /System/Resources/Documentation/; the reference is not an installation or release-status claim.

## Primary sources and implementation boundary

Digital Research, CP/M Operating System Manual, July 1982: https://bitsavers.org/pdf/digitalResearch/cpm/CPM_Operating_System_Manual_Jul82.pdf . CP/M 2.2 and Plus format cross-checks: https://www.seasip.info/Cpm/format22.html and https://www.seasip.info/Cpm/format31.html . Independent cpmtools definitions: https://manpages.debian.org/testing/cpmtools/cpm.5.en.html and https://manpages.debian.org/unstable/cpmtools/diskdefs.5.en.html . For provider availability and qualification, use Implementation and qualification boundary and its referenced repository evidence. Exact machine-specific sector profiles, Plus password/time encoding and write ordering require verification before claiming a complete independent writer.

The imported cpmtools baseline is lipro-cpm4l/cpmtools e534e20c15973a9559e981efb498a102020e5db7.

Retained source: https://github.com/Infiltrator-Projects/Filesystems/blob/101026021d9088d80b7f0f472d9061f0e2c688a3/native/filesystems/cpm/userspace/cpmfs.c (blob 5de3cff97a).
