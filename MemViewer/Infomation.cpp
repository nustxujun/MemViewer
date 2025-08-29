//#include "MemViewer.h"
//#include "imgui.h"
//#include "Utils.h"
//
//static std::string info_str = "asdasd";
//static ImVec4 color;
//void Info(const std::string& info)
//{
//    color = {1,1,1,1};
//    info_str = info;
//}
//void Error(const std::string& info)
//{
//    color = { 1,0,0,1 };
//    info_str = info;
//}
//void Warning(const std::string& info)
//{
//    color = { 1,1,0,1 };
//    info_str = info;
//
//}
//
//void ShowInfomation()
//{
//    auto windowWidth = ImGui::GetWindowSize().x;
//    auto textWidth = ImGui::CalcTextSize(info_str.c_str()).x;
//
//    ImGui::SetCursorPosX((windowWidth - textWidth) * 0.5f);
//    ImGui::TextColored(color, GBKToUTF8(info_str).c_str());
//}