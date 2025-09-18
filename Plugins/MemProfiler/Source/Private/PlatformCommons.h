#pragma once 

#if PLATFORM_WINDOWS
#include "Platforms/Windows/WinHook.h"
#include "Platforms/Windows/WindowsInternalAllocator.h"

#elif PLATFORM_IOS
#include "Platforms/iOS/iOSHook.h"
#include "Platforms/IOS/IOSInternalAllocator.h"


#endif