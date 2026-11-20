#include "CacheSerializer.h"
#include <fstream>
#include <filesystem>
#include <cstring>
#include <map>
#include <new>

static constexpr char CACHE_MAGIC[] = "MCACHE";
static constexpr uint32_t CACHE_VERSION = 1;

class BinaryWriter {
    std::ofstream& out;
public:
    BinaryWriter(std::ofstream& s) : out(s) {}
    void write_raw(const void* data, size_t size) { out.write(static_cast<const char*>(data), size); }
    template<typename T> void write_val(const T& val) { write_raw(&val, sizeof(T)); }
    void write_string(const std::string& s) {
        uint32_t len = static_cast<uint32_t>(s.size());
        write_val(len);
        if (len > 0) write_raw(s.data(), len);
    }
    template<typename T> void write_vec(const std::vector<T>& v) {
        uint32_t count = static_cast<uint32_t>(v.size());
        write_val(count);
        if (count > 0) write_raw(v.data(), sizeof(T) * count);
    }
};

class BinaryReader {
    std::ifstream& in;
public:
    BinaryReader(std::ifstream& s) : in(s) {}
    bool read_raw(void* data, size_t size) { in.read(static_cast<char*>(data), size); return in.good(); }
    template<typename T> bool read_val(T& val) { return read_raw(&val, sizeof(T)); }
    bool read_string(std::string& s) {
        uint32_t len;
        if (!read_val(len)) return false;
        s.resize(len);
        if (len > 0) return read_raw(s.data(), len);
        return true;
    }
    template<typename T> bool read_vec(std::vector<T>& v) {
        uint32_t count;
        if (!read_val(count)) return false;
        v.resize(count);
        if (count > 0) return read_raw(v.data(), sizeof(T) * count);
        return true;
    }
};

// ── Calltree ────────────────────────────────────────────────────────────────

static void SerializeCalltree(BinaryWriter& w, const Calltree::Ptr& tree) {
    uint32_t node_count = tree ? tree->node_count : 0;
    w.write_val(node_count);
    if (node_count == 0) return;

    w.write_string(tree->root->name);

    for (int i = 1; i < static_cast<int>(node_count); i++) {
        Node* node = tree->get(i);
        int32_t parent_index = node->parent.index;
        w.write_val(parent_index);
        w.write_string(node->name);
        w.write_val(node->basic_size);
        w.write_val(node->basic_count);
    }
}

static bool DeserializeCalltree(BinaryReader& r, Calltree::Ptr& tree) {
    uint32_t node_count;
    if (!r.read_val(node_count)) return false;
    if (node_count == 0) { tree = nullptr; return true; }

    tree = Calltree::create(node_count);

    for (uint32_t i = 1; i < node_count; i++)
        new (tree->get(i)) Node();

    if (!r.read_string(tree->root->name)) return false;

    for (uint32_t i = 1; i < node_count; i++) {
        Node* node = tree->get(i);
        int32_t parent_index;
        if (!r.read_val(parent_index)) return false;
        node->parent = NodeRef(tree, parent_index);
        if (!r.read_string(node->name)) return false;
        if (!r.read_val(node->basic_size)) return false;
        if (!r.read_val(node->basic_count)) return false;
    }

    for (uint32_t i = 1; i < node_count; i++) {
        int parent_idx = tree->get(i)->parent.index;
        tree->get(parent_idx)->children.push_back(NodeRef(tree, static_cast<int>(i)));
    }

    return true;
}

// ── Allocs ──────────────────────────────────────────────────────────────────

static void SerializeAllocs(BinaryWriter& w, const std::vector<AllocInfo>& allocs) {
    w.write_val(static_cast<uint32_t>(allocs.size()));
    for (const auto& a : allocs) {
        w.write_val(a.size);
        int32_t node_index = a.node ? a.node.index : -1;
        w.write_val(node_index);
        w.write_val(a.start);
        w.write_val(a.end);
        w.write_val(a.user_tag);
    }
}

static bool DeserializeAllocs(BinaryReader& r, std::vector<AllocInfo>& allocs,
                              const Calltree::Ptr& tree) {
    uint32_t count;
    if (!r.read_val(count)) return false;
    allocs.resize(count);
    for (uint32_t i = 0; i < count; i++) {
        if (!r.read_val(allocs[i].size)) return false;
        int32_t node_index;
        if (!r.read_val(node_index)) return false;
        if (tree && node_index >= 0)
            allocs[i].node = NodeRef(tree, node_index);
        if (!r.read_val(allocs[i].start)) return false;
        if (!r.read_val(allocs[i].end)) return false;
        if (!r.read_val(allocs[i].user_tag)) return false;
    }
    return true;
}

// ── Timelines ───────────────────────────────────────────────────────────────

static void SerializeTimelines(BinaryWriter& w, const std::vector<TimelineSeries>& series) {
    w.write_val(static_cast<uint32_t>(series.size()));
    for (const auto& s : series) {
        w.write_string(s.name);
        w.write_val(static_cast<uint8_t>(s.type));
        w.write_val(static_cast<uint8_t>(s.stacking ? 1 : 0));
        uint32_t data_count = static_cast<uint32_t>(s.data.size());
        w.write_val(data_count);
        if (data_count > 0)
            w.write_raw(s.data.data(), sizeof(float) * data_count);
    }
}

static bool DeserializeTimelines(BinaryReader& r, std::vector<TimelineSeries>& series) {
    uint32_t series_count;
    if (!r.read_val(series_count)) return false;
    series.resize(series_count);
    for (uint32_t i = 0; i < series_count; i++) {
        if (!r.read_string(series[i].name)) return false;
        uint8_t type, stacking;
        if (!r.read_val(type)) return false;
        if (!r.read_val(stacking)) return false;
        series[i].type = static_cast<TimelineSeries::Type>(type);
        series[i].stacking = stacking != 0;
        uint32_t data_count;
        if (!r.read_val(data_count)) return false;
        series[i].data.resize(data_count);
        if (data_count > 0)
            if (!r.read_raw(series[i].data.data(), sizeof(float) * data_count)) return false;
    }
    return true;
}

// ── Category maps ───────────────────────────────────────────────────────────

static void SerializeCategoryMap(BinaryWriter& w, const std::vector<CategoryMapping>& map) {
    w.write_val(static_cast<uint32_t>(map.size()));
    for (const auto& m : map) {
        w.write_val(m.category_index);
        w.write_val(m.matched_node_index);
    }
}

static bool DeserializeCategoryMap(BinaryReader& r, std::vector<CategoryMapping>& map) {
    uint32_t count;
    if (!r.read_val(count)) return false;
    map.resize(count);
    for (uint32_t i = 0; i < count; i++) {
        if (!r.read_val(map[i].category_index)) return false;
        if (!r.read_val(map[i].matched_node_index)) return false;
    }
    return true;
}

// ── SerializeCache ──────────────────────────────────────────────────────────

bool CacheSerializer::SerializeCache(const TraceData& data,
                                     const PrecomputedData& precomputed,
                                     const std::string& output_path) {
    std::ofstream file(output_path, std::ios::binary);
    if (!file) return false;
    BinaryWriter w(file);

    // Header
    w.write_raw(CACHE_MAGIC, 6);
    w.write_val(CACHE_VERSION);

    namespace fs = std::filesystem;
    std::string source_path = output_path;
    auto dot = source_path.rfind('.');
    if (dot != std::string::npos)
        source_path = source_path.substr(0, dot);
    source_path += ".memtrace";
    uint64_t source_hash = fs::exists(source_path) ? ComputeSourceHash(source_path) : 0;
    w.write_val(source_hash);

    // CALLTREE
    SerializeCalltree(w, data.calltree);

    // ALLOCS
    SerializeAllocs(w, data.allocs);

    // LUA_CALLTREE
    SerializeCalltree(w, data.lua_calltree);

    // LUA_ALLOCS
    SerializeAllocs(w, data.lua_allocs);

    // MEMORYINFOS
    w.write_val(static_cast<uint32_t>(data.memoryinfos.size()));
    for (const auto& mi : data.memoryinfos) {
        w.write_val(mi.used);
        w.write_val(mi.available);
        w.write_val(mi.overhead);
        uint32_t custom_count = static_cast<uint32_t>(mi.custom_datas.size());
        w.write_val(custom_count);
        for (const auto& [key, val] : mi.custom_datas) {
            w.write_string(key);
            w.write_val(val);
        }
    }

    // OBJECTS
    std::map<ObjClassInfo*, int32_t> class_index_map;
    std::vector<ObjClassInfo::Ptr> unique_classes;
    for (const auto& obj : data.objects) {
        if (obj.class_chain &&
            class_index_map.find(obj.class_chain.get()) == class_index_map.end()) {
            class_index_map[obj.class_chain.get()] =
                static_cast<int32_t>(unique_classes.size());
            unique_classes.push_back(obj.class_chain);
        }
    }

    w.write_val(static_cast<uint32_t>(unique_classes.size()));
    for (const auto& cls : unique_classes) {
        w.write_val(static_cast<uint32_t>(cls->name_chain.size()));
        for (const auto& name : cls->name_chain)
            w.write_string(name);
    }

    w.write_val(static_cast<uint32_t>(data.objects.size()));
    for (const auto& obj : data.objects) {
        w.write_string(obj.name);
        int32_t node_index = obj.node ? obj.node.index : -1;
        w.write_val(node_index);
        int32_t class_idx = obj.class_chain
            ? class_index_map[obj.class_chain.get()] : -1;
        w.write_val(class_idx);
        w.write_val(obj.begin);
        w.write_val(obj.end);
        w.write_val(obj.state);
    }

    // RHIS
    w.write_val(static_cast<uint32_t>(data.rhis.size()));
    for (const auto& rhi : data.rhis) {
        w.write_val(rhi.ptr);
        w.write_string(rhi.name);
        w.write_val(rhi.begin);
        w.write_val(rhi.end);
        w.write_val(rhi.node_index);
        w.write_val(rhi.type);
        w.write_val(rhi.size);
        if (rhi.type < 5) {
            for (int j = 0; j < 6; j++) {
                uint32_t attr = j < static_cast<int>(rhi.attrs.size())
                    ? rhi.attrs[j] : 0;
                w.write_val(attr);
            }
        }
    }

    // CHECKPOINTS
    w.write_val(static_cast<uint32_t>(data.checkpoints.size()));
    for (const auto& cp : data.checkpoints) {
        w.write_val(cp.frameid);
        w.write_string(cp.text);
    }

    // META
    w.write_val(data.max_range.begin);
    w.write_val(data.max_range.end);

    // TIMELINE_BASIC
    SerializeTimelines(w, precomputed.basic_timelines);

    // TIMELINE_CATEGORIES
    SerializeTimelines(w, precomputed.category_timelines);

    // CATEGORY_MAP_NATIVE
    SerializeCategoryMap(w, precomputed.native_category_map);

    // CATEGORY_MAP_LUA
    SerializeCategoryMap(w, precomputed.lua_category_map);

    // SUBCATEGORY_MAPS
    w.write_val(static_cast<uint32_t>(precomputed.native_subcategory_maps.size()));
    for (const auto& sub_map : precomputed.native_subcategory_maps)
        SerializeCategoryMap(w, sub_map);

    return file.good();
}

// ── DeserializeCache ────────────────────────────────────────────────────────

bool CacheSerializer::DeserializeCache(const std::string& input_path,
                                       TraceData* data,
                                       PrecomputedData* precomputed) {
    std::ifstream file(input_path, std::ios::binary);
    if (!file) return false;
    BinaryReader r(file);

    // Header
    char magic[6];
    if (!r.read_raw(magic, 6)) return false;
    if (std::memcmp(magic, CACHE_MAGIC, 6) != 0) return false;

    uint32_t version;
    if (!r.read_val(version)) return false;
    if (version != CACHE_VERSION) return false;

    uint64_t source_hash;
    if (!r.read_val(source_hash)) return false;

    // CALLTREE
    if (!DeserializeCalltree(r, data->calltree)) return false;

    // ALLOCS
    if (!DeserializeAllocs(r, data->allocs, data->calltree)) return false;

    // LUA_CALLTREE
    if (!DeserializeCalltree(r, data->lua_calltree)) return false;

    // LUA_ALLOCS
    if (!DeserializeAllocs(r, data->lua_allocs, data->lua_calltree)) return false;

    // MEMORYINFOS
    {
        uint32_t count;
        if (!r.read_val(count)) return false;
        data->memoryinfos.resize(count);
        for (uint32_t i = 0; i < count; i++) {
            auto& mi = data->memoryinfos[i];
            if (!r.read_val(mi.used)) return false;
            if (!r.read_val(mi.available)) return false;
            if (!r.read_val(mi.overhead)) return false;
            uint32_t custom_count;
            if (!r.read_val(custom_count)) return false;
            for (uint32_t j = 0; j < custom_count; j++) {
                std::string key;
                int64_t val;
                if (!r.read_string(key)) return false;
                if (!r.read_val(val)) return false;
                mi.custom_datas[std::move(key)] = val;
            }
        }
    }

    // OBJECTS
    {
        uint32_t class_count;
        if (!r.read_val(class_count)) return false;
        std::vector<ObjClassInfo::Ptr> classes(class_count);
        for (uint32_t i = 0; i < class_count; i++) {
            classes[i] = std::make_shared<ObjClassInfo>();
            classes[i]->index = static_cast<int>(i);
            uint32_t chain_len;
            if (!r.read_val(chain_len)) return false;
            classes[i]->name_chain.resize(chain_len);
            for (uint32_t j = 0; j < chain_len; j++)
                if (!r.read_string(classes[i]->name_chain[j])) return false;
        }

        uint32_t obj_count;
        if (!r.read_val(obj_count)) return false;
        data->objects.resize(obj_count);
        for (uint32_t i = 0; i < obj_count; i++) {
            auto& obj = data->objects[i];
            if (!r.read_string(obj.name)) return false;
            int32_t node_index;
            if (!r.read_val(node_index)) return false;
            if (data->calltree && node_index >= 0)
                obj.node = NodeRef(data->calltree, node_index);
            int32_t class_idx;
            if (!r.read_val(class_idx)) return false;
            if (class_idx >= 0 && class_idx < static_cast<int32_t>(classes.size()))
                obj.class_chain = classes[class_idx];
            if (!r.read_val(obj.begin)) return false;
            if (!r.read_val(obj.end)) return false;
            if (!r.read_val(obj.state)) return false;
        }
    }

    // RHIS
    {
        uint32_t count;
        if (!r.read_val(count)) return false;
        data->rhis.resize(count);
        for (uint32_t i = 0; i < count; i++) {
            auto& rhi = data->rhis[i];
            if (!r.read_val(rhi.ptr)) return false;
            if (!r.read_string(rhi.name)) return false;
            if (!r.read_val(rhi.begin)) return false;
            if (!r.read_val(rhi.end)) return false;
            if (!r.read_val(rhi.node_index)) return false;
            if (!r.read_val(rhi.type)) return false;
            if (!r.read_val(rhi.size)) return false;
            if (rhi.type < 5) {
                rhi.attrs.resize(6);
                for (int j = 0; j < 6; j++)
                    if (!r.read_val(rhi.attrs[j])) return false;
            }
        }
    }

    // CHECKPOINTS
    {
        uint32_t count;
        if (!r.read_val(count)) return false;
        data->checkpoints.resize(count);
        for (uint32_t i = 0; i < count; i++) {
            if (!r.read_val(data->checkpoints[i].frameid)) return false;
            if (!r.read_string(data->checkpoints[i].text)) return false;
        }
    }

    // META
    if (!r.read_val(data->max_range.begin)) return false;
    if (!r.read_val(data->max_range.end)) return false;

    // TIMELINE_BASIC
    if (!DeserializeTimelines(r, precomputed->basic_timelines)) return false;

    // TIMELINE_CATEGORIES
    if (!DeserializeTimelines(r, precomputed->category_timelines)) return false;

    // CATEGORY_MAP_NATIVE
    if (!DeserializeCategoryMap(r, precomputed->native_category_map)) return false;

    // CATEGORY_MAP_LUA
    if (!DeserializeCategoryMap(r, precomputed->lua_category_map)) return false;

    // SUBCATEGORY_MAPS
    {
        uint32_t num_categories;
        if (!r.read_val(num_categories)) return false;
        precomputed->native_subcategory_maps.resize(num_categories);
        for (uint32_t i = 0; i < num_categories; i++)
            if (!DeserializeCategoryMap(r, precomputed->native_subcategory_maps[i]))
                return false;
    }

    return true;
}

// ── IsCacheValid ────────────────────────────────────────────────────────────

bool CacheSerializer::IsCacheValid(const std::string& memtrace_path,
                                   const std::string& cache_path) {
    namespace fs = std::filesystem;
    if (!fs::exists(cache_path)) return false;

    std::ifstream file(cache_path, std::ios::binary);
    if (!file) return false;

    char magic[6];
    file.read(magic, 6);
    if (!file.good() || std::memcmp(magic, CACHE_MAGIC, 6) != 0) return false;

    uint32_t version;
    file.read(reinterpret_cast<char*>(&version), sizeof(version));
    if (!file.good() || version != CACHE_VERSION) return false;

    uint64_t stored_hash;
    file.read(reinterpret_cast<char*>(&stored_hash), sizeof(stored_hash));
    if (!file.good()) return false;

    return stored_hash == ComputeSourceHash(memtrace_path);
}

// ── ComputeSourceHash ───────────────────────────────────────────────────────

uint64_t CacheSerializer::ComputeSourceHash(const std::string& path) {
    namespace fs = std::filesystem;
    std::error_code ec;

    auto file_size = fs::file_size(path, ec);
    if (ec) return 0;

    auto mod_time = fs::last_write_time(path, ec);
    if (ec) return 0;

    uint64_t time_val = static_cast<uint64_t>(mod_time.time_since_epoch().count());
    uint64_t hash = file_size;
    hash ^= time_val + 0x9e3779b97f4a7c15ULL + (hash << 12) + (hash >> 4);
    return hash;
}
