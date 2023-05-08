This is a collection of unofficial, unvetted, unverified tooling I've written for binja.

It is not associated or affiliated with Vector35 in any capacity.

## UI Tooling

### RightSideBar

Thin sidebar pinned to the right side of the window.

Where new UI tooling added by this plugin goes.

#### ARM Manual

Embedded ARM Manual in the right-sidebar

## Binary Views

### KernelCacheView

Sketch for Prelinked KernelCache Support

### SharedCacheView

Sketch for Shared Cache Support integrated as a native BinaryView

## Workflows

### SUAP (Shut up about PAC)

This is a workflow that lifts all PAC intrinsics as nops. 

It results in actually clear and readable MLIL/HLIL in binaries protected by pointer auth.

### SUASIMD (Shut up about SIMD)

This workflow "fixes" "issues" with SIMD lifting. (i.e. a single instruction getting lifted to 16 HLIL ops)

It primarily tries to improve readability of code containing SIMD instructions at the cost of perfect accuracy in SIMD lifting.


