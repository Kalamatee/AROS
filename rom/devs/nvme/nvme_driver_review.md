# NVMe Driver Review

## Overview
This document records issues observed while reviewing the current NVMe driver implementation in `rom/devs/nvme`, together with suggestions for improving robustness, performance, and feature coverage (notably scatter/gather list support).

## Findings and Recommendations

### 1. Command identifier handling is hard-coded to 16 entries
`nvme_alloc_cmdid()` wraps the identifier after 16 commands and the queue setup allocates completion hook storage for only 16 slots, regardless of the actual submission queue depth (the admin queue is created with depth 64 and IO queues follow the controller-reported depth).【F:rom/devs/nvme/nvme_hw.c†L24-L46】【F:rom/devs/nvme/nvme_controllerclass.c†L192-L205】【F:rom/devs/nvme/nvme_busclass.c†L341-L387】 This results in command-id reuse while earlier requests are still outstanding once the queue depth exceeds 16, which can corrupt completions.

*Recommendation:* Size the hook/handler arrays to `nvmeq->q_depth` and treat command identifiers as a ring of that size. Track outstanding entries (e.g. a bitmap or free-list) to avoid reissuing an ID until its completion arrives.

*Implementation sketch:*
- Extend `struct nvme_queue` (in `nvme_intern.h`) with a dynamically sized bitmap or byte array and maintain a `next_cmdid` cursor.
- Populate the array from `nvme_alloc_queue()` by allocating `q_depth` slots for hooks, handlers, and optional per-command storage.
- Rework `nvme_alloc_cmdid()` to scan for a free slot while holding the queue lock, mark it busy, and return `-1` when all slots are in use so callers can back off or wait.
- Release the slot in `nvme_complete_event()` once the completion handler finishes, ensuring the unlock happens after the hook returns so handler state remains valid.

### 2. Submission queues are not flow-controlled
`nvme_submit_cmd()` advances the submission tail unconditionally and never checks whether the queue is full or whether the completion head has caught up.【F:rom/devs/nvme/nvme_hw.c†L49-L77】 When the queue fills, the driver will overwrite commands that are still pending, leading to lost or corrupted requests.

*Recommendation:* Track `sq_head`/`sq_tail` modulo the queue depth and block (or fail) when `(tail + 1) % depth == sq_head`. Consider per-queue wait lists so tasks sleep until space is available.

*Implementation sketch:*
- Record queue occupancy (`outstanding` counter or reuse the command-id bitmap) in `nvme_queue` so the submission path knows how many slots remain.
- Update `nvme_submit_cmd()` to calculate the next tail entry under the queue lock and bail out (or sleep on a queue-local signal) if the submission queue is full.
- Have `nvme_complete_event()` signal waiters after freeing a slot so producers blocked on a full queue can resume.

### 3. Excessive interrupt disabling during command submission
`nvme_alloc_cmdid()` and `nvme_submit_cmd()` wrap their critical sections in `Disable()/Enable()` even though spinlocks are also taken on SMP builds.【F:rom/devs/nvme/nvme_hw.c†L31-L75】 This globally masks interrupts, hurting latency and scalability.

*Recommendation:* Use the spinlock alone on SMP and rely on per-queue locking on UP. If interrupt masking is truly needed, use `Forbid()/Permit()` scoped to the queue instead of disabling all interrupts.

*Implementation sketch:*
- Introduce helper macros that acquire `nvmeq->q_lock` when SMP is enabled and fall back to `Forbid()/Permit()` elsewhere.
- Replace the `Disable()/Enable()` pairs in `nvme_alloc_cmdid()` and `nvme_submit_cmd()` with the new helpers so interrupts remain enabled during critical sections.
- Audit other hot paths (e.g. completion processing) to ensure they use the same synchronization primitives for consistency.

### 4. PRP construction violates NVMe rules and uses virtual addresses
`nvme_initprp()` derives PRP entries from the request’s virtual address and permits non-zero offsets in PRP2 (and subsequent list entries), despite the specification requiring page-aligned physical addresses for every entry after PRP1.【F:rom/devs/nvme/nvme_prp.c†L58-L149】 Because the driver never translates to physical addresses (nor accounts for IOMMUs), controllers will DMA to meaningless locations. Additionally, the optional PRP list is allocated with `AllocMem()`, which does not guarantee DMA-accessible memory.

*Recommendation:* Obtain DMA mappings for the buffer (e.g. via PCI HIDD DMA helpers) and write physical addresses into PRP entries. For two-page transfers, force PRP2 to the start of the second page (offset zero). When a PRP list is needed, allocate it from PCI-visible memory and keep it cache coherent.

*Implementation sketch:*
- Use the PCI HIDD DMA helper (`HIDD_PCIDriver_MapVirtual()` / `pciGetPhysical()` equivalent) to translate the Exec buffer into physical page addresses before filling PRP fields.
- Round PRP2 and subsequent list entries down to the nearest page boundary; keep the byte offset exclusively in PRP1 as mandated by the specification.
- Allocate PRP list pages from PCI-visible memory (for example via `HIDD_PCIDriver_AllocPCIMem()`), store their physical addresses in PRP2, and perform `CachePreDMA()/CachePostDMA()` on both the list and payload buffers.

### 5. Completion bookkeeping copies handlers instead of referencing them
`nvme_submit_iocmd()` copies the caller-provided handler structure into a per-ID array and then points `cehandlers[cmdid]` back to that array slot.【F:rom/devs/nvme/nvme_queue_io.c†L96-L110】 Because the array is shared and only 16 entries deep, concurrent operations can clobber handler state and the driver never records ownership for more than 16 outstanding commands.

*Recommendation:* Keep a single `completionevent_handler` per outstanding command (again sized to the queue depth) and fill it in place without extra copies.

*Implementation sketch:*
- Embed a per-command `completionevent_handler` array inside each queue (allocated in `nvme_alloc_queue()`).
- Adjust `nvme_submit_iocmd()` so it copies the caller's handler into the queue-owned slot (`cmdid` index) and simply stores a pointer to that slot in `cehandlers`.
- Update the completion path to clear the slot (and release any DMA bounce buffers) before marking the command-id free.

### 6. Missing memory ordering for doorbell writes
No memory barrier is placed between copying the command to the submission queue and ringing the doorbell.【F:rom/devs/nvme/nvme_hw.c†L65-L70】 On weakly ordered architectures the controller might see the doorbell update before the command contents become visible.

*Recommendation:* Insert a write memory barrier (`MemoryBarrier()`, `KrnStoreFence()`, etc.) before writing the doorbell register.

*Implementation sketch:*
- Drop a compiler- and architecture-friendly store fence (`__sync_synchronize()` or `AROS_MEMORY_BARRIER()`) right after the `CopyMem()` into the submission queue but before updating the tail doorbell.
- Wrap the barrier in a helper macro in `nvme_hw.h` so other call sites (e.g. admin queue submissions) can reuse it if needed.

### 7. Scatter/gather support is stubbed out
`nvme_initsgl()` is unimplemented and always fails, forcing every transfer to be physically contiguous in memory and limiting usable IO sizes.【F:rom/devs/nvme/nvme_sgl.c†L43-L46】 Since `nvme_sector_rw()` does not fall back when SGL setup fails, multi-segment buffers currently cause `IOERR_BADADDRESS`.

*Recommendation:* Implement `nvme_initsgl()` by walking the Exec scatter/gather structures (or building them from `struct MemList`) and emitting a chain of SGL descriptors. Wire it into `nvme_sector_rw()` so the driver chooses between PRP and SGL based on controller capabilities and buffer layout.

*Implementation sketch:*
- Detect whether `io_Data` references a flat buffer or a `struct MemList`; fall back to PRPs for the former when the transfer fits within MDTS and page alignment allows.
- Teach `nvme_initsgl()` to iterate over the Exec scatter/gather list, map each segment to a physical address, and emit either keyed-data or unkeyed SGL descriptors in a queue-owned DMA buffer.
- Extend `nvme_sector_rw()` so it first attempts PRP setup, then calls `nvme_initsgl()` when PRPs are unsuitable (non-contiguous physical pages or transfers exceeding MDTS) and frees the SGL DMA buffer in the completion hook.

### 8. Resource management gaps
The driver allocates a fresh PRP list buffer for every request that straddles three or more pages and frees it on completion.【F:rom/devs/nvme/nvme_prp.c†L95-L149】【F:rom/devs/nvme/nvme_queue_io.c†L69-L79】 This introduces significant allocation overhead in the IO path.

*Recommendation:* Maintain per-queue DMA pools for PRP/SGL lists to avoid repeated allocations, and consider reusing command structures for better cache locality.

*Implementation sketch:*
- Create per-queue caches (e.g. small Exec pools) for PRP list pages and SGL descriptor blocks sized to the queue depth.
- Hand out descriptors from the pool in the submission path and return them in the completion handler, falling back to `AllocMem()` only when the pool is temporarily exhausted.
- Keep frequently reused command templates (identify, flush, etc.) in queue-local storage so I/O hot paths avoid repeated `memset()` calls.

## Scatter/Gather Enablement Outline
1. Detect controller support via the Identify data and the optional command set fields (SGLS bit). Wire this into feature negotiation during controller bring-up.
2. Teach `nvme_sector_rw()` to select PRP vs. SGL dynamically. Attempt PRP first when the buffer is naturally contiguous and falls within MDTS. Fall back to SGL for non-contiguous buffers or very large transfers.
3. Implement `nvme_initsgl()` to translate the OS scatter/gather representation into NVMe SGL descriptors, ensuring descriptor chains obey controller alignment and length limits. Use DMA-safe allocations and cache maintenance similar to the PRP path.
4. Update completion handling to release any SGL list storage alongside the current PRP clean-up.

## Additional Ideas
- Add structured error logging for NVMe status codes (e.g. decode SCT/SC in `nvme_complete_ioevent()` and emit them through `bug()` or a device-specific logger) and plumb the decoded result into extended IO error values so callers can react programmatically.
- Wire up asynchronous completion polling and distribute queue interrupts across CPUs by enabling MSI-X vector affinity, allowing the queue tasks created in `nvme_busclass.c` to process completions on the CPU that submitted the I/O.

