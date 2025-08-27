#include "MemViewer.h"
#include "imgui.h"
#include "TraceParser.h"
#include "FrameParser.h"
#include "TraceInstance.h"
#include "Timeline.h"
#include "Concurrency.h"

#include <vector>
#include <format>



void TimelineView::InitializeImpl()
{
    frames.clear();
    custom_datas.clear();

    auto trace_basic = GetTrace();
    auto trace = (TraceInstance*) trace_basic.get();
    
    
    
    dragging_start = 0;
    dragging_end = 0;


    std::vector<uint64_t> size_vec;
    auto range = trace->getTraceRange();
    auto count = range.count() + 1;
    base_frame = range.begin;
    size_vec.resize(count);


    ParallelTask([&](int idx){
        if (idx == 0)
        {
            for (auto& alloc : trace->getTotalAllocs())
            {
                auto end = std::min<int>(alloc.end, range.end);

                Assert(alloc.size < 1024 * 1024 * 1024, "invalid trace data");
                EXIT_IF_STOPED()

                    for (int i = alloc.start; i ALLOC_END_CMP(<= , < ) end; ++i)
                    {
                        size_vec[i - range.begin] += alloc.size;
                    }
            }
        }
        else if (idx == 1)
        {

        }
    }, 2);



    float inv = 1.0f / 1024.0f / 1024.0f ;
    frames.reserve(count);
    max_size = 0;
    for (auto size : size_vec)
    {
        auto total_size = (size ) * inv;
        //max_size = std::max(max_size, total_size);
        frames.push_back(total_size );
    }

    uint64_t total_max = 0;
    if (trace->getMemoryInfos().size() > 0)
    {
        total_max = trace->getMemoryInfos()[0].used + trace->getMemoryInfos()[0].available;
        total_scaling = 1.0f / total_max;
    }

    for (auto& size : trace->getMemoryInfos())
    {
        max_size = std::max(max_size, size.used * inv);
    }

    if (trace->getTotalAllocs().size() == 0)
    {
        frames.clear();
        for (auto& size : trace->getMemoryInfos())
        {
            frames.push_back(size.used * inv);
        }
    }

    scrolling = {0,0};
}

void TimelineView::ShowImpl()
{



    auto trace_basic = GetTrace();
    auto trace = std::dynamic_pointer_cast<TraceInstance>(trace_basic);
    auto range = trace->getTraceRange();



    const float scrollbar_height = 15;
    const float checkpoint_height = 20;

    const auto& chkpnts = trace->getCheckpoints();

    ImGui::BeginChild("Tag", {0, 0}, ImGuiChildFlags_AutoResizeY);
    auto cp_count = chkpnts.size();
    auto cp_idx = 0;
    for (auto& cp : chkpnts)
    {
        cp_idx++;
        if (ImGui::Button(cp.text.c_str()))
        {
            select_range_cb(1, cp.frameid);
            dragging_start = 0;
            dragging_end = cp.frameid - base_frame;
        }
        if (cp_idx < cp_count)
        {
            ImGui::SameLine();
        }
    }
    ImGui::EndChild();

    ImGui::BeginChild("Canvas");

    ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();      // ImDrawList API uses screen coordinates!
    ImVec2 canvas_sz = ImGui::GetContentRegionAvail();   // Resize canvas to what's available
    if (canvas_sz.x < 50.0f) canvas_sz.x = 50.0f;
    if (canvas_sz.y < 50.0f) canvas_sz.y = 50.0f;
    canvas_sz.y -= scrollbar_height + checkpoint_height;
    ImVec2 canvas_p1 = ImVec2(canvas_p0.x + canvas_sz.x, canvas_p0.y + canvas_sz.y);

    // Draw border and background color
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRectFilled(canvas_p0, canvas_p1, IM_COL32(50, 50, 50, 255));
    draw_list->AddRect(canvas_p0, canvas_p1, IM_COL32(255, 255, 255, 255));

    // This will catch our interactions
    ImGui::InvisibleButton("canvas", canvas_sz, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
    const bool is_active = ImGui::IsItemActive();   // Held
    const bool is_focus = ImGui::IsItemFocused();
    const ImVec2 origin(canvas_p0.x + scrolling.x, canvas_p0.y + scrolling.y); // Lock scrolled origin
    const ImVec2 mouse_pos_in_canvas(io.MousePos.x - canvas_p0.x, io.MousePos.y - canvas_p0.y);
    const bool is_in_canvas = io.MousePos.x >= canvas_p0.x && io.MousePos.x <= canvas_p1.x && io.MousePos.y <= canvas_p1.y && io.MousePos.y > canvas_p0.y;

    // Pan (we use a zero mouse threshold when there's no context menu)
    // You may decide to make that threshold dynamic based on whether the mouse is hovering something etc.
    const float mouse_threshold_for_pan = opt_enable_context_menu ? -1.0f : 0.0f;
    if (is_active && ImGui::IsMouseDragging(ImGuiMouseButton_Right, mouse_threshold_for_pan))
    {
        scrolling.x += io.MouseDelta.x;
        scrolling.y += io.MouseDelta.y;
    }

    if (is_in_canvas && io.MouseWheel != 0)
    {
        auto len = (mouse_pos_in_canvas.x - scrolling.x) * freq;

        int diff = (int)std::max(1.0f, freq * 0.1f);
        if (io.MouseWheel > 0)
        {
            
            //freq = std::min(8, freq + 1);
            freq += diff;
        }
        else 
        {
            freq = std::max(1, freq - diff);
        }

        scrolling.x = mouse_pos_in_canvas.x - len / freq;
    }

    draw_list->PushClipRect(canvas_p0, canvas_p1, true);

    const float grid_width = 8;
    const float frame_width = grid_width / freq;
    const float local_begin_x = scrolling.x + canvas_p0.x;
    const float begin_x = local_begin_x - fmodf(local_begin_x, frame_width);
    int index = 0;
    for (auto x = begin_x; x < canvas_p1.x ; x += grid_width, index++)
    {
        if (x < 0)
            continue;

        const auto draw_x =  x;

        if (index % 5 == 0)
        {
            if (index % 10 == 0)
            {
                draw_list->AddLine(ImVec2(draw_x, canvas_p0.y), ImVec2(draw_x, canvas_p0.y + 10), IM_COL32(255, 255, 255, 255), 2.0f);
                auto num = std::format("{0}", index * freq);
                auto str_len = ImGui::CalcTextSize(num.c_str(), num.c_str() + num.length());
                draw_list->AddText(ImVec2(draw_x - str_len.x * 0.5f, canvas_p0.y + 12), IM_COL32_WHITE, num.c_str());
            }
            else
                draw_list->AddLine(ImVec2(draw_x, canvas_p0.y), ImVec2(draw_x, canvas_p0.y + 7), IM_COL32(255, 255, 255, 100), 1.0f);


 
        }
        else
        {
            draw_list->AddLine(ImVec2(draw_x, canvas_p0.y), ImVec2(draw_x, canvas_p0.y + 4), IM_COL32(255, 255, 255, 100), 1.0f);
        }


    }

    const auto frame_scaling = (canvas_sz.y *0.8f) / max_size;
    const auto std_step = frame_scaling * 512;


    uint32_t overlay_frame = 0;

    const auto real_frame_width = std::max(1.0f , frame_width <= 3? frame_width: frame_width - 1);

    static std::vector<ImVec2> line_points ;
    static std::vector<ImVec2> used_points;
    line_points.clear();
    used_points.clear();

    int text_frame = -1;

    int cur_checkpoint = 0;
    for (auto frame = 0; frame < trace->getMemoryInfos().size(); ++frame)
    {
        auto x = begin_x + frame * frame_width;
        if (x < canvas_p0.x )
            continue;

        if (x > canvas_p1.x)
            break;
            
        bool draw_info = false;
        ImU32 col = IM_COL32(255, 200, 0, 200);
        if (io.MousePos.x >= x && io.MousePos.x < x + frame_width && io.MousePos.y < canvas_p1.y && io.MousePos.y> canvas_p0.y)
        {
            overlay_frame = frame;
            col = IM_COL32(255, 255, 0, 255);

            draw_info = true;

            text_frame = frame;
        }

        if (cur_checkpoint < chkpnts.size())
        {
            auto cur_frameid = frame + range.begin;
            while (chkpnts[cur_checkpoint].frameid < cur_frameid && cur_checkpoint < chkpnts.size())
            {
                cur_checkpoint++;
            }

            auto& cp = chkpnts[cur_checkpoint];

            if (cp.frameid == cur_frameid)
            {
                col = IM_COL32(0,255,255, 200);
            }
        }


        if (frame < frames.size())
        {
            auto rect_min_y =  canvas_p1.y - frames[frame] * frame_scaling;

            if (frame < trace->getMemoryInfos().size() && trace->getMemoryInfos()[frame].overhead != 0)
                draw_list->AddRectFilled(ImVec2(x, rect_min_y - trace->getMemoryInfos()[frame].overhead / 1024.0 / 1024.0 * frame_scaling), ImVec2(x + real_frame_width, rect_min_y), IM_COL32(255,160,0,200));

            draw_list->AddRectFilled(ImVec2(x, rect_min_y),ImVec2(x + real_frame_width, canvas_p1.y), col);
        }
        if (frame < custom_datas.size())
            draw_list->AddRectFilled(ImVec2(x, canvas_p1.y - custom_datas[frame] * frame_scaling), ImVec2(x + real_frame_width, canvas_p1.y), IM_COL32(100,100,255,255));

        if (frame < (custom_datas2.size()) )
            line_points.push_back({ x + real_frame_width * 0.5f,canvas_p1.y - custom_datas2[frame] * canvas_sz.y });
            //draw_list->AddLine({x + real_frame_width,canvas_p1.y - custom_datas2[frame] * canvas_sz.y }, {begin_x + (frame + 1) * frame_width + real_frame_width,canvas_p1.y - custom_datas2[frame + 1] * canvas_sz.y },IM_COL32(100, 255,100,255),2.0f);

        if (frame < (trace->getMemoryInfos().size()))
            used_points.push_back({ x + real_frame_width * 0.5f,canvas_p1.y - (trace->getMemoryInfos()[frame].used )/ 1024 / 1024 * frame_scaling  });
    }



    if (/*is_focus &&*/ is_in_canvas && !is_scrolling)
    { 
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && !is_dragging && is_focus)
        {
            is_dragging = true;
            dragging_start = overlay_frame;
        }
        if (is_dragging )
        {
            dragging_end = overlay_frame;
            if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                is_dragging = false;
                if (dragging_end == dragging_start)
                    dragging_start = 0;

                select_range_cb(dragging_start + base_frame, dragging_end + base_frame);


            }
        }
    }

    //if (dragging_start != -1)
    {

        auto x0 = begin_x + dragging_start * frame_width;
        auto x1 = begin_x + dragging_end * frame_width;

        auto min_x = std::min(x0, x1);
        auto max_x = std::max(x0, x1);

        draw_list->AddRectFilled(ImVec2(min_x, canvas_p0.y), ImVec2(max_x + frame_width, canvas_p1.y), IM_COL32(255, 0, 0, 40));
    }

    // custom_data2
    for (int frame = 0; frame < ((int)line_points.size() - 1); ++frame)
    {
        draw_list->AddLine(line_points[frame], line_points[frame + 1], IM_COL32(100, 255, 100, 255), 2.0f);
    }

    // total
    for (int frame = 0; frame < ((int)used_points.size() - 1); ++frame)
    {
        draw_list->AddLine(used_points[frame], used_points[frame + 1], IM_COL32(255, 0, 0, 255), 2.0f);
    }

    int i = 1;
    for (float y = canvas_p1.y; y >= (canvas_p0.y - 20); y -= std_step, i++)
    {
        draw_list->AddLine(ImVec2(canvas_p0.x, y - std_step), ImVec2(canvas_p1.x, y - std_step), IM_COL32(200, 200, 200, 100));

        auto num = std::format("{0} MB", i * 512);
        auto str_len = ImGui::CalcTextSize(num.c_str(), num.c_str() + num.length());
        draw_list->AddText(ImVec2(canvas_p1.x - str_len.x - 5 , y - std_step - str_len.y), IM_COL32(200, 200, 200, 200), num.c_str());
    }


    draw_list->PopClipRect();

    //scrollbar background
    draw_list->AddRectFilled({ canvas_p0.x, canvas_p1.y }, { canvas_p1.x,canvas_p1.y + scrollbar_height  }, IM_COL32(255, 255, 255, 20), 0.0f);

    float show_frame_count = (canvas_sz.x + canvas_p0.x - begin_x) / frame_width;
    float scrollbar_end = 0;
    float scrollbar_start = 0;
    if (frames.size() > 0 )
    {
        scrollbar_end = canvas_p0.x + canvas_sz.x * show_frame_count / frames.size();
        scrollbar_start = canvas_p0.x + canvas_sz.x* (std::max(0.0f, canvas_p0.x -begin_x) / frame_width) / frames.size();
    }

    //drag scrollbar
    if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && io.MousePos.x >= canvas_p0.x && io.MousePos.x <= canvas_p1.x && io.MousePos.y >= canvas_p1.y && io.MousePos.y <= canvas_p1.y + scrollbar_height)
    {
        if (!is_scrolling)
            is_scrolling = true;
    }

    if (/*is_focus &&*/ io.MousePos.x >= canvas_p0.x && io.MousePos.x <= canvas_p1.x && is_scrolling)
    {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            auto scroll_width = scrollbar_end - scrollbar_start;
            int frame_pos = (io.MousePos.x - canvas_p0.x - scroll_width * 0.5f)  * frames.size() / canvas_sz.x;
            scrolling.x = ( - frame_pos * frame_width) ;
        }
        else
        {
            is_scrolling = false;
        }
    }

    for (auto& cp : trace->getCheckpoints())
    {
        float pos = canvas_sz.x * (float(cp.frameid - range.begin) / frames.size());
        draw_list->AddRectFilled({ pos, canvas_p1.y }, { pos + 2,canvas_p1.y + scrollbar_height }, IM_COL32(0, 255, 255, 200), 0.0f);
    }

    // scrollbar 
    draw_list->AddRectFilled({ scrollbar_start, canvas_p1.y }, { scrollbar_end,canvas_p1.y + scrollbar_height }, IM_COL32(255, 255, 255, 100), 0.0f);

    if (text_frame != -1)
    {
        std::string cp_str;
        int frameid = text_frame + range.begin;
        for (auto& cp : chkpnts)
        {
            if (frameid == cp.frameid)
            {
                cp_str = cp.text;
                break;
            }
        }


        auto frame = text_frame;

        struct TextContent
        {
            std::string text;
            unsigned int color;
        };

        std::vector<TextContent> content =
        {
            {std::to_string(frame), IM_COL32(255, 255, 255, 200)},
        };

        if (!cp_str.empty())
            content.push_back({ cp_str, IM_COL32(0, 255, 255, 255) });

        if (frame < frames.size())
            content.push_back({ std::format("trk:{0:.2f} MB", frames[frame]), IM_COL32_WHITE });

        if (frame < trace->getMemoryInfos().size())
        {
            content.push_back({std::format("phy:{0:.2f} MB", double(trace->getMemoryInfos()[frame].used) / (1024 * 1024)), IM_COL32(255, 0, 0, 255) });
            content.push_back({std::format("ovh:{0:.2f} MB", double(trace->getMemoryInfos()[frame].overhead) / (1024 * 1024)), IM_COL32(255, 150, 0, 255) });
        }


        auto x = begin_x + frame * frame_width;
        float width = 0;
        float height = 15;

        for (auto& c : content)
        {
            auto size = ImGui::CalcTextSize(c.text.c_str());
            width = std::max(width, size.x);
            height = std::max(height, size.y);
        }
        auto t_x = x + frame_width * 0.5f;
        auto t_y = canvas_p1.y - frames[frame] * frame_scaling - height - 80;

        draw_list->AddRectFilled({ t_x, t_y }, { t_x + width, t_y + height * content.size()}, IM_COL32_BLACK, 0.5f);

        for (int i = 0; i < content.size(); ++i)
        {
            auto& c = content[i];
            draw_list->AddText({ t_x, t_y + height * i}, c.color, c.text.c_str());
        }

    }
    ImGui::EndChild();


}

void TimelineView::SetCustomData(std::vector<float> datas)
{
    custom_datas = std::move(datas);
}
//
//void SetCustomData2(std::vector<float> datas) 
//{
//    custom_datas2 = std::move(datas);
//}
