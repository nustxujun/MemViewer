#if PLATFORM_IOS
#include "IOSMemoryInfo.h"
#include <mach/mach.h>
#include <mach/vm_map.h>


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


std::map<std::string, int64> FIOSMemoryInfo::GetCustomMemoryInfo()
{
	std::map<std::string,int64> Dirties;
	
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

		uint32 regionDirtySize = (info.pages_resident) * vm_kernel_page_size;

		if(info.user_tag == 0)
		{

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
