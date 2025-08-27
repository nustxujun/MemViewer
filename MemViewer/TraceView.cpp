#include "TraceView.h"
#include "TraceInstance.h"
#include "imgui/imgui.h"
#include "Timeline.h"
#include "CallstackView.h"
#include "CategoryView.h"
#include "RHIView.h"
#include "ObjectView.h"
#include <fstream>
#include <sstream>

TraceView::TraceView()
{
	AddNewTrace(0);
}


void TraceView::AddNewTrace(int split)
{


	//if (splits[split].traces.size() > 0 && (*splits[split].traces.rbegin())->path == "blank")
	//	return;
	AddTab(split, std::make_shared<Trace>(this));
}

void TraceView::AddTab(int split, TabView::Ptr view)
{
	if (splits.size() <= split)
	{
		splits.resize(split + 1);
	}

	splits[split].traces.emplace_back(view);
}

void TraceView::Show()
{


	if (splits.size() > 1)
	{
		for (auto i = splits.begin(); i != splits.end(); )
		{
			if (i->traces.size() == 0)
			{
				i = splits.erase(i);
			}
			else
			{
				i++;
			}
		}
	}


	if (ImGui::BeginMainMenuBar())
	{
		
		ImGui::EndMainMenuBar();
	}



	ImGuiTableFlags flags = ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_Resizable   | ImGuiTableFlags_ContextMenuInBody;


	int num_splits = splits.size();
	if (ImGui::BeginTable("table", num_splits, flags))
	{
		ImGui::TableNextRow();
		for (int i = 0; i < num_splits; ++i)
		{
			ImGui::TableSetColumnIndex(i);

			ImGui::BeginChild(i + 1);
			auto& traces = splits[i].traces;
			if (ImGui::BeginTabBar("Traces"), ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_FittingPolicyResizeDown | ImGuiTabBarFlags_AutoSelectNewTabs)
			{

				std::vector<int> removelist;
				for (int j = 0; j < traces.size(); ++j)
				{
					auto trace = traces[j];
					bool open = true;
					ImGui::PushID(j);
					if (ImGui::BeginTabItem((trace->getName() + trace->num_str).c_str(), &open))
					{
						if (ImGui::Button("<-"))
						{
							Move(i, j, i - 1);
						}
						ImGui::SameLine();
						if (ImGui::Button("->"))
						{
							Move(i, j, i + 1);
						}
						ImGui::SameLine();
						if (ImGui::Button("|<->|"))
						{
							MoveToNewSplit(i, j);
						}

						trace->Show(i,j);

						ImGui::EndTabItem();
					}
					ImGui::PopID();

					if (!open)
					{
						removelist.insert(removelist.begin(), j);
					}
				}

				for (auto& idx : removelist)
				{
					traces[idx]->discard();
					traces.erase(traces.begin() + idx);
				}


				if (ImGui::TabItemButton("+", ImGuiTabItemFlags_Trailing | ImGuiTabItemFlags_NoTooltip))
				{
					AddNewTrace(i);
				}

				ImGui::EndTabBar();
			}

			ImGui::EndChild();
		}



		ImGui::EndTable();
	}

	if (popup_comparation)
	{
		ImGui::OpenPopup("Comparation");
		popup_comparation = false;
	}
	//if (ImGui::BeginPopupModal("Comparation", 0, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
	if (ImGui::BeginPopup("Comparation"))
	{
		int grp = 0;
		for (auto& split : splits)
		{
			int idx = 0;
			for (auto& view : split.traces)
			{
				auto trace = view->getTrace();

				if (!(grp == CmpGrp && idx == CmpIdx) && trace && ImGui::Selectable((view->getName() + view->num_str).c_str()))
				{
					move_functions.push_back([=]() {
						auto src = splits[CmpGrp].traces[CmpIdx];
						auto dst = splits[grp].traces[idx];
						auto diff = src->getTrace()->getDiff(dst->getTrace());

						auto view = std::make_shared<SpanView>(std::format("({}) <-> ({})", src->getName(), dst->getName()), std::format("Span_{}_{}", src->getLongName(), dst->getLongName()), this, diff);

						AddTab(CmpGrp + 1, view);

						});
					ImGui::CloseCurrentPopup();
				}
				++idx;
			}
			++grp;
		}

		ImGui::EndPopup();
	}



	for (auto& f : move_functions)
	{
		f();
	}
	move_functions.clear();

}
void TraceView::Move(int src_grp, int src_idx, int dst_grp)
{
	move_functions.push_back([=]() {
		if ( src_grp < 0 || src_grp >= splits.size() || src_idx < 0 || src_idx >= splits[src_grp].traces.size() )
			return;


		auto v = splits[src_grp].traces[src_idx];
		splits[src_grp].traces.erase(splits[src_grp].traces.begin() + src_idx);

		if (dst_grp < 0)
			splits.insert(splits.begin(),{{v}});
		else if (dst_grp >= splits.size())
			splits.push_back({{v}});
		else
			splits[dst_grp].traces.push_back(v);

	});


}

void TraceView::MoveToNewSplit(int src_grp, int src_idx)
{
	move_functions.push_back([=](){
		if (splits[src_grp].traces.size() > 1)
		{
			auto v = splits[src_grp].traces[src_idx];
			splits[src_grp].traces.erase(splits[src_grp].traces.begin() + src_idx);

			splits.insert(splits.begin() + src_grp, { {v} });
		}
	});

}

void TraceView::Compare(int src_grp, int src_idx)
{
	popup_comparation = true;
	CmpGrp = src_grp;
	CmpIdx = src_idx;
	//ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	//ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));

	//if (ImGui::BeginPopupModal("Comparation", 0, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))


}
void TraceView::Span(int group, int dst_idx)
{
	move_functions.push_back([=]() {
		auto src = splits[group].traces[dst_idx];
		auto trace = src->getTrace();

		auto ret_data = std::make_shared<TraceData>();

		if (trace->getCalltree())
			ret_data->calltree = trace->getCalltree()->clone();
		if (trace->getLuaCalltree())
			ret_data->lua_calltree = trace->getLuaCalltree()->clone();

		ret_data->objects = trace->getObjects();
		ret_data->rhis = trace->getRHIs();

		auto diff = std::make_shared<TraceInstanceDiff>(ret_data);

		auto view = std::make_shared<SpanView>(std::format("{} Span", src->getName()), std::format("Span_{}", src->getLongName()), this, diff);

		AddTab(group + 1, view);
	});
}

TraceView::Trace::Trace(TraceView* v):trace_view(v)
{
	static int unique_id = 1;
	index = unique_id++;
	auto tl = new TimelineView("Timeline");
	tl->notifySelectedRange([this](uint32_t begin, uint32_t end) {
		timeline->syncTask(true);



		for (auto& v : views)
		{
			v->syncTask(true);
		}

		TraceInstance::ParseRange(begin, end,std::dynamic_pointer_cast<TraceInstance>(trace_file));

	});
	timeline = std::shared_ptr<View>(tl);
	views = {
		std::shared_ptr<View>(new CallstackView("CallTree")),
		std::shared_ptr<View>(new CategoryView("Category", tl)),
		std::shared_ptr<View>(new RHIView("RHI")),
		std::shared_ptr<View>(new ObjectView("UObject"))

	};

	standalones.resize(views.size());
}


void TraceView::Trace::Show(int group, int index)
{
	ImGui::SameLine();
	if (trace_file && trace_file->isPrepared() && ImGui::Button("compare"))
	{
		trace_view->Compare(group, index);
	}

	ImGui::SameLine();
	if (trace_file && trace_file->isPrepared() && ImGui::Button("span"))
	{
		trace_view->Span(group, index);
	}
	ImGui::SameLine();

	ImGui::Text(GBKToUTF8(path).c_str());

	if (file_browser.isLoading())
	{
		auto Space = ImGui::GetContentRegionAvail();
		ImGui::SetCursorPosX(Space.x / 2 - 3);
		ImGui::SetCursorPosY(Space.y / 2 - 3);
		Spinner("##spinner", 15, 6, ImGui::GetColorU32(ImGuiCol_ButtonHovered));
		return;
	}

	if (path.empty())
	{
		file_browser.ShowFileBrowser([trace = weak_from_this(), trace_view = trace_view, group](std::string p, int type)
		{
			if (trace.expired())
				return;

			if (type == 0)
			{
				trace.lock()->trace_file = TraceInstance::Create(p, [trace, group](TraceInstanceBasic::Ptr trace_file) {
					if (trace.expired())
						return;
					auto shared = trace.lock();
					shared->timeline->SetTrace(trace_file);
					shared->timeline->Initialize();


					for (auto& view : shared->views)
					{
						view->SetTrace(trace_file);
						view->Initialize();
					}
				});
				trace.lock()->path = p.substr(9,p.length() - 13 - 9);
			}
			else
			{
				trace.lock()->trace_file = TraceInstanceDiff::CreateFromFile(p,[trace, p, trace_view, group](TraceInstanceDiff::Ptr trace_file){
					if (trace.expired())
						return;
					auto view = std::make_shared<SpanView>(p, p, trace_view, trace_file);
					trace_view->AddTab(group,view);
				});
			}
		});
	}
	else if (trace_file && trace_file->isUpdated())
	{
		timeline->Show(0, 200, ImGuiChildFlags_ResizeY | ImGuiChildFlags_Borders);


		for (auto& view : views)
		{
			view->Update();
		}

		trace_file->Prepare();
	}
	else
	{
		timeline->Show(0, 200, ImGuiChildFlags_ResizeY | ImGuiChildFlags_Borders);
		ImGui::BeginChild("Views");
		if (ImGui::BeginTabBar("Traces"), ImGuiTabBarFlags_FittingPolicyResizeDown)
		{

			int idx = 0;
			for (auto& view : views)
			{
				uint8_t& standalone = standalones[idx];
				if (!standalone && ImGui::BeginTabItem(view->getName().c_str()))
				{
					if (ImGui::Button("standalone"))
					{
						trace_view->AddTab(group + 1, std::make_shared<StandaloneView>(std::format("{}({})",view->getName(),getName()),view, &standalone));
						standalone = 1;
					}
					view->Show();
					ImGui::EndTabItem();
				}

				idx++;
			}

			ImGui::EndTabBar();
		}

		ImGui::EndChild();
	}
}

std::string TraceView::Trace::getName()
{
	return std::format("Trace {}", index);
}

std::string TraceView::Trace::getLongName()
{
	return std::format("Trace_{}", path);
}

void StandaloneView::Show(int, int)
{
	view->Show();
}


SpanView::SpanView(std::string n,std::string ln, TraceView* view, std::shared_ptr<TraceInstanceBasic> data) :name(std::move(n)), long_name(std::move(ln)),trace_view(view), trace_file(std::move(data))
{
	views = {
		std::shared_ptr<View>(new CallstackView("CallTree")),
		std::shared_ptr<View>(new CategoryView("Category")),
		std::shared_ptr<View>(new RHIView("RHI")),
		std::shared_ptr<View>(new ObjectView("UObject"))
	};
	standalones.resize(views.size());
	trace_file->Prepare();
	for (auto& v : views)
	{
		v->SetTrace(trace_file);
		v->Initialize();
		v->Update();
	}
}

void SpanView::Show(int group, int index)
{

	ImGui::SameLine();
	if (trace_file && trace_file->isPrepared() && ImGui::Button("compare"))
	{
		trace_view->Compare(group, index);
	}
	ImGui::SameLine();

	if (trace_file && trace_file->isPrepared() && ImGui::Button("upload"))
	{
		std::vector<char> buffer;
		buffer.resize(1024);
		memcpy(buffer.data(), long_name.c_str(), long_name.length() + 1);
		ModalWindow::OpenModalWindow("upload",[=, buffer = std::move(buffer)]() mutable {
			buffer.resize(1024);
			ImGui::SetNextItemWidth(1024);
			ImGui::InputText("", buffer.data(), 1024);


			if (ImGui::Button("confirm"))
			{
				upload(UTF8ToGBK( buffer.data()));
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("cancal"))
			{
				ImGui::CloseCurrentPopup();
			}
		});
	}
	ImGui::SameLine();

	ImGui::Text(GBKToUTF8(long_name).c_str());


	ImGui::BeginChild("Views");
	if (ImGui::BeginTabBar("Traces"), ImGuiTabBarFlags_FittingPolicyResizeDown)
	{

		int idx = 0;
		for (auto& view : views)
		{
			uint8_t& standalone = standalones[idx];
			if (!standalone && ImGui::BeginTabItem(view->getName().c_str()))
			{
				if (ImGui::Button("standalone"))
				{
					trace_view->AddTab(group + 1, std::make_shared<StandaloneView>(std::format("{}({})", view->getName(), getName()), view, &standalone));
					standalone = 1;
				}
				view->Show();
				ImGui::EndTabItem();
			}

			idx++;
		}

		ImGui::EndTabBar();
	}

	ImGui::EndChild();
}


void SpanView::upload(std::string name)
{
	const int version = 1;
	

	auto now = std::chrono::system_clock::now();
	auto seconds = std::chrono::floor<std::chrono::seconds>(now);
	auto local_time = std::chrono::current_zone()->to_local(seconds);
	auto formatted_time = std::format("{:%Y_%m_%d-%H_%M_%S}_", local_time);

	std::fstream output("upload", std::ios::out | std::ios::binary);
	

	TraceData::SerializeTraceSpan(trace_file->getTraceData().get(),[&](uint8_t* buffer, int size){
		output.write((const char*)buffer, size);
	},true);



	output.close();

	static std::string upload_file_api = "http://9.134.132.170:8080/upload_span";

	run_cmd(std::format("tar -czvf upload.zip upload"));
	run_cmd(std::format("curl -F \"file=@{};filename={}{}\" {}", "upload.zip", formatted_time, name + ".zip", upload_file_api));
}