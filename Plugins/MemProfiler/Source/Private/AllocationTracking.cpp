//#if ENABLE_LOW_LEVEL_MEM_TRACKER
#include "AllocationTracking.h"
#include "HAL/PlatformFilemanager.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "Async/ParallelFor.h"
#include "Misc/CoreDelegates.h"
#include "Containers/Queue.h"
#include "Containers/SortedMap.h"


#include <map>
#include <atomic>
#include <string>

#if PLATFORM_IOS 
#include <sys/mman.h>
#include <mach/mach.h>
#include <mach/vm_map.h>
#include <dlfcn.h>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#elif PLATFORM_ANDROID
#include <sys/mman.h>
#include <dlfcn.h>
#include "Android/AndroidPlatformFile.h"
#elif PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

struct AllocInfo
{
	uint64 Ptr;
	uint32 Size;
	uint32 Trace;
	uint32 Begin;
	uint32 Tag;
};

struct DeallocInfo
{
	uint64 Ptr;
	uint32 End;
	uint32 Tag;
};

auto SerializeStr = [](auto& Ar, const FString& Str){
	int StrLen = Str.Len();
	Ar << StrLen;
	Ar.Serialize(TCHAR_TO_ANSI(*Str), StrLen);
};

struct AllocTrackState
{
	FString FolderPath;
	std::atomic_bool bInitialized = false;
	int TrackerType = 1; // ELLMTracker::Default
	uint32 SizeLimit = 0;

	std::atomic_int64_t Overhead = 0;

	FCriticalSection InternalMallocMutex;
	bool bInternalMalloc = false;
	bool bCapturingStack = false;
	bool bBackTrace = true;
	bool bMallocTrace = true;
	bool bStandAlone = false;
	bool bTrackVM = false;
};

static AllocTrackState& GetState()
{
	static AllocTrackState State;
	return State;
}

static bool IsInternalMalloc()
{
	FScopeLock Lock(&GetState().InternalMallocMutex);
	return GetState().bInternalMalloc;
}

constexpr int CacheSize = 32;
template<class T, bool bAutoDrop = true>
class MappedAlloc
{
public:
	MappedAlloc(const char* path)
	{
		PageSize = FPlatformMemory::GetConstants().PageSize * sizeof(T) * CacheSize;
#if PLATFORM_IOS || PLATFORM_ANDROID
		Handle = open(path, O_RDWR | O_CREAT, S_IRWXU);
		checkf(Handle != -1, TEXT("errno: %d, path: %s"), errno, ANSI_TO_TCHAR(path));
#elif PLATFORM_WINDOWS
		FileHandle = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		checkf(FileHandle != INVALID_HANDLE_VALUE, TEXT("errno: %d, path %s"), GetLastError(), ANSI_TO_TCHAR(path));

		LARGE_INTEGER LI;
		LI.QuadPart = (NumPages + 1) * PageSize;
		ensure(SetFilePointerEx(FileHandle, LI, NULL, FILE_BEGIN) != INVALID_SET_FILE_POINTER);
		ensure(SetEndOfFile(FileHandle) != 0);
#endif
	}

	~MappedAlloc()
	{

	}

	T* Malloc(uint32 n = 1)
	{
		FScopeLock Lock(&SharedMutex);

		if (Begin + n >= End)
		{
			FScopeLock InternalLock(&GetState().InternalMallocMutex);
			GetState().bInternalMalloc = true;
#if PLATFORM_IOS || PLATFORM_ANDROID

			ensure(ftruncate(Handle, (NumPages + 1) * PageSize) == 0);
			if (bAutoDrop && Buffer)
			{
				//				madvise(Buffer, PageSize, MADV_DONTNEED);
				munmap(Buffer, PageSize);
				GetState().Overhead -= PageSize;
			}
			T* Page = (T*)mmap(0, PageSize, PROT_READ | PROT_WRITE, MAP_SHARED, Handle, NumPages * PageSize);
#elif PLATFORM_WINDOWS

			if (Buffer)
			{
				if (!bAutoDrop)
				{
					FMemory::Memcpy(Buffer, ShadowBuffer, PageSize);
				}

				UnmapViewOfFile(Buffer);
				//GetState().Overhead -= PageSize;
				CloseHandle(Handle);


			}


			LARGE_INTEGER LI;
			LI.QuadPart = (NumPages + 1) * PageSize;
			ensure(SetFilePointerEx(FileHandle, LI, NULL, FILE_BEGIN) != INVALID_SET_FILE_POINTER);
			if (SetEndOfFile(FileHandle) != 0)
			{
				//checkf(false, TEXT("errno: %d"), GetLastError());
				//checkf(false, TEXT("errno: %d"), GetLastError());
			}
			Handle = CreateFileMappingA(FileHandle, NULL, PAGE_READWRITE, LI.HighPart, LI.LowPart, NULL);
			checkf(Handle != INVALID_HANDLE_VALUE && Handle != 0, TEXT("errno: %d"), GetLastError());

			LI.QuadPart = NumPages * PageSize;
			T* Page = (T*)MapViewOfFile(Handle, FILE_MAP_ALL_ACCESS, LI.HighPart, LI.LowPart, PageSize);
			checkf(Page, TEXT("errno: %d"), ::GetLastError());
			if (!bAutoDrop)
			{
				ShadowBuffer = (T*)malloc(PageSize);
				memset(ShadowBuffer, 0, PageSize);

			}
#else
			T* Page = 0;
			return nullptr;
#endif
			GetState().bInternalMalloc = false;

			check(Page);
			GetState().Overhead += PageSize;

			NumPages++;

			Buffer = Page;
#if PLATFORM_WINDOWS
			if (!bAutoDrop)
				Begin = ShadowBuffer;
			else
#endif
				Begin = Page;
			End = (T*)((uint8*)Begin + PageSize);

		}
		auto Cur = Begin;
		Begin += n;

		return Cur;
	}

private:
	T* Buffer = 0;
	T* Begin = 0;
	T* End = 0;
	uint64 PageSize = 0;
	uint32 NumPages = 0;
#if PLATFORM_WINDOWS
	HANDLE FileHandle = 0;
	HANDLE Handle = 0;
	T* ShadowBuffer = 0;
#elif PLATFORM_IOS || PLATFORM_ANDROID
	int Handle = 0;
#endif
	FCriticalSection SharedMutex;
};


struct Recorder
{
	FString Name;
	FArchive* File;
	TFunction<void(FArchive&)> Callback;
};




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


static void alloc_hooker(uint32_t type_flags, uintptr_t zone_ptr, uintptr_t arg2, uintptr_t arg3, uintptr_t return_val, uint32_t num_hot_to_skip)
{

#if PLATFORM_IOS


	uintptr_t size = 0;
	uintptr_t ptr_arg = 0;


	uint32_t alias = 0;
	VM_GET_FLAGS_ALIAS(type_flags, alias);
	// skip all VM allocation events from malloc_zone
//	if (alias >= VM_MEMORY_MALLOC && alias <= VM_MEMORY_MALLOC_NANO) {
//		return;
//	}
	
	if (!GetState().bTrackVM && alias != 0 && alias != VM_MEMORY_IOACCELERATOR && alias != VM_MEMORY_IOSURFACE)
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
		FAllocationTracking::TrackFree((const void*)ptr_arg, alias);
	}
	if ((type_flags & memory_logging_type_alloc) || (type_flags & memory_logging_type_vm_allocate)) {
		if (return_val == 0 || return_val == (uintptr_t)MAP_FAILED) {
			return;
		}
		size = arg2;

		FAllocationTracking::TrackAllocation((const void*)return_val, size,alias);
	}
#endif
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


struct TraceKey
{
	TraceKey(uint64* InTrace) :Trace(InTrace)
	{
	}

	bool operator < (const TraceKey& Other)const
	{

		auto B1 = Trace;
		auto B2 = Other.Trace;

		auto E1 = Trace + *Trace;
		auto E2 = Other.Trace + *Other.Trace;

		while (B1 != E1 && B2 != E2)
		{
			if (*(B1) == *(B2))
			{
				B1++;
				B2++;
				continue;
			}

			return *B1 < *B2;
		}

		return false;
	}

	int Size()const
	{
		return (int)Trace[0];
	}

	uint64& operator[](int i) const
	{
		return Trace[i + 1];
	}
	uint64* Trace;

};





template<typename T, typename Comp = std::less<T>>
class splay_tree {
public:
	Comp comp;
	unsigned long p_size = 0;

	struct node {
		node* left, * right;
		node* parent;
		T key;
		node(const T& init = T()) : left(nullptr), right(nullptr), parent(nullptr), key(init) { }
		~node() {

		}
	};

	node* end_ptr = nullptr;
	node* free_list = nullptr;
	void* Block = nullptr;

	node* root = 0;

	void alloc_space()
	{

		const FPlatformMemoryConstants& MemoryConstants = FPlatformMemory::GetConstants();
		auto alloc_size = MemoryConstants.PageSize * sizeof(node);

		FScopeLock Lock(&GetState().InternalMallocMutex);
		GetState().bInternalMalloc = true;
#if PLATFORM_IOS || PLATFORM_ANDROID
		Block = mmap(0, alloc_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
#elif PLATFORM_WINDOWS
		Block = ::malloc(alloc_size);
#endif
		GetState().bInternalMalloc = false;

		GetState().Overhead += alloc_size;
		check(Block);
		auto count = MemoryConstants.PageSize;
		auto begin = (node*)Block;

		*((node**)begin) = 0;
		for (SIZE_T i = 1; i < count; ++i)
		{
			*((node**)(begin + i)) = (begin + i - 1);
		}

		free_list = begin + count - 1;

	}

	node* create_node(T key) {
		node* n = nullptr;
		if (free_list != nullptr)
		{
			n = free_list;

			free_list = *(node**)n;

		}
		else
		{
			alloc_space();
			return create_node(std::move(key));
		}


		new(n) node(std::move(key));
		return n;
	}

	void destroy_node(node* n)
	{
		*(node**)n = free_list;
		free_list = n;
	}

	void left_rotate(node* x) {
		node* y = x->right;
		if (y) {
			x->right = y->left;
			if (y->left) y->left->parent = x;
			y->parent = x->parent;
		}

		if (!x->parent) root = y;
		else if (x == x->parent->left) x->parent->left = y;
		else x->parent->right = y;
		if (y) y->left = x;
		x->parent = y;
	}

	void right_rotate(node* x) {
		node* y = x->left;
		if (y) {
			x->left = y->right;
			if (y->right) y->right->parent = x;
			y->parent = x->parent;
		}
		if (!x->parent) root = y;
		else if (x == x->parent->left) x->parent->left = y;
		else x->parent->right = y;
		if (y) y->right = x;
		x->parent = y;
	}

	void splay(node* x) {
		while (x->parent) {
			if (!x->parent->parent) {
				if (x->parent->left == x) right_rotate(x->parent);
				else left_rotate(x->parent);
			}
			else if (x->parent->left == x && x->parent->parent->left == x->parent) {
				right_rotate(x->parent->parent);
				right_rotate(x->parent);
			}
			else if (x->parent->right == x && x->parent->parent->right == x->parent) {
				left_rotate(x->parent->parent);
				left_rotate(x->parent);
			}
			else if (x->parent->left == x && x->parent->parent->right == x->parent) {
				right_rotate(x->parent);
				left_rotate(x->parent);
			}
			else {
				left_rotate(x->parent);
				right_rotate(x->parent);
			}
		}
	}

	void replace(node* u, node* v) {
		if (!u->parent) root = v;
		else if (u == u->parent->left) u->parent->left = v;
		else u->parent->right = v;
		if (v) v->parent = u->parent;
	}

	node* subtree_minimum(node* u) {
		while (u->left) u = u->left;
		return u;
	}

	node* subtree_maximum(node* u) {
		while (u->right) u = u->right;
		return u;
	}
public:
	splay_tree() { alloc_space(); }

	node* insert(const T& key) {
		node* z = root;
		node* p = nullptr;

		while (z) {
			p = z;
			if (comp(z->key, key)) z = z->right;
			else z = z->left;
		}

		z = create_node(key);
		z->parent = p;

		if (!p) root = z;
		else if (comp(p->key, z->key)) p->right = z;
		else p->left = z;

		splay(z);
		p_size++;

		return z;
	}

	node* find(const T& key) {
		node* z = root;
		while (z) {
			if (comp(z->key, key)) z = z->right;
			else if (comp(key, z->key)) z = z->left;
			else return z;
		}
		return nullptr;
	}

	void erase(const T& key) {
		node* z = find(key);
		if (!z) return;

		splay(z);

		if (!z->left) replace(z, z->right);
		else if (!z->right) replace(z, z->left);
		else {
			node* y = subtree_minimum(z->right);
			if (y->parent != z) {
				replace(y, y->right);
				y->right = z->right;
				y->right->parent = y;
			}
			replace(z, y);
			y->left = z->left;
			y->left->parent = y;
		}

		destroy_node(z);
		p_size--;
	}

	const T& minimum() { return subtree_minimum(root)->key; }
	const T& maximum() { return subtree_maximum(root)->key; }

	bool empty() const { return root == nullptr; }
	unsigned long size() const { return p_size; }


	void visit(TFunction<void(node*)>&& visitor)
	{
		auto visit_func = [&](auto node, auto& func){
			if (!node)
				return;

			visitor(node);
			func(node->left, func);
			func(node->right, func);
		};

		visit_func(root, visit_func);
	}
};

template<class Key, class Value, class Cmp = std::less<Key> >
class splay_map
{
	using Type = std::pair<Key, Value >;
	struct PairCmp
	{
	public:
		bool operator()(const Type& a, const Type& b)const
		{
			static const Cmp cmp;
			return cmp(a.first, b.first);
		}
	};

	using Tree = splay_tree< Type, PairCmp>;
	using NodePtr = typename Tree::node*;
	Tree tree;
public:
	splay_map()//:tree(Cap)
	{

	};

	Value& operator[](const Key& key)
	{
		auto mkey = std::make_pair(key, Value{});
		auto n = tree.find(mkey);
		if (!n)
			n = tree.insert(mkey);
		return n->key.second;
	}

	NodePtr find(const Key& key)
	{
		auto mkey = std::make_pair(key, Value{});
		return tree.find(mkey);
	}

	NodePtr end()const
	{
		return 0;
	}

	void erase(const Key& key)
	{
		auto mkey = std::make_pair(key, Value{});
		tree.erase(mkey);
	}

	void erase(NodePtr n)
	{
		tree.erase(n->key);
	}

	Value& get(NodePtr n)
	{
		return n->key.second;
	}

	void visit(TFunction<void(const Key& , const Value&)>&& visitor)
	{
		tree.visit([&](auto node){
			visitor(node->key.first, node->key.second);
		});
	}
};

//static splay_map<const void*, AllocInfo*>* AddrMap;


struct AllocTrackEnvironment
{
	MappedAlloc<uint64, false>* MappedStacks = 0;
	MappedAlloc<AllocInfo>* Allocations = 0;
	MappedAlloc<DeallocInfo>* Deallocations = 0;

	splay_map<TraceKey, uint32> TraceMap;

	TSharedPtr<FArchive> FrameFile;
	TSharedPtr<FArchive> RHIFile;
	TSharedPtr<FArchive> SnapshotFile;
	TSharedPtr<FArchive> CustomFile;

	TArray<Recorder> Recorders;

	FCriticalSection TraceMutex;
	FCriticalSection RHIMutex;
	FCriticalSection CustomMutex;
	std::atomic_uint32_t FrameCount = 1;
};

static AllocTrackEnvironment& GetEnv()
{
	static AllocTrackEnvironment Env;
	return Env;
}



static bool IsInitialized()
{
	return GetState().bInitialized;
}

static std::atomic_uint32_t& GetCurrentTime()
{
	return GetEnv().FrameCount;
}



bool FAllocationTracking::IsEnabled()
{
	return IsInitialized();
}

bool FAllocationTracking::IsCapturingStack()
{
	return GetState().bCapturingStack;
}

bool FAllocationTracking::IsStandalone()
{
	return GetState().bStandAlone;
}

uint32 FAllocationTracking::GetCurrentFrame()
{
	return GetCurrentTime();
}
//  void FAllocationTracking::Uninitialize()
//  {
//  #if PLATFORM_IOS
//  	if (bCapturingStack)
//  		malloc_logger = nullptr;
//  #endif
//  	bInitialized = false;
//  }
static FString& GetFolderPath()
{
	return GetState().FolderPath;
}

FString GetFilePath(const TCHAR* Filename)
{
#if PLATFORM_IOS
	return IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*FPaths::Combine(GetFolderPath(), Filename));
#elif PLATFORM_ANDROID
	return IAndroidPlatformFile::GetPlatformPhysical().FileRootPath(*FPaths::Combine(GetFolderPath(), Filename));
#else
	return FPaths::Combine(GetFolderPath(), Filename);
#endif
}


#if PLATFORM_IOS

struct SegmentInfo
{
	mach_vm_address_t start;
	mach_vm_address_t end;
	std::string name;
	std::string image;
};

std::vector<SegmentInfo> Segments = []
{
    std::vector<SegmentInfo> segments;
    
    uint32_t imageCount = _dyld_image_count();
    for (uint32_t i = 0; i < imageCount; i++) {
        const char* imagePath = _dyld_get_image_name(i);
        std::string imageName = imagePath;
        size_t lastSlash = imageName.find_last_of('/');
        if (lastSlash != std::string::npos) {
            imageName = imageName.substr(lastSlash + 1);
        }
        
        const struct mach_header* header = (const struct mach_header*)_dyld_get_image_header(i);
        intptr_t slide = _dyld_get_image_vmaddr_slide(i);
        
        if (header->magic == MH_MAGIC_64) {
            const struct mach_header_64* header64 = (const struct mach_header_64*)header;
            const struct load_command* cmd = (const struct load_command*)(header64 + 1);
            
            for (uint32_t j = 0; j < header64->ncmds; j++) {
                if (cmd->cmd == LC_SEGMENT_64) {
                    const struct segment_command_64* seg = (const struct segment_command_64*)cmd;
                    mach_vm_address_t segStart = seg->vmaddr + slide;
                    mach_vm_address_t segEnd = segStart + seg->vmsize;
                    
                    segments.push_back( {segStart, segEnd, std::string(seg->segname), imageName});
                }
                cmd = (const struct load_command*)((char*)cmd + cmd->cmdsize);
            }
        }
    }

	std::sort(segments.begin(), segments.end(), [](auto& a, auto& b ){
		return a.start < b.start;
	});
    return segments;
}();

static std::map<uint32, std::string> RegionNameMapping = 
{
	{1, "MALLOC metadata"},
	{2, "MALLOC_SMALL"},
	{3, "MALLOC_LARGE"},
	{4, "MALLOC_HUGE"},
	{5, "SBRK"},
	{6, "REALLOC"},
	{7, "MALLOC_TINY"},
	{8, "MALLOC_LARGE_REUSABLE"},
	{9, "MALLOC_LARGE_REUSED"},

	{10, "Performance tool data"},

	{11, "MALLOC_NANO"},
	{12, "MALLOC_MEDIUM"},
	
	{21, "IOKit"},
	
	{30, "Stack"},
	{32, "shared memory"},
	{35, "unshared memory"},
	
	{41, "Foundation"},
	
	{51, "LayerKit"},
	{53, "WebKit Malloc"},
	
	{60, "dyld private memory"},
	{62, "SQLite page cache"},
	
	{73, "Kernel Alloc Once"},
	{78, "Activity Tracing(Genealogy)"},
	
	{87, "Skywalk Networking"},
	{88, "IOSurface"},
	{89, "libnetwork"},
	{90, "Audio"},
	
	{100, "IOAccelerator"},
	{104, "ColorSync"},
};

static std::map<std::string, uint32> GetIOSDirtySize(uint32& total_malloc, uint32& total_others)
{
	std::map<std::string,uint32> Dirties;
	
	struct RegionInfo
	{
		vm_address_t addr; 
		uint32 dirty;
		mach_vm_address_t imageStart;
		mach_vm_address_t imageEnd;
	};
	vm_address_t address = 0;
	vm_size_t size = 0;
	
	task_t task = mach_task_self();
	
	while (true) {
		vm_region_extended_info_data_t info;
		mach_msg_type_number_t infoCount = VM_REGION_EXTENDED_INFO_COUNT;
		mach_port_t objectName = MACH_PORT_NULL;
		
		kern_return_t kr = vm_region_64(
			task,
			&address,
			&size,
			VM_REGION_EXTENDED_INFO,
			(vm_region_info_t)&info,
			&infoCount,
			&objectName
		);
		
		if (kr != KERN_SUCCESS) {
			break; 
		}

		uint32 regionDirtySize = (info.pages_dirtied + info.pages_swapped_out) * vm_kernel_page_size;

		if(info.user_tag == 0)
		{
			total_others += regionDirtySize;

			auto pos = std::upper_bound(Segments.begin(), Segments.end(), address, [](auto& addr, auto& seg){
				return addr < seg.end;	
			});

			if (pos != Segments.end() && address >= pos->start)
			{
				Dirties[pos->name] += regionDirtySize;
			}
			else if (info.external_pager != 0)
			{
				Dirties["mapped file"] += regionDirtySize;
			}
			else
			{
				Dirties["VM_ALLOCATE"] += regionDirtySize;
			}
		}
		else
		{
			total_malloc += regionDirtySize;

			auto RegName = RegionNameMapping.find(info.user_tag);
			if (RegName == RegionNameMapping.end())
			{
				Dirties[std::string("Tag_") + std::to_string(info.user_tag)] += regionDirtySize;
			}
			else
			{
				Dirties[RegName->second] += regionDirtySize;
			}

		}


		address += size;
	}

	
	return Dirties;
}




#endif 

FORCENOINLINE static void GetProgramSize()
{
	auto Stats = FPlatformMemory::GetStats();
#if PLATFORM_IOS
	uint32 total_malloc = 0;
	uint32 total_others = 0;
	auto dirty_size = GetIOSDirtySize(total_malloc,total_others );
	uint64 Used = Stats.FootPrintMemory - total_others; // ignore memory which is not alloced from malloc
#else 
	uint64 Used = Stats.UsedPhysical;
#endif
	FAllocationTracking::TrackAllocation((void*)1, (uint32)Used, 0);
}

void FAllocationTracking::Initialize(const TCHAR* CmdLine, const TCHAR* InDir)
{

	if (IsInitialized())
		return;

	bool bEnabled = FParse::Param(CmdLine, TEXT("memoryprofile")) == true;

	UE_LOG(LogCore, Log, TEXT("allocation tracking is %s"), bEnabled ? TEXT("enabled") : TEXT("disabled"));

	if (!bEnabled)
		return;

#if PLATFORM_IOS
	orig_malloc_logger = malloc_logger;
	orig_syscall_logger = __syscall_logger;
	malloc_logger = nullptr;
	__syscall_logger = nullptr;
#endif


	FParse::Value(CmdLine, TEXT("ignoresmallalloc="), GetState().SizeLimit);
	FParse::Value(CmdLine, TEXT("tracktype="), GetState().TrackerType);

	GetState().bMallocTrace = FParse::Param(CmdLine, TEXT("nomalloc")) == false;
	GetState().bBackTrace = FParse::Param(CmdLine, TEXT("nostack")) == false;
	GetState().bCapturingStack = FParse::Param(CmdLine, TEXT("memalloclogger")) == true;
	GetState().bStandAlone = FParse::Param(CmdLine, TEXT("nollm")) == true;
	GetState().bTrackVM = FParse::Param(CmdLine, TEXT("trackvm")) == true;
	auto& FileMgr = IFileManager::Get();
	//    const FString Dir = FPaths::Combine(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()), TEXT("Profiling"));
	const FString Dir = InDir;

	//	auto DateTime = FDateTime::Now().ToString(TEXT("%Y_%m_%d-%H_%M_%S"));
	//    const auto FolderPath = FPaths::Combine(Dir, DateTime);
	auto& FolderPath = GetFolderPath();
	//	FolderPath= FPaths::Combine(Dir, DateTime);
	FolderPath = Dir;

	FileMgr.MakeDirectory(*FolderPath, true);


	auto& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();


	auto StackPath = GetFilePath( TEXT("stacks"));
	auto AllocPath = GetFilePath( TEXT("allocations"));
	auto DeallocPath = GetFilePath( TEXT("deallocations"));


	UE_LOG(LogCore, Log, TEXT("%s, %s"), *StackPath, *AllocPath);

	GetEnv().MappedStacks = new MappedAlloc<uint64, false>(TCHAR_TO_UTF8(*StackPath));
	GetEnv().Allocations = new MappedAlloc<AllocInfo>(TCHAR_TO_UTF8(*AllocPath));
	GetEnv().Deallocations = new MappedAlloc<DeallocInfo>(TCHAR_TO_UTF8(*DeallocPath));
	{
		TSharedPtr<FArchive> ModuleFile = MakeShareable<FArchive>(FileMgr.CreateFileWriter(*FPaths::Combine(FolderPath, TEXT("modules")), FILEWRITE_AllowRead));

#if PLATFORM_ANDROID
		Dl_info dlinfo;
		int Count = 0;
		if (dladdr((const void*)&FAllocationTracking::Initialize, &dlinfo) != 0)
		{
			Count = 1;
			(*ModuleFile) << Count;
			{
				uint64 BaseAddr = (uint64)dlinfo.dli_fbase;
				uint32 ImgeSize = 0xffffffff;//dlinfo.dli_size;
				(*ModuleFile) << BaseAddr;
				(*ModuleFile) << ImgeSize;
				int StrLen = FCStringAnsi::Strlen(dlinfo.dli_fname);
				(*ModuleFile) << StrLen;
				ModuleFile->Serialize((void*)dlinfo.dli_fname, StrLen);
			}
		}
		else
		{
			(*ModuleFile) << Count;
		}
#else
		auto Count = FPlatformStackWalk::GetProcessModuleCount();
		TArray< FStackWalkModuleInfo> ModuleInfos;
		ModuleInfos.SetNum(Count);
		Count = FPlatformStackWalk::GetProcessModuleSignatures(ModuleInfos.GetData(), Count);
		if (Count == 0)
		{
			UE_LOG(LogCore, Warning, TEXT("no modules found."));
		}
		(*ModuleFile) << Count;
		for (int i = 0; i < Count; ++i)
		{
			(*ModuleFile) << ModuleInfos[i].BaseOfImage;
			(*ModuleFile) << ModuleInfos[i].ImageSize;
			int StrLen = FCStringWide::Strlen(ModuleInfos[i].ImageName);
			(*ModuleFile) << StrLen;
			ModuleFile->Serialize(TCHAR_TO_ANSI(ModuleInfos[i].ImageName), StrLen);
		}
#endif
	}

	// {
	// 	TSharedPtr<FArchive> InfoFile = MakeShareable<FArchive>(FileMgr.CreateFileWriter(*FPaths::Combine(FolderPath, TEXT("infos")), FILEWRITE_AllowRead));

	// 	auto WriteInfo = [&](FString Key, FString Value){
	// 		FString Str = FString::Printf(TEXT("%s=%s"), *Key, *Value);
	// 		int Len = Str.Len();
	// 		(*InfoFile) << Len;
	// 		InfoFile->Serialize(TCHAR_TO_ANSI(*Str), Len);
	// 	};
	// 	WriteInfo(TEXT("cmdline"), CmdLine);
	// }

	GetEnv().SnapshotFile = MakeShareable<FArchive>(FileMgr.CreateFileWriter(*FPaths::Combine(FolderPath, TEXT("snapshots")), FILEWRITE_AllowRead));
	GetEnv().FrameFile = MakeShareable<FArchive>(FileMgr.CreateFileWriter(*FPaths::Combine(FolderPath, TEXT("frames")), FILEWRITE_AllowRead));
	GetEnv().RHIFile = MakeShareable<FArchive>(FileMgr.CreateFileWriter(*FPaths::Combine(FolderPath, TEXT("rhis")), FILEWRITE_AllowRead));
	GetEnv().CustomFile = MakeShareable<FArchive>(FileMgr.CreateFileWriter(*FPaths::Combine(FolderPath, TEXT("customs")), FILEWRITE_AllowRead));

	//	ObjectFile = MakeShareable<FArchive>(FileMgr.CreateFileWriter(*FPaths::Combine(FolderPath, TEXT("objects")), FILEWRITE_AllowRead));

	for (auto& R : GetEnv().Recorders)
	{
		R.File = (FileMgr.CreateFileWriter(*FPaths::Combine(FolderPath, *R.Name), FILEWRITE_AllowRead));
	}


#if PLATFORM_IOS
	if (IsCapturingStack() && GetState().bMallocTrace)
	{
		malloc_logger = malloc_hooker;
		__syscall_logger = syscall_hooker;
	}
	else
	{
		malloc_logger = orig_malloc_logger;
		__syscall_logger = orig_syscall_logger;
	}
#endif

	GetState().bInitialized = true;
	GetProgramSize();
};


int FAllocationTracking::GetTrackerType()
{
	return GetState().TrackerType;
}

#if PLATFORM_WINDOWS
static int FastCaptureBackTraceOnWin64(uint64* BackTrace, uint32 MaxDepth)
{
	CONTEXT ContextRecord;
	RtlCaptureContext(&ContextRecord);

	UINT iFrame;
	UINT nFrames = MaxDepth;
	for (iFrame = 0; iFrame < nFrames; iFrame++)
	{
		DWORD64 ImageBase;
		PRUNTIME_FUNCTION pFunctionEntry = RtlLookupFunctionEntry(ContextRecord.Rip, &ImageBase, NULL);

		if (pFunctionEntry == NULL)
			break;

		PVOID HandlerData;
		DWORD64 EstablisherFrame;
		RtlVirtualUnwind(UNW_FLAG_NHANDLER,
			ImageBase,
			ContextRecord.Rip,
			pFunctionEntry,
			&ContextRecord,
			&HandlerData,
			&EstablisherFrame,
			NULL);

		BackTrace[iFrame] = ContextRecord.Rip;
	}
	return iFrame;
}
#endif

int FAllocationTracking::GetCurrentStackId()
{
	if (!GetState().bBackTrace)
		return -1;
	static const int MAX_DEPTH = 1024;
	uint64 Stack[MAX_DEPTH + 1];

#if PLATFORM_WINDOWS 
	Stack[0] = FastCaptureBackTraceOnWin64(Stack + 1, MAX_DEPTH);
#else
	Stack[0] = FPlatformStackWalk::CaptureStackBackTrace(Stack + 1, MAX_DEPTH);
#endif
	
	constexpr int skip_num = 2;
	
	if (Stack[0] <= skip_num)
		return -1;
	
	Stack[0] -= skip_num;
	FMemory::Memmove(Stack + 1, Stack + 1 + skip_num, Stack[0] * sizeof(uint64));
	
	TraceKey Key(Stack);

	auto& Env = GetEnv();

	int StackId = 0;

	{
		FScopeLock Lock(&Env.TraceMutex);
		auto TraceRet = Env.TraceMap.find(Key);

		static uint32 StackIndex = 0;
		if (TraceRet == Env.TraceMap.end())
		{
			auto TraceCount = Stack[0] + 1;
			auto MappedTrace = Env.MappedStacks->Malloc((uint32)TraceCount);
			//            auto Trace = Stacks->Malloc(TraceCount);
			FMemory::Memcpy(MappedTrace, Stack, TraceCount * sizeof(uint64));
			//            FMemory::Memcpy(Trace, Stack, TraceCount * sizeof(uint64));

			Key.Trace = MappedTrace;
			StackId = StackIndex++;
			Env.TraceMap[Key] = StackId;
		}
		else
		{
			StackId = Env.TraceMap.get(TraceRet);
		}
	}
	
	return StackId;
}

void FAllocationTracking::TrackAllocation(const void* Ptr, uint32 Size, uint32 VMUserTag)
{
	if (Size <= GetState().SizeLimit)
	{
		return;
	}

	if (IsInternalMalloc())
		return;

	int StackId = GetCurrentStackId();
	
	if (StackId < 0)
		return;

#if PLATFORM_IOS	
	auto RealSize = malloc_size(Ptr);
	if (RealSize > Size)
		Size = RealSize;
#endif

	auto Info = GetEnv().Allocations->Malloc();
	Info->Ptr = (uint64)Ptr;
	Info->Size = Size;
	Info->Trace = StackId;
	Info->Begin = GetCurrentTime();
	Info->Tag = VMUserTag;

}

void FAllocationTracking::TrackFree(const void* Ptr, uint32 VMUserTag)
{
	if (IsInternalMalloc())
		return;

	auto Info = GetEnv().Deallocations->Malloc();
	Info->Ptr = (uint64)Ptr;
	Info->End = GetCurrentTime();
	Info->Tag = VMUserTag;
}

FCriticalSection& GetRHIMutex()
{

	return GetEnv().RHIMutex;
}


void FAllocationTracking::TrackAllocRHITexture(const void* Ptr, const FString& Name, uint32 Size, uint32 Width, uint32 Height, uint32 Depth, uint32 Format, uint32 NumMips, uint32 SampleCount, uint32 ArrayCount, uint32 Type, uint32 bRT)
{
	if (!IsInitialized())
		return;

	FScopeLock Lock(&GetRHIMutex());
	auto& RHIFile = GetEnv().RHIFile;


	uint32 bCreate = 1;
	(*RHIFile) << bCreate;
	uint64 Addr = (uint64)Ptr;
	(*RHIFile) << Addr;
	uint32 Time = GetCurrentTime();
	(*RHIFile) << Time;
	int StrLen = Name.Len();
	(*RHIFile) << StrLen;
	RHIFile->Serialize(TCHAR_TO_ANSI(*Name), StrLen);

	(*RHIFile) << Type;

	(*RHIFile) << Size;

	uint32 SizeX = 1;
	uint32 SizeY = 1;
	uint32 SizeZ = 1;
	switch (Type)
	{

	case TextureArray:
		SizeZ = ArrayCount;
	case Texture:
		SizeX = Width;
		SizeY = Height;
		break;
	case Texture3D:
		SizeX = Width; SizeY = Height; SizeZ = Depth;
		break;
	case TextureCubeArray:
		SizeY = ArrayCount;
	case TextureCube:
		SizeX = Width;
	}

	(*RHIFile) << SizeX;
	(*RHIFile) << SizeY;
	(*RHIFile) << SizeZ;
	(*RHIFile) << Format;
	(*RHIFile) << NumMips;
	(*RHIFile) << bRT;
}

//static const void* P = 0;

void FAllocationTracking::TrackFreeRHITexture(const void* Ptr)
{
	if (!IsInitialized())
		return;
	FScopeLock Lock(&GetRHIMutex());

	auto& RHIFile = GetEnv().RHIFile;
	//	check (P != Ptr);


	uint32 bCreate = 0;
	(*RHIFile) << bCreate;
	uint64 Addr = (uint64)Ptr;
	(*RHIFile) << Addr;
	uint32 Time = GetCurrentTime();
	(*RHIFile) << Time;
}



void FAllocationTracking::TrackAllocRHIBuffer(const void* Ptr, const FString& Name, uint32 Size, uint32 Type)
{
	if (!IsInitialized())
		return;
	FScopeLock Lock(&GetRHIMutex());

	auto& RHIFile = GetEnv().RHIFile;

	uint32 bCreate = 1;
	(*RHIFile) << bCreate;
	uint64 Addr = (uint64)Ptr;
	(*RHIFile) << Addr;
	uint32 Time = GetCurrentTime();
	(*RHIFile) << Time;
	int StrLen = Name.Len();
	(*RHIFile) << StrLen;
	RHIFile->Serialize(TCHAR_TO_ANSI(*Name), StrLen);
	(*RHIFile) << Type;

	(*RHIFile) << Size;
}

void FAllocationTracking::TrackFreeRHIBuffer(const void* Ptr)
{
	TrackFreeRHITexture(Ptr);
}

void FAllocationTracking::TrackAllocObject(const void* Ptr, const FString& Name, uint32 Size, uint32 Type)
{
	auto& Custom = *GetEnv().CustomFile.Get();
	FScopeLock Lock(&GetEnv().CustomMutex);

	uint32 bCreate = 1;
	Custom << bCreate;
	uint64 Addr = (uint64)Ptr;
	Custom << Addr;
	uint32 Time = GetCurrentTime();
	Custom << Time;

	SerializeStr(Custom, Name);
	Custom << Type; 
	Custom << Size;
	int BufferSize = 0;
	Custom << BufferSize;
}


void FAllocationTracking::TrackAllocObjectWithData(const void* Ptr, const FString& Name, uint32 Size, uint32 Type, const void* Buffer, uint32 BufferSize)
{
	auto& Custom = *GetEnv().CustomFile.Get();
	FScopeLock Lock(&GetEnv().CustomMutex);

	uint32 bCreate = 1;
	Custom << bCreate;
	uint64 Addr = (uint64)Ptr;
	Custom << Addr;
	uint32 Time = GetCurrentTime();
	Custom << Time;

	SerializeStr(Custom, Name);
	Custom << Type;
	Custom << Size;
	Custom << BufferSize;
	if (BufferSize > 0)
		Custom.Serialize((void*)Buffer, BufferSize);
	
}
void FAllocationTracking::TrackFreeObject(const void* Ptr)
{
	auto& Custom = *GetEnv().CustomFile.Get();
	FScopeLock Lock(&GetEnv().CustomMutex);
	uint32 bCreate = 0;
	Custom << bCreate;
	uint64 Addr = (uint64)Ptr;
	Custom << Addr;
	uint32 Time = GetCurrentTime();
	Custom << Time;
}

void FAllocationTracking::AddFrameRecorder(const FString& name, TFunction<void(FArchive& Ar)> Recorder)
{
	if (GetFolderPath().IsEmpty())
	{
		GetEnv().Recorders.Add({ name, {}, MoveTemp(Recorder) });
	}
	else
	{
		auto& FileMgr = IFileManager::Get();
		auto File = (FileMgr.CreateFileWriter(*FPaths::Combine(GetFolderPath(), *name), FILEWRITE_AllowRead));
		GetEnv().Recorders.Add({ name, File, MoveTemp(Recorder) });
	}
}

void FAllocationTracking::BeginFrame()
{
	if (!IsInitialized())
		return;

	for (auto& R : GetEnv().Recorders)
	{
		R.Callback(*R.File);
	}

	auto& FrameFile = GetEnv().FrameFile;

	auto Stats = FPlatformMemory::GetStats();
#if PLATFORM_IOS
	uint64 Used = Stats.FootPrintMemory;
#else 
	uint64 Used = Stats.UsedPhysical;
#endif
	//		Used -= (uint64)FMath::Max(0ll,(int64) Overhead);
	uint64 Available = Stats.AvailablePhysical;
	int FrameId = GetCurrentTime();
	(*FrameFile) << FrameId;
	(*FrameFile) << Used;
	(*FrameFile) << Available;
	int64 OH = GetState().Overhead;
	(*FrameFile) << OH;


	TArray<uint8> Data;
#if PLATFORM_IOS

	{
		uint32 total_malloc = 0;
		uint32 total_others = 0;
		auto Dirties = GetIOSDirtySize(total_malloc,total_others );
	
		
		FMemoryWriter Writer(Data);
		
		int Platform = 2; // 1: windows 2: ios 3:android
		Writer << Platform;
		uint32 Count = Dirties.size();
		Writer << Count;

		for (auto& info : Dirties)
		{
			uint32 len = info.first.length();
			uint32 size = info.second;

			Writer << len;
			Writer.Serialize((uint8*)info.first.c_str(), len);
			Writer << size;
		}
	}

#endif

	uint32 Count = Data.Num();
	(*FrameFile) << Count;
	FrameFile->Serialize(Data.GetData(), Count);


	GetCurrentTime() += 1;

	FrameFile->Flush();
	{
		FScopeLock Lock(&GetRHIMutex());
		GetEnv().RHIFile->Flush();
	}
}

static void TakeSnapshot(const FString& Name)
{
	if (!IsInitialized())
		return;
	uint32 Time = GetCurrentTime();
	(*GetEnv().SnapshotFile) << Time;
	std::string NameStr = TCHAR_TO_UTF8(*Name);
	int Len = NameStr.length();
	(*GetEnv().SnapshotFile) << Len;
	GetEnv().SnapshotFile->Serialize((void*)NameStr.c_str(), Len);

	GetEnv().SnapshotFile->Flush();
}

static FAutoConsoleCommand AllocationTrackingTakeSnpashot(TEXT("alloc.take"), TEXT(""), FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args) 
{
	{
		TakeSnapshot(Args.Num() > 0 ? Args[0] : LexToString(GetCurrentTime().load()));
	}
}));

#include <sstream>
#include <iomanip>
static FAutoConsoleCommand AllocationTrackingGenerateSymbols(TEXT("alloc.gensymbols"), TEXT(""), FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
{

	auto& Env = GetEnv();
	FScopeLock Lock(&Env.TraceMutex);
	FScopeLock Lock2(&GetState().InternalMallocMutex);
	GetState().bInternalMalloc = true;

	TSortedMap<int64, std::string> AddrMap;

	Env.TraceMap.visit([&](const TraceKey& trace, auto){
		for (int i = 0; i < trace.Size(); ++i)
		{
			AddrMap.Add(trace[i], {});
		}
	});

	auto File = IFileManager::Get().CreateFileWriter(*FPaths::Combine(GetFolderPath(), TEXT("symbols")), FILEWRITE_AllowRead);


	std::stringstream content;

	std::string LastName;
	for (auto& Item : AddrMap)
	{
		FProgramCounterSymbolInfo SymbolInfo;
		FPlatformStackWalk::ProgramCounterToSymbolInfo(Item.Key, SymbolInfo);
		Item.Value = SymbolInfo.FunctionName;
		if (Item.Value.empty())
		{
			Item.Value = "unknown";
		}

		if (LastName != Item.Value)
		{
			LastName = Item.Value;
			content << Item.Key << " " << Item.Value << "\n";
		}
	}



	auto Str = content.str();

	File->Serialize((void*)Str.c_str(), Str.length());

	File->Flush();
	delete File;




	GetState().bInternalMalloc = false;

}));

