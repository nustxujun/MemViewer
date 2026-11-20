#include "RHIAnalyzer.h"
#include "CategoryAnalyzer.h"
#include "Utils.h"
#include <algorithm>
#include <unordered_map>
#include <format>

const std::string RHIAnalyzer::TypeNames[9] = {
    "Texture2D",
    "TextureArray",
    "Texture3D",
    "TextureCube",
    "TextureCubeArray",
    "VertexBuffer",
    "IndexBuffer",
    "StructuredBuffer",
    "UniformBuffer"
};

const PixelFormatInfo RHIAnalyzer::PixelFormats[74] = {
    {"unknown",              0,  0,  0,  0},
    {"A32B32G32R32F",        1,  1,  1, 16},
    {"B8G8R8A8",             1,  1,  1,  4},
    {"G8",                   1,  1,  1,  1},
    {"G16",                  1,  1,  1,  2},
    {"DXT1",                 4,  4,  1,  8},
    {"DXT3",                 4,  4,  1, 16},
    {"DXT5",                 4,  4,  1, 16},
    {"UYVY",                 2,  1,  1,  4},
    {"FloatRGB",             1,  1,  1,  4},
    {"FloatRGBA",            1,  1,  1,  8},
    {"DepthStencil",         1,  1,  1,  4},
    {"ShadowDepth",          1,  1,  1,  4},
    {"R32_FLOAT",            1,  1,  1,  4},
    {"G16R16",               1,  1,  1,  4},
    {"G16R16F",              1,  1,  1,  4},
    {"G16R16F_FILTER",       1,  1,  1,  4},
    {"G32R32F",              1,  1,  1,  8},
    {"A2B10G10R10",          1,  1,  1,  4},
    {"A16B16G16R16",         1,  1,  1,  8},
    {"D24",                  1,  1,  1,  4},
    {"PF_R16F",              1,  1,  1,  2},
    {"PF_R16F_FILTER",       1,  1,  1,  2},
    {"BC5",                  4,  4,  1, 16},
    {"V8U8",                 1,  1,  1,  2},
    {"A1",                   1,  1,  1,  1},
    {"FloatR11G11B10",       1,  1,  1,  4},
    {"A8",                   1,  1,  1,  1},
    {"R32_UINT",             1,  1,  1,  4},
    {"R32_SINT",             1,  1,  1,  4},
    {"PVRTC2",               8,  4,  1,  8},
    {"PVRTC4",               4,  4,  1,  8},
    {"R16_UINT",             1,  1,  1,  2},
    {"R16_SINT",             1,  1,  1,  2},
    {"R16G16B16A16_UINT",    1,  1,  1,  8},
    {"R16G16B16A16_SINT",    1,  1,  1,  8},
    {"R5G6B5_UNORM",         1,  1,  1,  2},
    {"R8G8B8A8",             1,  1,  1,  4},
    {"A8R8G8B8",             1,  1,  1,  4},
    {"BC4",                  4,  4,  1,  8},
    {"R8G8",                 1,  1,  1,  2},
    {"ATC_RGB",              4,  4,  1,  8},
    {"ATC_RGBA_E",           4,  4,  1, 16},
    {"ATC_RGBA_I",           4,  4,  1, 16},
    {"X24_G8",               1,  1,  1,  1},
    {"ETC1",                 4,  4,  1,  8},
    {"ETC2_RGB",             4,  4,  1,  8},
    {"ETC2_RGBA",            4,  4,  1, 16},
    {"PF_R32G32B32A32_UINT", 1,  1,  1, 16},
    {"PF_R16G16_UINT",       1,  1,  1,  4},
    {"ASTC_4x4",             4,  4,  1, 16},
    {"ASTC_6x6",             6,  6,  1, 16},
    {"ASTC_8x8",             8,  8,  1, 16},
    {"ASTC_10x10",          10, 10,  1, 16},
    {"ASTC_12x12",          12, 12,  1, 16},
    {"BC6H",                 4,  4,  1, 16},
    {"BC7",                  4,  4,  1, 16},
    {"R8_UINT",              1,  1,  1,  1},
    {"L8",                   1,  1,  1,  1},
    {"XGXR8",                1,  1,  1,  4},
    {"R8G8B8A8_UINT",        1,  1,  1,  4},
    {"R8G8B8A8_SNORM",       1,  1,  1,  4},
    {"R16G16B16A16_UNORM",   1,  1,  1,  8},
    {"R16G16B16A16_SNORM",   1,  1,  1,  8},
    {"PLATFORM_HDR_0",       0,  0,  0,  0},
    {"PLATFORM_HDR_1",       0,  0,  0,  0},
    {"PLATFORM_HDR_2",       0,  0,  0,  0},
    {"NV12",                 1,  1,  1,  1},
    {"PF_R32G32_UINT",       1,  1,  1,  8},
    {"PF_ETC2_R11_EAC",      4,  4,  1,  8},
    {"PF_ETC2_RG11_EAC",     4,  4,  1, 16},
    {"R8",                   1,  1,  1,  1},
    {"S8",                   1,  1,  1,  1},
    {"D16",                  1,  1,  1,  2},
};

std::vector<RHITypeInfo> RHIAnalyzer::Analyze(
    const std::vector<RhiInfo>& rhis,
    const std::string& config_path)
{
    std::vector<RHITypeInfo> types(RhiInfo::Max);
    for (int i = 0; i < RhiInfo::Max; ++i)
    {
        types[i].type = i;
        types[i].name = TypeNames[i];
    }

    std::vector<CategoryConfig::Category> config;
    if (!config_path.empty())
        config = CategoryAnalyzer::ParseConfig(config_path);

    for (int i = 0; i < RhiInfo::Max; ++i)
    {
        if (i < (int)config.size())
        {
            for (auto& sub : config[i].subs)
                types[i].sub_types.push_back({sub.name});
        }
        else
        {
            types[i].sub_types.push_back({"Untagged"});
        }
    }

    auto find_sub = [&](const std::string& name, int type_idx) -> int {
        if (type_idx >= (int)config.size())
            return 0;

        auto& cat = config[type_idx];
        int last = (int)cat.subs.size() - 1;
        for (int i = 0; i < last; ++i)
        {
            for (auto& key : cat.subs[i].keys)
            {
                if (name.find(key) != std::string::npos)
                    return i;
            }
        }
        return last;
    };

    // type -> sub_type -> group_name -> group_index
    std::unordered_map<int, std::unordered_map<int, std::unordered_map<std::string, int>>> rhi_maps;

    for (auto& rhi : rhis)
    {
        if (rhi.type >= RhiInfo::Max)
            continue;

        auto& type = types[rhi.type];
        int sub_idx = find_sub(rhi.name, rhi.type);

        if (sub_idx >= (int)type.sub_types.size())
            sub_idx = (int)type.sub_types.size() - 1;

        auto& sub = type.sub_types[sub_idx];

        if (!rhi_maps[rhi.type][sub_idx].contains(rhi.name))
        {
            sub.groups.emplace_back(RHIGroupInfo{rhi.name});
            rhi_maps[rhi.type][sub_idx][rhi.name] = (int)sub.groups.size() - 1;
        }

        auto grp_idx = rhi_maps[rhi.type][sub_idx][rhi.name];
        auto& group = sub.groups[grp_idx];

        auto count_val = rhi.size > 0 ? 1 : -1;

        type.count += count_val;
        type.size += rhi.size;

        sub.count += count_val;
        sub.size += rhi.size;

        group.count += count_val;
        group.size += rhi.size;

        group.items.push_back(rhi);
    }

    return types;
}

std::vector<RHITypeInfo> RHIAnalyzer::GetTypeSummary(
    const std::vector<RhiInfo>& rhis)
{
    std::vector<RHITypeInfo> types(RhiInfo::Max);
    for (int i = 0; i < RhiInfo::Max; ++i)
    {
        types[i].type = i;
        types[i].name = TypeNames[i];
    }

    for (auto& rhi : rhis)
    {
        if (rhi.type >= RhiInfo::Max)
            continue;

        auto count_val = rhi.size > 0 ? 1 : -1;
        types[rhi.type].count += count_val;
        types[rhi.type].size += rhi.size;
    }

    return types;
}

std::vector<RHIGroupInfo> RHIAnalyzer::GetResources(
    const std::vector<RHITypeInfo>& analyzed,
    int type, int sub_type,
    const std::string& name_filter,
    int texture_type, int pixel_format,
    int limit, int offset)
{
    if (type < 0 || type >= (int)analyzed.size())
        return {};

    auto& type_info = analyzed[type];
    std::vector<RHIGroupInfo> result;

    auto filter_key = name_filter.empty() ? std::string{} : to_lower(name_filter);

    auto collect_from_sub = [&](const RHISubTypeInfo& sub) {
        for (auto& group : sub.groups)
        {
            if (!filter_key.empty() && to_lower(group.name).find(filter_key) == std::string::npos)
                continue;

            if (group.count == 0 && group.size == 0)
                continue;

            bool skip = false;
            if (type < 5 && (texture_type != 0 || pixel_format != 0))
            {
                skip = true;
                for (auto& rhi : group.items)
                {
                    if (rhi.attrs.empty())
                        continue;

                    bool fmt_match = !pixel_format || ((int)rhi.attrs[RhiInfo::Format] == pixel_format);
                    if (!fmt_match)
                        continue;

                    if (texture_type != 0)
                    {
                        bool is_rt = rhi.attrs[RhiInfo::RenderTarget] != 0;
                        if (is_rt != (texture_type == 2))
                            continue;
                    }

                    skip = false;
                    break;
                }
            }

            if (!skip)
                result.push_back(group);
        }
    };

    if (sub_type >= 0 && sub_type < (int)type_info.sub_types.size())
    {
        collect_from_sub(type_info.sub_types[sub_type]);
    }
    else
    {
        for (auto& sub : type_info.sub_types)
            collect_from_sub(sub);
    }

    if (offset >= (int)result.size())
        return {};

    auto begin = result.begin() + offset;
    auto end = (offset + limit <= (int)result.size()) ? begin + limit : result.end();
    return {begin, end};
}

std::string RHIAnalyzer::GetTextureDetail(const RhiInfo& rhi)
{
    if (rhi.attrs.size() <= RhiInfo::RenderTarget)
        return "";

    auto w = rhi.attrs[RhiInfo::SizeX];
    auto h = rhi.attrs[RhiInfo::SizeY];
    auto d = rhi.attrs[RhiInfo::SizeZ];
    auto fmt = rhi.attrs[RhiInfo::Format];
    auto is_rt = rhi.attrs[RhiInfo::RenderTarget] != 0;

    const auto& fmt_name = GetFormatName(fmt);

    return std::format("{}x{}x{} {} {}",
                       w, h, d, fmt_name,
                       is_rt ? "RT" : "");
}

const std::string& RHIAnalyzer::GetFormatName(int format)
{
    static const std::string unknown = "unknown";
    if (format < 0 || format >= 74)
        return unknown;
    return PixelFormats[format].name;
}
