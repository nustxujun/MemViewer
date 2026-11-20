#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>

#include <cstdint>
#include <limits>

#include "CoreTypes.h"
#include "CategoryAnalyzer.h"
#include "TimelineCalculator.h"
#include "CallstackAnalyzer.h"
#include "ObjectAnalyzer.h"
#include "RHIAnalyzer.h"
#include "CacheSerializer.h"
#include "Utils.h"

namespace py = pybind11;

// Helper: convert CallstackNodeInfo to dict
static py::dict node_to_dict(const CallstackNodeInfo& info) {
    py::dict d;
    d["node_index"] = info.node_index;
    d["name"] = info.name;
    d["size"] = info.size;
    d["basic_size"] = info.basic_size;
    d["count"] = info.count;
    d["basic_count"] = info.basic_count;
    d["is_matched"] = info.is_matched;
    d["children_count"] = info.children_count;
    return d;
}

// Holder for RHI analyzed data - allows get_rhi_resources to use the C++ vector
struct RHIAnalyzedHolder {
    std::vector<RHITypeInfo> analyzed;
};

PYBIND11_MODULE(TraceAnalyzer_py, m) {
    m.doc() = "TraceAnalyzer Python bindings for memory analysis";

    // FrameRange
    py::class_<FrameRange>(m, "FrameRange")
        .def_readwrite("begin", &FrameRange::begin)
        .def_readwrite("end", &FrameRange::end)
        .def("count", &FrameRange::count);

    // TraceData - exposed minimally
    py::class_<TraceData, std::shared_ptr<TraceData>>(m, "TraceData")
        .def_readonly("max_range", &TraceData::max_range);

    // TraceSession
    py::class_<TraceSession>(m, "TraceSession")
        .def("set_range", &TraceSession::SetRange)
        .def("get_ranged_data", &TraceSession::GetRangedData, py::return_value_policy::reference_internal)
        .def("get_full_data", &TraceSession::GetFullData, py::return_value_policy::reference_internal)
        .def("get_summary", [](const TraceSession& s) {
            py::dict d;
            auto& fd = s.GetFullData();
            d["frame_begin"] = fd.max_range.begin;
            d["frame_end"] = fd.max_range.end;
            d["frame_count"] = fd.max_range.count();
            d["alloc_count"] = fd.allocs.size();
            d["lua_alloc_count"] = fd.lua_allocs.size();
            d["object_count"] = fd.objects.size();
            d["rhi_count"] = fd.rhis.size();
            d["checkpoint_count"] = fd.checkpoints.size();
            d["node_count"] = fd.calltree ? fd.calltree->node_count : 0;
            d["lua_node_count"] = fd.lua_calltree ? fd.lua_calltree->node_count : 0;
            return d;
        })
        .def("get_checkpoints", [](const TraceSession& s) {
            py::list result;
            for (auto& cp : s.GetFullData().checkpoints) {
                py::dict d;
                d["text"] = cp.text;
                d["frameid"] = cp.frameid;
                result.append(d);
            }
            return result;
        })
        .def("get_memory_summary", [](const TraceSession& s) {
            auto& rd = s.GetRangedData();
            auto& fd = s.GetFullData();
            auto& infos = fd.memoryinfos;
            if (infos.empty()) return py::dict();

            // Get range indices
            int begin = std::max(0, rd.max_range.begin - fd.max_range.begin);
            int end = std::min((int)infos.size(), rd.max_range.end - fd.max_range.begin);

            uint64_t min_used = UINT64_MAX, max_used = 0, sum_used = 0;
            uint64_t min_avail = UINT64_MAX, max_avail = 0;
            int64_t min_ovh = INT64_MAX, max_ovh = INT64_MIN;
            int count = 0;

            for (int i = begin; i < end && i < (int)infos.size(); i++) {
                min_used = std::min(min_used, infos[i].used);
                max_used = std::max(max_used, infos[i].used);
                sum_used += infos[i].used;
                min_avail = std::min(min_avail, infos[i].available);
                max_avail = std::max(max_avail, infos[i].available);
                min_ovh = std::min(min_ovh, infos[i].overhead);
                max_ovh = std::max(max_ovh, infos[i].overhead);
                count++;
            }

            py::dict d;
            d["used_min_mb"] = min_used / 1024.0 / 1024.0;
            d["used_max_mb"] = max_used / 1024.0 / 1024.0;
            d["used_avg_mb"] = count > 0 ? (sum_used / count) / 1024.0 / 1024.0 : 0;
            d["available_min_mb"] = min_avail / 1024.0 / 1024.0;
            d["available_max_mb"] = max_avail / 1024.0 / 1024.0;
            d["overhead_min_mb"] = min_ovh / 1024.0 / 1024.0;
            d["overhead_max_mb"] = max_ovh / 1024.0 / 1024.0;
            return d;
        });

    // Calltree::Ptr wrapper for processed trees
    py::class_<Calltree, Calltree::Ptr>(m, "Calltree")
        .def_readonly("node_count", &Calltree::node_count);

    // PrecomputedData
    py::class_<PrecomputedData>(m, "PrecomputedData");

    // RHIAnalyzedHolder - for get_rhi_resources
    py::class_<RHIAnalyzedHolder, std::shared_ptr<RHIAnalyzedHolder>>(m, "RHIAnalyzedHolder")
        .def(py::init<>());

    // === Module-level functions ===

    // Load cache - returns (session, precomputed)
    m.def("load_cache", [](const std::string& path) {
        auto session = std::make_unique<TraceSession>();
        PrecomputedData precomputed;
        if (!CacheSerializer::DeserializeCache(path, &session->full_data, &precomputed)) {
            throw std::runtime_error("Failed to load cache: " + path);
        }
        session->ranged_data = session->full_data;
        session->selected_range = session->full_data.max_range;
        return py::make_tuple(std::move(session), std::move(precomputed));
    }, py::arg("path"));

    // Load trace directly
    m.def("load_trace", [](const std::string& path) {
        auto session = std::make_unique<TraceSession>();
        if (!TraceData::ParseTraceFile(path, &session->full_data)) {
            throw std::runtime_error("Failed to parse trace: " + path);
        }
        session->ranged_data = session->full_data;
        session->selected_range = session->full_data.max_range;
        return std::move(session);
    }, py::arg("path"));

    // GetDiff
    m.def("get_diff", [](const TraceData& d1, const TraceData& d2) {
        return TraceData::GetDiff(d1, d2);
    }, py::arg("data1"), py::arg("data2"));

    // Create a TraceSession from TraceData::Ptr (used for compare results)
    m.def("create_session_from_data", [](std::shared_ptr<TraceData> data) {
        auto session = std::make_unique<TraceSession>();
        session->full_data = std::move(*data);
        session->ranged_data = session->full_data;
        session->selected_range = session->full_data.max_range;
        return std::move(session);
    }, py::arg("data"));

    // === Category Analysis ===
    m.def("parse_category_config", [](const std::string& path) {
        auto cats = CategoryAnalyzer::ParseConfig(path);
        py::list result;
        for (auto& c : cats) {
            py::dict d;
            d["name"] = c.name;
            d["keys"] = c.keys;
            py::list subs;
            for (auto& s : c.subs) {
                py::dict sd;
                sd["name"] = s.name;
                sd["keys"] = s.keys;
                subs.append(sd);
            }
            d["subs"] = subs;
            result.append(d);
        }
        return result;
    }, py::arg("path"));

    m.def("analyze_categories", [](TraceSession& session, const std::string& config_path, const std::string& type) {
        auto cats = CategoryAnalyzer::ParseConfig(config_path);
        auto& rd = session.GetRangedData();
        auto calltree = (type == "lua") ? rd.lua_calltree : rd.calltree;
        if (!calltree) throw std::runtime_error("No calltree for type: " + type);

        auto [results, mapping] = CategoryAnalyzer::AnalyzeAndBuildMap(calltree, cats);

        py::list out;
        for (auto& r : results) {
            py::dict d;
            d["name"] = r.name;
            d["total_size"] = r.total_size;
            d["total_count"] = r.total_count;
            d["alloc_count"] = (int)r.allocs.size();
            out.append(d);
        }
        return out;
    }, py::arg("session"), py::arg("config_path"), py::arg("type") = "native");

    m.def("build_category_map", [](TraceSession& session, const std::string& config_path, const std::string& type) {
        auto cats = CategoryAnalyzer::ParseConfig(config_path);
        auto& rd = session.GetRangedData();
        auto calltree = (type == "lua") ? rd.lua_calltree : rd.calltree;
        if (!calltree) throw std::runtime_error("No calltree for type: " + type);
        auto mapping = CategoryAnalyzer::BuildCategoryMap(calltree, cats);
        py::list result;
        for (auto& m : mapping) {
            py::dict d;
            d["category_index"] = m.category_index;
            d["matched_node_index"] = m.matched_node_index;
            result.append(d);
        }
        return result;
    }, py::arg("session"), py::arg("config_path"), py::arg("type") = "native");

    // === Timeline ===
    m.def("get_precomputed_timeline", [](const PrecomputedData& pre, const std::string& name, int max_points) {
        for (auto& s : pre.basic_timelines) {
            if (s.name == name) {
                py::dict d;
                d["name"] = s.name;
                auto data = (max_points > 0 && (int)s.data.size() > max_points)
                    ? TimelineCalculator::Downsample(s.data, max_points) : s.data;
                d["data"] = data;
                d["frame_count"] = (int)s.data.size();
                return d;
            }
        }
        for (auto& s : pre.category_timelines) {
            if (s.name == name) {
                py::dict d;
                d["name"] = s.name;
                auto data = (max_points > 0 && (int)s.data.size() > max_points)
                    ? TimelineCalculator::Downsample(s.data, max_points) : s.data;
                d["data"] = data;
                d["frame_count"] = (int)s.data.size();
                return d;
            }
        }
        throw std::runtime_error("Timeline series not found: " + name);
    }, py::arg("precomputed"), py::arg("name"), py::arg("max_points") = 500);

    m.def("list_timeline_series", [](const PrecomputedData& pre) {
        py::list result;
        for (auto& s : pre.basic_timelines) {
            py::dict d;
            d["name"] = s.name;
            d["type"] = s.type == TimelineSeries::Bar ? "bar" : "line";
            d["frame_count"] = (int)s.data.size();
            result.append(d);
        }
        for (auto& s : pre.category_timelines) {
            py::dict d;
            d["name"] = s.name;
            d["type"] = s.type == TimelineSeries::Bar ? "bar" : "line";
            d["frame_count"] = (int)s.data.size();
            result.append(d);
        }
        return result;
    }, py::arg("precomputed"));

    m.def("downsample", &TimelineCalculator::Downsample, py::arg("data"), py::arg("target_count"));

    // === Callstack ===
    m.def("build_filtered_tree", [](TraceSession& session, const std::string& type,
            const std::vector<std::string>& filters, bool case_sensitive) {
        auto& rd = session.GetRangedData();
        auto calltree = (type == "lua") ? rd.lua_calltree : rd.calltree;
        if (!calltree) throw std::runtime_error("No calltree for type: " + type);
        return CallstackAnalyzer::BuildFilteredTree(calltree, filters, case_sensitive);
    }, py::arg("session"), py::arg("type") = "native",
       py::arg("filters") = std::vector<std::string>{}, py::arg("case_sensitive") = false);

    m.def("get_callstack_top", [](const Calltree::Ptr& tree, int depth, int64_t min_size, int limit) {
        auto nodes = CallstackAnalyzer::GetTopNodes(tree, depth, min_size, limit);
        py::list result;
        for (auto& n : nodes) result.append(node_to_dict(n));
        return result;
    }, py::arg("tree"), py::arg("depth") = 3, py::arg("min_size") = 0, py::arg("limit") = 50);

    m.def("get_callstack_children", [](const Calltree::Ptr& tree, int node_index) {
        auto nodes = CallstackAnalyzer::GetChildren(tree, node_index);
        py::list result;
        for (auto& n : nodes) result.append(node_to_dict(n));
        return result;
    }, py::arg("tree"), py::arg("node_index"));

    m.def("get_callstack_path", [](const Calltree::Ptr& tree, int node_index) {
        auto nodes = CallstackAnalyzer::GetPath(tree, node_index);
        py::list result;
        for (auto& n : nodes) result.append(node_to_dict(n));
        return result;
    }, py::arg("tree"), py::arg("node_index"));

    m.def("search_callstack", [](const Calltree::Ptr& tree, const std::vector<std::string>& keywords,
            bool case_sensitive, int limit) {
        auto nodes = CallstackAnalyzer::Search(tree, keywords, case_sensitive, limit);
        py::list result;
        for (auto& n : nodes) result.append(node_to_dict(n));
        return result;
    }, py::arg("tree"), py::arg("keywords"), py::arg("case_sensitive") = false, py::arg("limit") = 50);

    m.def("get_node_info", [](const Calltree::Ptr& tree, int node_index) {
        return node_to_dict(CallstackAnalyzer::GetNodeInfo(tree, node_index));
    }, py::arg("tree"), py::arg("node_index"));

    // === Objects ===
    m.def("get_object_classes", [](TraceSession& session, const std::string& type, int limit, int offset) {
        auto& rd = session.GetRangedData();
        auto summaries = ObjectAnalyzer::GetClassSummary(rd.objects, limit, offset);
        py::list result;
        for (auto& s : summaries) {
            py::dict d;
            d["class_name"] = s.class_name;
            d["class_chain"] = s.class_chain;
            d["add_count"] = s.add_count;
            d["delete_count"] = s.delete_count;
            result.append(d);
        }
        return result;
    }, py::arg("session"), py::arg("type") = "native", py::arg("limit") = 50, py::arg("offset") = 0);

    m.def("get_objects", [](TraceSession& session, const std::string& type,
            const std::string& class_filter, int limit, int offset) {
        auto& rd = session.GetRangedData();
        auto objs = ObjectAnalyzer::GetObjectsByClass(rd.objects, class_filter, limit, offset);
        py::list result;
        for (auto* obj : objs) {
            py::dict d;
            d["name"] = obj->name;
            d["begin"] = obj->begin;
            d["end"] = obj->end;
            d["state"] = obj->state;
            if (obj->class_chain) d["class_chain"] = obj->class_chain->name_chain;
            auto stack = ObjectAnalyzer::GetObjectCallstack(*obj);
            d["callstack"] = stack;
            result.append(d);
        }
        return result;
    }, py::arg("session"), py::arg("type") = "native",
       py::arg("class_filter") = "", py::arg("limit") = 50, py::arg("offset") = 0);

    // === RHI ===
    m.def("analyze_rhi", [](TraceSession& session, const std::string& type, const std::string& config_path) {
        auto& rd = session.GetRangedData();
        auto analyzed = RHIAnalyzer::Analyze(rd.rhis, config_path);
        auto holder = std::make_shared<RHIAnalyzedHolder>();
        holder->analyzed = analyzed;

        py::list result;
        for (auto& t : analyzed) {
            py::dict d;
            d["type"] = t.type;
            d["name"] = t.name;
            d["count"] = t.count;
            d["size"] = t.size;
            py::list subs;
            for (auto& s : t.sub_types) {
                py::dict sd;
                sd["name"] = s.name;
                sd["count"] = s.count;
                sd["size"] = s.size;
                sd["group_count"] = (int)s.groups.size();
                subs.append(sd);
            }
            d["sub_types"] = subs;
            result.append(d);
        }
        return py::make_tuple(result, holder);
    }, py::arg("session"), py::arg("type") = "native", py::arg("config_path") = "");

    m.def("get_rhi_resources", [](const std::shared_ptr<RHIAnalyzedHolder>& holder, int type_idx, int sub_type,
            const std::string& name_filter, int texture_type, int pixel_format, int limit, int offset) {
        if (!holder || type_idx < 0 || type_idx >= (int)holder->analyzed.size())
            return py::list();

        auto groups = RHIAnalyzer::GetResources(holder->analyzed, type_idx, sub_type,
            name_filter, texture_type, pixel_format, limit, offset);

        py::list result;
        for (auto& g : groups) {
            py::dict d;
            d["name"] = g.name;
            d["count"] = g.count;
            d["size"] = g.size;
            py::list items;
            for (auto& rhi : g.items) {
                py::dict rd;
                rd["ptr"] = rhi.ptr;
                rd["name"] = rhi.name;
                rd["begin"] = rhi.begin;
                rd["end"] = rhi.end;
                rd["node_index"] = rhi.node_index;
                rd["type"] = rhi.type;
                rd["size"] = rhi.size;
                rd["attrs"] = rhi.attrs;
                rd["texture_detail"] = RHIAnalyzer::GetTextureDetail(rhi);
                items.append(rd);
            }
            d["items"] = items;
            result.append(d);
        }
        return result;
    }, py::arg("analyzed_holder"), py::arg("type_idx"), py::arg("sub_type") = -1,
       py::arg("name_filter") = "", py::arg("texture_type") = 0, py::arg("pixel_format") = 0,
       py::arg("limit") = 50, py::arg("offset") = 0);

    m.def("get_format_name", [](int format) {
        return std::string(RHIAnalyzer::GetFormatName(format));
    }, py::arg("format"));

    m.def("get_texture_detail", [](py::dict rhi_dict) {
        RhiInfo rhi;
        if (rhi_dict.contains("ptr")) rhi.ptr = rhi_dict["ptr"].cast<uint64_t>();
        if (rhi_dict.contains("name")) rhi.name = rhi_dict["name"].cast<std::string>();
        if (rhi_dict.contains("begin")) rhi.begin = rhi_dict["begin"].cast<uint32_t>();
        if (rhi_dict.contains("end")) rhi.end = rhi_dict["end"].cast<uint32_t>();
        if (rhi_dict.contains("node_index")) rhi.node_index = rhi_dict["node_index"].cast<uint32_t>();
        if (rhi_dict.contains("type")) rhi.type = rhi_dict["type"].cast<uint32_t>();
        if (rhi_dict.contains("size")) rhi.size = rhi_dict["size"].cast<int32_t>();
        if (rhi_dict.contains("attrs")) rhi.attrs = rhi_dict["attrs"].cast<std::vector<uint32_t>>();
        return RHIAnalyzer::GetTextureDetail(rhi);
    }, py::arg("rhi_dict"));
}
