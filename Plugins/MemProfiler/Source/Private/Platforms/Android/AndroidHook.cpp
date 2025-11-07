#if PLATFORM_ANDROID
#include "AndroidHook.h"
#include "xHook/xHook.h"


static bool bIntialized = false;

typedef void* (*malloc_t)(size_t size);
malloc_t Original_malloc = &malloc;
MallocEvent OnMallocEvent;
static void* My_malloc(size_t size)
{
    auto ptr = Original_malloc(size);

    if (bIntialized)
        OnMallocEvent(ptr,  size, 0);
    return ptr;
}

typedef void (*free_t)(void*);
free_t Original_free = &free;
FreeEvent OnFreeEvent;
static void My_free(void* ptr)
{
    Original_free(ptr);

    if (bIntialized)
        OnFreeEvent(ptr, 0);
}

typedef void* (*calloc_t)(size_t, size_t);
calloc_t Original_calloc = 0;
static void* My_calloc(size_t nmemb, size_t size)
{
    auto ptr = Original_calloc(nmemb,  size);
if (bIntialized)
    OnMallocEvent(ptr, nmemb * size, 0);
    return ptr;
}

typedef void* (*memalign_t)(size_t, size_t);
memalign_t Original_memalign;
static void* My_memalign(size_t align, size_t size)
{
    auto ptr = Original_memalign(align, size);
if (bIntialized)
    OnMallocEvent(ptr, size,0);
    return ptr;
}

using aligned_alloc_t = memalign_t;
aligned_alloc_t Original_aligned_alloc;
static void* My_aligned_alloc(size_t align, size_t size)
{
    auto ptr = Original_aligned_alloc(align, size);
if (bIntialized)
    OnMallocEvent(ptr, size,0);
    return ptr;
}

typedef int (*posix_memalign_t)(void**, size_t, size_t);
posix_memalign_t Original_posix_memalign;
static int My_posix_memalign(void** memptr, size_t align, size_t size)
{
    auto ret = Original_posix_memalign(memptr, align, size);
if (bIntialized)
    OnMallocEvent(*memptr, size,0);
    return ret;
}

typedef void* (*realloc_t)(void*, size_t);
realloc_t Original_realloc;
static void* My_realloc(void* ptr, size_t size)
{
    if (ptr != nullptr)
    {
    if (bIntialized)
        OnFreeEvent(ptr, 0);
    }
    ptr = Original_realloc(ptr, size);

    if (size > 0)
    {
    if (bIntialized)
        OnMallocEvent(ptr, size, 0);
    }

    return ptr;
}

#define CREATE_HOOK(Func) xhook_register("libUnreal",#Func,(void*)&My_##Func,(void**)&Original_##Func );

void FAndroidHook::StartHooking(MallocEvent OnMalloc, FreeEvent OnFree)
{
    OnMallocEvent = OnMalloc;
    OnFreeEvent = OnFree;
    
    CREATE_HOOK(malloc);
    CREATE_HOOK(free);
    CREATE_HOOK(memalign);
    CREATE_HOOK(aligned_alloc);
    CREATE_HOOK(posix_memalign);
    CREATE_HOOK(realloc);

    xhook_refresh(0);

    bIntialized = true;
}

void FAndroidHook::StopHooking()
{
    
}
PRAGMA_DISABLE_OPTIMIZATION
void* FAndroidHook::InternalMalloc(size_t Size)
{
	return Original_malloc(Size);
}

void FAndroidHook::InternalFree(void* Ptr)
{
	Original_free(Ptr);
}

PRAGMA_ENABLE_OPTIMIZATION
#endif