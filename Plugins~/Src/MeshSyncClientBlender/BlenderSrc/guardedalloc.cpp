#include "pch.h"

#if BLENDER_VERSION >= 405

#include <cstddef>
#include <cstdlib>
#include <limits>
#ifdef _WIN32
#include <malloc.h>
#endif

namespace {

constexpr size_t kAlignedFlag = 1;

struct MemHeadAligned {
    short alignment;
    size_t size_and_flags;
};

static_assert(sizeof(MemHeadAligned) == 16,
              "Blender 5.0 x64 guarded allocator layout changed");
static_assert(alignof(MemHeadAligned) == 8,
              "Blender 5.0 x64 guarded allocator alignment changed");
static_assert(offsetof(MemHeadAligned, size_and_flags) == 8,
              "Blender 5.0 x64 guarded allocator fields changed");

void* platform_aligned_alloc(size_t size, size_t alignment)
{
#ifdef _WIN32
    return _aligned_malloc(size, alignment);
#else
    void* ptr = nullptr;
    return posix_memalign(&ptr, alignment, size) == 0 ? ptr : nullptr;
#endif
}

void platform_aligned_free(void* ptr)
{
#ifdef _WIN32
    _aligned_free(ptr);
#else
    std::free(ptr);
#endif
}

} // namespace

extern "C" {

static void msblen_mem_free(void* ptr)
{
    if (!ptr)
        return;

    auto* head = static_cast<MemHeadAligned*>(ptr) - 1;
    if ((head->size_and_flags & kAlignedFlag) == 0)
        return;

    const size_t padding = static_cast<size_t>(head->alignment) -
        sizeof(MemHeadAligned) % static_cast<size_t>(head->alignment);
    platform_aligned_free(reinterpret_cast<char*>(head) - padding);
}

static void* msblen_mem_malloc_aligned(size_t size, size_t alignment)
{
    alignment = alignment < sizeof(void*) ? sizeof(void*) : alignment;
    if ((alignment & (alignment - 1)) != 0 ||
        alignment > static_cast<size_t>(std::numeric_limits<short>::max()) ||
        size > std::numeric_limits<size_t>::max() - 3)
        return nullptr;

    size = (size + 3) & ~size_t(3);
    const size_t padding = alignment - sizeof(MemHeadAligned) % alignment;
    if (size > std::numeric_limits<size_t>::max() - padding - sizeof(MemHeadAligned))
        return nullptr;

    void* allocation = platform_aligned_alloc(
        size + padding + sizeof(MemHeadAligned), alignment);
    if (!allocation)
        return nullptr;

    auto* head = reinterpret_cast<MemHeadAligned*>(
        static_cast<char*>(allocation) + padding);
    head->alignment = static_cast<short>(alignment);
    head->size_and_flags = size | kAlignedFlag;
    return head + 1;
}

#if BLENDER_VERSION < 501
void MEM_freeN(void* ptr)
{
    msblen_mem_free(ptr);
}

void* MEM_mallocN_aligned(size_t size, size_t alignment, const char*)
{
    return msblen_mem_malloc_aligned(size, alignment);
}
#else
void MEM_delete_void(void* ptr)
{
    msblen_mem_free(ptr);
}

void* MEM_new_uninitialized_aligned(size_t size, size_t alignment, const char*)
{
    return msblen_mem_malloc_aligned(size, alignment);
}
#endif

} // extern "C"

#endif
