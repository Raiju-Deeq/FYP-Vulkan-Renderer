/**
 * @file VmaImplementation.cpp
 * @brief Single translation unit that provides Vulkan Memory Allocator symbols.
 *
 * VMA is a header-only style library, but exactly one `.cpp` file in the
 * program must define `VMA_IMPLEMENTATION` before including `vk_mem_alloc.h`.
 * Keeping that here avoids accidental multiple-definition linker errors when
 * more Milestone 2 files start including VMA for buffer and image handles.
 */

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>
