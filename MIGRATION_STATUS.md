# CPM Common V2 migration

An independent native read-only implementation now exists for the explicitly marked PCW180 profile. The retained [cpmtools source](https://github.com/Infiltrator-Projects/Filesystems/tree/101026021d9088d80b7f0f472d9061f0e2c688a3/native/filesystems/cpm) is the reference for the JCE boot specification, CP/M directory and exact record length semantics; original notices remain intact. New code is GPL-3.0-or-later.

Native directory/extent validation, namespace, fragmented payload reads and allocation accounting are implemented. Exact-revision CI must establish their correctness on independent populated media and constructed corrupt fixtures. Other diskdefs, ambiguous unmarked media, skewed/container geometry, password semantics, sparse records, mutation, format, recovery, defragmentation and physical BOOT/kernel qualification remain outstanding. The current published release installs only the reference PDF.

[Exact development CI 46121597000](https://github.com/Infiltrator-Projects/Filesystem-CPM/actions/runs/46121597000) passed all defined tests. The runtime release is separately gated on the new revision's tests, source reconstruction and installed package verification.
