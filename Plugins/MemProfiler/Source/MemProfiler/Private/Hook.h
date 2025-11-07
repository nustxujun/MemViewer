#pragma once

typedef void (*MallocEvent)(const void* Ptr, uint32 Size, uint32 UserData);
typedef void (*FreeEvent)(const void* Ptr, uint32 UserData);


