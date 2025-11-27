#pragma once


#include <string>
#include <vector>
#include <functional>
#include <format>
// if the allocation has been malloced and freed in same time, it's a transient alloc.
#define INCLUDE_TRANSIENT_ALLOC 0

#if INCLUDE_TRANSIENT_ALLOC
#define ALLOC_END_CMP(INC, EXINC) INC
#else
#define ALLOC_END_CMP(INC, EXINC) EXINC


#endif


extern void InitializeMainWindow(void* hwnd);
extern void ShowMainWindow();
extern int GetMode();








//extern void InitializeFrequence();
//extern void UpdateFrequence();
//extern void ShowFrequence();
