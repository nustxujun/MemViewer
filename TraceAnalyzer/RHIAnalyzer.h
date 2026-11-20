#pragma once
#include "CoreTypes.h"
#include <vector>
#include <string>

struct PixelFormatInfo {
    std::string name;
    int block_size_x, block_size_y, block_size_z;
    int block_bytes;
};

struct RHIGroupInfo {
    std::string name;
    int32_t count = 0;
    int64_t size = 0;
    std::vector<RhiInfo> items;
};

struct RHISubTypeInfo {
    std::string name;
    int32_t count = 0;
    int64_t size = 0;
    std::vector<RHIGroupInfo> groups;
};

struct RHITypeInfo {
    int type;
    std::string name;
    int32_t count = 0;
    int64_t size = 0;
    std::vector<RHISubTypeInfo> sub_types;
};

class RHIAnalyzer {
public:
    static const std::string TypeNames[9];
    static const PixelFormatInfo PixelFormats[74];

    static std::vector<RHITypeInfo>
    Analyze(const std::vector<RhiInfo>& rhis,
            const std::string& config_path = "");

    static std::vector<RHITypeInfo>
    GetTypeSummary(const std::vector<RhiInfo>& rhis);

    static std::vector<RHIGroupInfo>
    GetResources(const std::vector<RHITypeInfo>& analyzed,
                 int type, int sub_type = -1,
                 const std::string& name_filter = "",
                 int texture_type = 0, int pixel_format = 0,
                 int limit = 50, int offset = 0);

    static std::string GetTextureDetail(const RhiInfo& rhi);
    static const std::string& GetFormatName(int format);
};
