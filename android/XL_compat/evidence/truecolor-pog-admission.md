# True-color POG audit and unfinished support

A Pumo resource audit found a real admission gap. The shared bitmap validator
checks indexed pixel bounds, but accepts the XL true-color flag. The XL POG
installer then initializes an indexed bitmap and drops that flag. Bounds
validation therefore did not establish faithful resource support.

The runtime still lacks true-color POG support. A trial preparation guard
rejects BM_FLAG_TGA (128), but also rejects the original Robotech Arena fixture
before its existing mechanics tests can start. The trial is not included in
production code. This increment records the defect and the next implementation
requirements; it must not be described as an implemented admission correction.

## Source and original evidence

The pinned XL revision is 2576a98b5de310e3ac7c9777df2677d1627b8bae:

- include/bitmap.h, SHA-256 9044979682a8e219fa0526cc092150e562d04d2d772b23ce1e63d164d58c7024, defines BM_FLAG_TGA as 128
- gameio/loadtextures.cpp, SHA-256 bf1ae25e058d6ca745e5833ea0e2c0e425c5716571f6dc5f109a49d67c5452b6, preserves that flag, selects four bytes per pixel and interprets height as width times the serialized height byte when width exceeds 256

The source then uses the TGA pixel reader and derives animation frame counts.
These dimensions, pixels, transparency and animation semantics need their own
implementation and live output evidence before lifting the restriction.

Pumo contains 26 flagged replacements in five original POGs: pmlev1 (5), pmlev2
(5), pmlev3 (5), pm-hub (6) and tulev2 (5). Every header has width 1024 and height
byte 1. XL interprets each as a 1024-square RGBA frame; the indexed installer
would interpret it as 1024 by 1. The original HOG remains
6e3e723e549897188a8fdc19db8b04630de9d428d2b31f8e3f48af108915b043.
Header/resource observations do not qualify their actual application.

Original Robotech Arena also has 19 flagged replacements in its 1196424-byte
rtarena.pog. Its HOG remains
adbfea019c1ac29ac922e15876c22ca6ce30eb02d598a98e6ef7dfdce2b2dd3e.
The prior controlled mechanics results do not establish correct texture pixels,
dimensions, transparency or original presentation. The existing experimental
water profile already leaves rendering unqualified; this audit identifies a
specific reason and prevents those tests from being used as texture evidence.

This is also a reminder that the level inventory is not the complete package
inventory. Pumo has twelve HXM files and fifteen INI files. Its INIs request
the standard medium ship, disable automap teleport, and enable 3D powerups;
mvlev5 additionally specifies secret saving. Those settings require an audit of
source defaults, consumers, ownership and transitions. Their mere presence is
not evidence that each introduces a new unsupported gameplay requirement.

## Reproduction and next implementation

An added native one-pixel RGBA control reproduces the current defect with
"unimplemented RGBA POG accepted as indexed pixels". The trial regression also
contains a 512-square frame-strip case, ordinary loading, unchanged live state
and indexed recovery, but that complete sequence is not yet validated: adding
the guard rejects original Robotech Arena at startup first. Do not count the
trial as passing new regression coverage.

The trial guard passes the separate native Neptune/HXM controls and builds in
the normal Android binary. Those results do not resolve the failed Arena case.
The uncommitted code experiment is preserved locally as
temp/xl-wall-keys-20261028/pog-admission-prototype.patch, with its failing logs;
the published runtime and test code remain unchanged.

Complete owned RGBA decoding, dimensions/frame ownership, transparency and
installation through the existing resource transaction. Check actual pixels,
visibility/collision consumers, animation, disk/rewind, graphics-context recovery
and Redux isolation before qualifying original POG application. Keep a useful
rejection for any remaining unsupported encoding. If a mechanics fixture is
split into an explicitly derived indexed variant during development, retain
the original's failed requirement independently; do not relabel it as an
original-mission pass or silently drop its authored resources.

No Pumo level is newly admitted, no original bytes are rewritten and no campaign
progression is claimed. The passing 96-case selection on the preceding wall-key
commit remains valid within its documented scope, but does not cover this newly
identified resource format. Green existing tests are not evidence that it works.
