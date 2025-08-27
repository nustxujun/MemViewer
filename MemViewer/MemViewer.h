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

extern void InitializeTimeline();
extern void ShowTimeline();
extern void SetCustomData(std::vector<float> datas);
extern void SetCustomData2(std::vector<float> datas);

//extern void UpdateTreeView();
//extern void ShowTreeView();
//extern void FilterByString(const char* filters, bool ignore_case);
//
//
//extern void ShowFilter();
//
//extern void UpdateCategory();
//extern void InitializeCategory();
//extern void ShowCategory();


extern bool ShowFileBrowser(std::string& path);








//extern void InitializeFrequence();
//extern void UpdateFrequence();
//extern void ShowFrequence();
