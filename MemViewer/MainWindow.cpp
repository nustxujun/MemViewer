#include "MemViewer.h"
#include "TraceParser.h"
#include "imgui.h"
#include "imgui/backends/imgui_impl_win32.h"
#include "Concurrency.h"
#include "FrequenceView.h"
#include "CategoryView.h"
#include "CallstackView.h"
#include "BlockView.h"
#include "ObjectView.h"
#include "RHIView.h"
#include "Snapshot.h"
#include "Utils.h"
#include "FrameParser.h"
#include "CustomObj.h"

#include "TraceView.h"

#include <windows.h>
#include <tchar.h>
#include <iostream>
#include <format>
#include <algorithm>
#include <filesystem>

static int hide_ui = 0;
static int selected_mode = 1;
//FrequenceView FreqView("FreqView");
//CategoryView CateView("CategoryView");
//CallstackView TreeView("CallstackView");
//BlockView BlkView("BlockView");
//ObjectView ObjView("ObjectView");
//RHIView RhiView("RhiResourceView");
//SnapshotView SnView("SnapshotView");
//CustomObj CustomView("CustomObjView");



static float scaling = 1.0f;
void InitializeMainWindow(void* hwnd)
{
    ImGuiIO& io = ImGui::GetIO();

    scaling = ImGui_ImplWin32_GetDpiScaleForHwnd(hwnd);
    if (std::filesystem::exists("c:\\Windows\\Fonts\\CascadiaCode.ttf"))
    {
        io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\CascadiaCode.ttf", 13.0f * scaling,0, io.Fonts->GetGlyphRangesChineseFull());
    }
    else
    {
        io.Fonts->AddFontDefault();
    }

    if (std::filesystem::exists("c:\\Windows\\Fonts\\simhei.ttf"))
    {
        ImFontConfig cfg;
        cfg.MergeMode = true;
        io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\simhei.ttf", 13.0f * scaling, &cfg, io.Fonts->GetGlyphRangesChineseFull());
    }
    io.Fonts->Build();
}

std::string openFileDialog() {

    char cur_dir[MAX_PATH];
    ::GetCurrentDirectoryA(MAX_PATH, cur_dir);

    OPENFILENAMEA open;
    ZeroMemory(&open, sizeof(open));

    char path[MAX_PATH] = {};

    ZeroMemory(&open, sizeof(open));
    open.lStructSize = sizeof(open);
    open.hwndOwner = NULL;
    open.lpstrFile = path;
    open.nMaxFile = sizeof(path);
    open.lpstrFilter = "Trace File\0*.memtrace;*.zip\0";
    open.nFilterIndex = 1;
    open.lpstrFileTitle = NULL;
    open.nMaxFileTitle = 0;
    open.lpstrInitialDir = NULL;
    open.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    auto selected = GetOpenFileNameA(&open);
    auto ret = GetLastError();

    ::SetCurrentDirectoryA(cur_dir);

    return path;
}

extern int GetMode()
{
    return selected_mode;
}

void ShowMainWindow() 
{
    ProcessTasks();


    static char filter[1024];
    static std::string file_history;

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize( viewport->WorkSize );

    bool open = true; 
    ImGui::Begin("Main window",&open,flags);

    std::string file_path;
    ModalWindow::ProcessModalWindow();

    static TraceView View;
    View.Show();



    ImGui::End();
}

void RefreshFrameData()
{
    //typedef void(*Func)();
    //Func updates[] = { UpdateTreeView   };
    //ParallelTask([&](int idx){
    //    updates[idx]();
    //},sizeof(updates) / sizeof(Func));

    //TreeView.Update();
    //CateView.Update();
    ////FreqView.Update();
    //BlkView.Update();
    //ObjView.Update();
    //RhiView.Update();CustomView.Update();
    //SnView.Update();
}

