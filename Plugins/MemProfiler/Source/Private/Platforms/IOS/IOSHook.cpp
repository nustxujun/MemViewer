#if PLATFORM_IOS
#include "IOSHook.h"
#include "IOSInternalAllocator.h"

#include <sys/mman.h>
#include <mach/mach.h>
#include <mach/vm_map.h>
#include <dlfcn.h>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>


typedef void (malloc_logger_t)(uint32_t type, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3, uintptr_t result, uint32_t num_hot_frames_to_skip);
extern malloc_logger_t* malloc_logger;
extern malloc_logger_t* __syscall_logger;
static malloc_logger_t* orig_malloc_logger;
static malloc_logger_t* orig_syscall_logger;

#define memory_logging_type_free 0
#define memory_logging_type_generic 1 /* anything that is not allocation/deallocation */
#define memory_logging_type_alloc 2 /* malloc, realloc, etc... */
#define memory_logging_type_dealloc 4 /* free, realloc, etc... */
#define memory_logging_type_vm_allocate 16 /* vm_allocate or mmap */
#define memory_logging_type_vm_deallocate 32 /* vm_deallocate or munmap */
#define memory_logging_type_mapped_file_or_shared_mem 128

MallocEvent OnMallocEvent = nullptr;
FreeEvent OnFreeEvent = nullptr;


static void alloc_hooker(uint32_t type_flags, uintptr_t zone_ptr, uintptr_t arg2, uintptr_t arg3, uintptr_t return_val, uint32_t num_hot_to_skip)
{
	uintptr_t size = 0;
	uintptr_t ptr_arg = 0;


	uint32_t alias = 0;
	VM_GET_FLAGS_ALIAS(type_flags, alias);
	// skip all VM allocation events from malloc_zone
//	if (alias >= VM_MEMORY_MALLOC && alias <= VM_MEMORY_MALLOC_NANO) {
//		return;
//	}

	if (FIOSIntenralAllocator::InternalZone == zone_ptr)
		return;

	if ( alias != 0 && alias != VM_MEMORY_IOACCELERATOR && alias != VM_MEMORY_IOSURFACE)
		return;

	// skip allocation events from mapped_file
	if (type_flags & memory_logging_type_mapped_file_or_shared_mem) {
		return;
	}

	// check incoming data
	if ((type_flags & memory_logging_type_alloc) && (type_flags & memory_logging_type_dealloc)) {
		size = arg3;
		ptr_arg = arg2; // the original pointer
		if (ptr_arg == return_val) {
			return; // realloc had no effect, skipping
		}
		if (ptr_arg == 0) { // realloc(NULL, size) same as malloc(size)
			type_flags ^= memory_logging_type_dealloc;
		}
		else {
			// realloc(arg1, arg2) -> result is same as free(arg1); malloc(arg2) -> result
			alloc_hooker(memory_logging_type_dealloc | alias, zone_ptr, ptr_arg, (uintptr_t)0, (uintptr_t)0, num_hot_to_skip + 1);
			alloc_hooker(memory_logging_type_alloc | alias, zone_ptr, size, (uintptr_t)0, return_val, num_hot_to_skip + 1);
			return;
		}
	}

	if ((type_flags & memory_logging_type_dealloc) || (type_flags & memory_logging_type_vm_deallocate)) {

		size = arg3;
		ptr_arg = arg2;
		if (ptr_arg == 0) {
			return; // free(nil)
		}
		OnFreeEvent((const void*)ptr_arg, alias);
	}
	if ((type_flags & memory_logging_type_alloc) || (type_flags & memory_logging_type_vm_allocate)) {
		if (return_val == 0 || return_val == (uintptr_t)MAP_FAILED) {
			return;
		}
		size = arg2;

		OnMallocEvent((const void*)return_val, size, alias);
	}
}


static void malloc_hooker(uint32_t type_flags, uintptr_t zone_ptr, uintptr_t arg2, uintptr_t arg3, uintptr_t return_val, uint32_t num_hot_to_skip)
{
	if (orig_malloc_logger)
		orig_malloc_logger(type_flags, zone_ptr, arg2, arg3, return_val, num_hot_to_skip);
	alloc_hooker(type_flags, zone_ptr, arg2, arg3, return_val, num_hot_to_skip);
}

static void syscall_hooker(uint32_t type_flags, uintptr_t zone_ptr, uintptr_t arg2, uintptr_t arg3, uintptr_t return_val, uint32_t num_hot_to_skip)
{
	if (orig_syscall_logger)
		orig_syscall_logger(type_flags, zone_ptr, arg2, arg3, return_val, num_hot_to_skip);
	alloc_hooker(type_flags, zone_ptr, arg2, arg3, return_val, num_hot_to_skip);
}


void FIOSHook::StartHooking(MallocEvent OnMalloc, FreeEvent OnFree)
{
	OnMallocEvent = OnMalloc;
	OnFreeEvent = OnFree;

	orig_malloc_logger = malloc_logger;
	orig_syscall_logger = __syscall_logger;

	malloc_logger = malloc_hooker;
	__syscall_logger = syscall_hooker;
}

#endif