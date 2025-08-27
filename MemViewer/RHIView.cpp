#include "RHIView.h"
#include "TraceParser.h"
#include "FrameParser.h"
#include "MemViewer.h"
#include "TraceInstance.h"
#include "Utils.h"
#include "imgui/imgui.h"
#include <filesystem>
#include "resource.h"

enum EPixelFormat
{
	PF_Unknown = 0,
	PF_A32B32G32R32F = 1,
	PF_B8G8R8A8 = 2,
	PF_G8 = 3,
	PF_G16 = 4,
	PF_DXT1 = 5,
	PF_DXT3 = 6,
	PF_DXT5 = 7,
	PF_UYVY = 8,
	PF_FloatRGB = 9,
	PF_FloatRGBA = 10,
	PF_DepthStencil = 11,
	PF_ShadowDepth = 12,
	PF_R32_FLOAT = 13,
	PF_G16R16 = 14,
	PF_G16R16F = 15,
	PF_G16R16F_FILTER = 16,
	PF_G32R32F = 17,
	PF_A2B10G10R10 = 18,
	PF_A16B16G16R16 = 19,
	PF_D24 = 20,
	PF_R16F = 21,
	PF_R16F_FILTER = 22,
	PF_BC5 = 23,
	PF_V8U8 = 24,
	PF_A1 = 25,
	PF_FloatR11G11B10 = 26,
	PF_A8 = 27,
	PF_R32_UINT = 28,
	PF_R32_SINT = 29,
	PF_PVRTC2 = 30,
	PF_PVRTC4 = 31,
	PF_R16_UINT = 32,
	PF_R16_SINT = 33,
	PF_R16G16B16A16_UINT = 34,
	PF_R16G16B16A16_SINT = 35,
	PF_R5G6B5_UNORM = 36,
	PF_R8G8B8A8 = 37,
	PF_A8R8G8B8 = 38,	// Only used for legacy loading; do NOT use!
	PF_BC4 = 39,
	PF_R8G8 = 40,
	PF_ATC_RGB = 41,	// Unsupported Format
	PF_ATC_RGBA_E = 42,	// Unsupported Format
	PF_ATC_RGBA_I = 43,	// Unsupported Format
	PF_X24_G8 = 44,	// Used for creating SRVs to alias a DepthStencil buffer to read Stencil. Don't use for creating textures.
	PF_ETC1 = 45,	// Unsupported Format
	PF_ETC2_RGB = 46,
	PF_ETC2_RGBA = 47,
	PF_R32G32B32A32_UINT = 48,
	PF_R16G16_UINT = 49,
	PF_ASTC_4x4 = 50,	// 8.00 bpp
	PF_ASTC_6x6 = 51,	// 3.56 bpp
	PF_ASTC_8x8 = 52,	// 2.00 bpp
	PF_ASTC_10x10 = 53,	// 1.28 bpp
	PF_ASTC_12x12 = 54,	// 0.89 bpp
	PF_BC6H = 55,
	PF_BC7 = 56,
	PF_R8_UINT = 57,
	PF_L8 = 58,
	PF_XGXR8 = 59,
	PF_R8G8B8A8_UINT = 60,
	PF_R8G8B8A8_SNORM = 61,
	PF_R16G16B16A16_UNORM = 62,
	PF_R16G16B16A16_SNORM = 63,
	PF_PLATFORM_HDR_0 = 64,
	PF_PLATFORM_HDR_1 = 65,	// Reserved.
	PF_PLATFORM_HDR_2 = 66,	// Reserved.
	PF_NV12 = 67,
	PF_R32G32_UINT = 68,
	PF_ETC2_R11_EAC = 69,
	PF_ETC2_RG11_EAC = 70,
	PF_R8 = 71,
	PF_StencilOnly = 72,
	PF_D16 = 73,
	PF_MAX = 74,
};

struct FPixelFormatInfo
{
	std::string Name;
	int32_t			BlockSizeX,
		BlockSizeY,
		BlockSizeZ,
		BlockBytes,
		NumComponents;
	/** Platform specific token, e.g. D3DFORMAT with D3DDrv										*/
	uint32_t			PlatformFormat;
	/** Whether the texture format is supported on the current platform/ rendering combination	*/
	bool			Supported;
	EPixelFormat	UnrealFormat;
};

#define TEXT(x) x

const FPixelFormatInfo	GPixelFormats[PF_MAX] =
{
	// Name						BlockSizeX	BlockSizeY	BlockSizeZ	BlockBytes	NumComponents	PlatformFormat	Supported		UnrealFormat

	{ TEXT("unknown"),			0,			0,			0,			0,			0,				0,				0,				PF_Unknown			},
	{ TEXT("A32B32G32R32F"),	1,			1,			1,			16,			4,				0,				1,				PF_A32B32G32R32F	},
	{ TEXT("B8G8R8A8"),			1,			1,			1,			4,			4,				0,				1,				PF_B8G8R8A8			},
	{ TEXT("G8"),				1,			1,			1,			1,			1,				0,				1,				PF_G8				},
	{ TEXT("G16"),				1,			1,			1,			2,			1,				0,				1,				PF_G16				},
	{ TEXT("DXT1"),				4,			4,			1,			8,			3,				0,				1,				PF_DXT1				},
	{ TEXT("DXT3"),				4,			4,			1,			16,			4,				0,				1,				PF_DXT3				},
	{ TEXT("DXT5"),				4,			4,			1,			16,			4,				0,				1,				PF_DXT5				},
	{ TEXT("UYVY"),				2,			1,			1,			4,			4,				0,				0,				PF_UYVY				},
	{ TEXT("FloatRGB"),			1,			1,			1,			4,			3,				0,				1,				PF_FloatRGB			},
	{ TEXT("FloatRGBA"),		1,			1,			1,			8,			4,				0,				1,				PF_FloatRGBA		},
	{ TEXT("DepthStencil"),		1,			1,			1,			4,			1,				0,				0,				PF_DepthStencil		},
	{ TEXT("ShadowDepth"),		1,			1,			1,			4,			1,				0,				0,				PF_ShadowDepth		},
	{ TEXT("R32_FLOAT"),		1,			1,			1,			4,			1,				0,				1,				PF_R32_FLOAT		},
	{ TEXT("G16R16"),			1,			1,			1,			4,			2,				0,				1,				PF_G16R16			},
	{ TEXT("G16R16F"),			1,			1,			1,			4,			2,				0,				1,				PF_G16R16F			},
	{ TEXT("G16R16F_FILTER"),	1,			1,			1,			4,			2,				0,				1,				PF_G16R16F_FILTER	},
	{ TEXT("G32R32F"),			1,			1,			1,			8,			2,				0,				1,				PF_G32R32F			},
	{ TEXT("A2B10G10R10"),      1,          1,          1,          4,          4,              0,              1,				PF_A2B10G10R10		},
	{ TEXT("A16B16G16R16"),		1,			1,			1,			8,			4,				0,				1,				PF_A16B16G16R16		},
	{ TEXT("D24"),				1,			1,			1,			4,			1,				0,				1,				PF_D24				},
	{ TEXT("PF_R16F"),			1,			1,			1,			2,			1,				0,				1,				PF_R16F				},
	{ TEXT("PF_R16F_FILTER"),	1,			1,			1,			2,			1,				0,				1,				PF_R16F_FILTER		},
	{ TEXT("BC5"),				4,			4,			1,			16,			2,				0,				1,				PF_BC5				},
	{ TEXT("V8U8"),				1,			1,			1,			2,			2,				0,				1,				PF_V8U8				},
	{ TEXT("A1"),				1,			1,			1,			1,			1,				0,				0,				PF_A1				},
	{ TEXT("FloatR11G11B10"),	1,			1,			1,			4,			3,				0,				0,				PF_FloatR11G11B10	},
	{ TEXT("A8"),				1,			1,			1,			1,			1,				0,				1,				PF_A8				},
	{ TEXT("R32_UINT"),			1,			1,			1,			4,			1,				0,				1,				PF_R32_UINT			},
	{ TEXT("R32_SINT"),			1,			1,			1,			4,			1,				0,				1,				PF_R32_SINT			},

	// IOS Support
	{ TEXT("PVRTC2"),			8,			4,			1,			8,			4,				0,				0,				PF_PVRTC2			},
	{ TEXT("PVRTC4"),			4,			4,			1,			8,			4,				0,				0,				PF_PVRTC4			},

	{ TEXT("R16_UINT"),			1,			1,			1,			2,			1,				0,				1,				PF_R16_UINT			},
	{ TEXT("R16_SINT"),			1,			1,			1,			2,			1,				0,				1,				PF_R16_SINT			},
	{ TEXT("R16G16B16A16_UINT"),1,			1,			1,			8,			4,				0,				1,				PF_R16G16B16A16_UINT},
	{ TEXT("R16G16B16A16_SINT"),1,			1,			1,			8,			4,				0,				1,				PF_R16G16B16A16_SINT},
	{ TEXT("R5G6B5_UNORM"),     1,          1,          1,          2,          3,              0,              1,              PF_R5G6B5_UNORM		},
	{ TEXT("R8G8B8A8"),			1,			1,			1,			4,			4,				0,				1,				PF_R8G8B8A8			},
	{ TEXT("A8R8G8B8"),			1,			1,			1,			4,			4,				0,				1,				PF_A8R8G8B8			},
	{ TEXT("BC4"),				4,			4,			1,			8,			1,				0,				1,				PF_BC4				},
	{ TEXT("R8G8"),				1,			1,			1,			2,			2,				0,				1,				PF_R8G8				},

	{ TEXT("ATC_RGB"),			4,			4,			1,			8,			3,				0,				0,				PF_ATC_RGB			},
	{ TEXT("ATC_RGBA_E"),		4,			4,			1,			16,			4,				0,				0,				PF_ATC_RGBA_E		},
	{ TEXT("ATC_RGBA_I"),		4,			4,			1,			16,			4,				0,				0,				PF_ATC_RGBA_I		},
	{ TEXT("X24_G8"),			1,			1,			1,			1,			1,				0,				0,				PF_X24_G8			},
	{ TEXT("ETC1"),				4,			4,			1,			8,			3,				0,				0,				PF_ETC1				},
	{ TEXT("ETC2_RGB"),			4,			4,			1,			8,			3,				0,				0,				PF_ETC2_RGB			},
	{ TEXT("ETC2_RGBA"),		4,			4,			1,			16,			4,				0,				0,				PF_ETC2_RGBA		},
	{ TEXT("PF_R32G32B32A32_UINT"),1,		1,			1,			16,			4,				0,				1,				PF_R32G32B32A32_UINT},
	{ TEXT("PF_R16G16_UINT"),	1,			1,			1,			4,			4,				0,				1,				PF_R16G16_UINT},

	// ASTC support
	{ TEXT("ASTC_4x4"),			4,			4,			1,			16,			4,				0,				0,				PF_ASTC_4x4			},
	{ TEXT("ASTC_6x6"),			6,			6,			1,			16,			4,				0,				0,				PF_ASTC_6x6			},
	{ TEXT("ASTC_8x8"),			8,			8,			1,			16,			4,				0,				0,				PF_ASTC_8x8			},
	{ TEXT("ASTC_10x10"),		10,			10,			1,			16,			4,				0,				0,				PF_ASTC_10x10		},
	{ TEXT("ASTC_12x12"),		12,			12,			1,			16,			4,				0,				0,				PF_ASTC_12x12		},

	{ TEXT("BC6H"),				4,			4,			1,			16,			3,				0,				1,				PF_BC6H				},
	{ TEXT("BC7"),				4,			4,			1,			16,			4,				0,				1,				PF_BC7				},
	{ TEXT("R8_UINT"),			1,			1,			1,			1,			1,				0,				1,				PF_R8_UINT			},
	{ TEXT("L8"),				1,			1,			1,			1,			1,				0,				0,				PF_L8				},
	{ TEXT("XGXR8"),			1,			1,			1,			4,			4,				0,				1,				PF_XGXR8 			},
	{ TEXT("R8G8B8A8_UINT"),	1,			1,			1,			4,			4,				0,				1,				PF_R8G8B8A8_UINT	},
	{ TEXT("R8G8B8A8_SNORM"),	1,			1,			1,			4,			4,				0,				1,				PF_R8G8B8A8_SNORM	},

	{ TEXT("R16G16B16A16_UINT"),1,			1,			1,			8,			4,				0,				1,				PF_R16G16B16A16_UNORM },
	{ TEXT("R16G16B16A16_SINT"),1,			1,			1,			8,			4,				0,				1,				PF_R16G16B16A16_SNORM },
	{ TEXT("PLATFORM_HDR_0"),	0,			0,			0,			0,			0,				0,				0,				PF_PLATFORM_HDR_0   },
	{ TEXT("PLATFORM_HDR_1"),	0,			0,			0,			0,			0,				0,				0,				PF_PLATFORM_HDR_1   },
	{ TEXT("PLATFORM_HDR_2"),	0,			0,			0,			0,			0,				0,				0,				PF_PLATFORM_HDR_2   },

	// NV12 contains 2 textures: R8 luminance plane followed by R8G8 1/4 size chrominance plane.
	// BlockSize/BlockBytes/NumComponents values don't make much sense for this format, so set them all to one.
	{ TEXT("NV12"),				1,			1,			1,			1,			1,				0,				0,				PF_NV12             },

	{ TEXT("PF_R32G32_UINT"),   1,   		1,			1,			8,			2,				0,				1,				PF_R32G32_UINT      },

	{ TEXT("PF_ETC2_R11_EAC"),  4,   		4,			1,			8,			1,				0,				0,				PF_ETC2_R11_EAC     },
	{ TEXT("PF_ETC2_RG11_EAC"), 4,   		4,			1,			16,			2,				0,				0,				PF_ETC2_RG11_EAC    },
	{ TEXT("R8"),				1,			1,			1,			1,			1,				0,				1,				PF_R8				},
	{ TEXT("S8"),				1,			1,			1,			1,			1,				0,				1,				PF_StencilOnly		},
	{ TEXT("D16"),				1,			1,			1,			2,			1,				0,				1,				PF_D16				},

};



const std::string Categories[RhiInfo::Max] = {
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

static const ImGuiTableFlags tbl_flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_Sortable;

static uint32_t calTextureSize(uint32_t type, uint32_t format, uint32_t mips, uint32_t w, uint32_t h, uint32_t d)
{
	uint32_t pixel_size = 0;

	uint32_t size = 0;
	switch (type)
	{
	case RhiInfo::TextureCube:
		size = w * w * 6;
	case RhiInfo::TextureCubeArray:
		size = w * w * 6 * h;
	default:
		size = w * h * d;
		break;
	}

	auto& info = GPixelFormats[format];
	size = size * info.BlockBytes / info.BlockSizeY / info.BlockSizeX / info.BlockSizeZ;

	return size;
}


void RHIView::InitializeImpl()
{
	selected_rhi_res = 0;
}
void RHIView::UpdateImpl()
{
	//auto range = GetTrace()->getSelectRange();

	//auto begin_frame = std::min(range.begin, range.end);
	//auto end_frame = std::max(range.begin, range.end);


	auto oldselect = 0;
	if (types.size() > 0)
		oldselect = types[selected_type].type;
	selected_type = 0;
	types.clear();
	types.resize(Categories->size());
	int idx = 0;

	CategoryParser parser;
	auto grps = parser(get_or_create_default_file(IDR_RHI_CONFIG_INI, "rhi_config.ini"));
	for (auto& type : types)
	{
		type.name = Categories[idx];
		for (auto& grp : grps[idx].subs)
		{
			type.infos.push_back({grp.name});
		}
		type.type = idx++;
		
	}




	auto find_cat = [&](const std::string& name, int type)->int
	{
		auto& grp = grps[type];
		int count = grp.subs.size() - 1;
		int i = 0;
		for (; i < count; ++i)
		{
			for (auto& key : grp.subs[i].keys)
			{
				if (name.find(key) != std::string::npos)
				{
					return i;
				}
			}
		}
		return i ;
	};

	std::unordered_map<int,std::unordered_map<int,std::unordered_map<std::string, int>>> rhi_maps;
	for (auto& rhi : GetTrace()->getRHIs())
	{
		//if ( rhi.begin > end_frame || rhi.end < begin_frame || (rhi.begin >= begin_frame && rhi.end <= end_frame) )
		//	continue;


		auto& type = types[rhi.type];
		auto subtype  = find_cat(rhi.name, rhi.type);
		
		if (!rhi_maps[rhi.type][subtype].contains(rhi.name))
		{
			type.infos[subtype].rhis.emplace_back(rhi.name);
			rhi_maps[rhi.type][subtype][rhi.name] = type.infos[subtype].rhis.size() - 1;
		}
		
		auto& infos = type.infos[subtype].rhis[rhi_maps[rhi.type][subtype][rhi.name]];

		auto rhicpy = rhi;


		auto count = rhi.size > 0 ? 1 : -1;
		type.count += count;
		type.size += rhi.size;

		type.infos[subtype].count+= count;
		type.infos[subtype].size += rhi.size;

		infos.size += rhi.size;
		infos.count += count;

		infos.rhis.push_back(std::move(rhicpy));

	}

	auto cmp = [](auto& a, auto& b, bool l) {
		if (l)
			return std::less()(a, b);
		else
			return std::greater()(a, b);
		};

	auto cmp2 = [cmp](int col, int dir, auto& a, auto& b) {
		switch (col)
		{
		case 0: return cmp(a.name, b.name, dir);
		case 1: return cmp(a.size, b.size, dir);
		case 2: return cmp(a.count, b.count, dir);
		}

		return false;
		};

	auto header = [=]() { return std::vector<TableList::TableDescriptor::Header>{{"name", ImGuiTableColumnFlags_NoHide}, { "size",ImGuiTableColumnFlags_WidthFixed, 150 }, { "count",ImGuiTableColumnFlags_WidthFixed, 100 }}; };
	tables.Init({
		// category
		{
			header,
			[grps, this](){
				std::vector<TableList::TableDescriptor::item> items;
				int idx = 0;
				for (auto& grp : types)
				{
					items.emplace_back(idx++, std::vector<std::string>{grp.name, size_tostring(grp.size), std::to_string( grp.count)});
				}
				return items;
			},
			[&](int col, int dir) {
				std::stable_sort(types.begin(), types.end(), [&](auto& a, auto& b) {
					return cmp2(col, dir, a, b);
				});
			},
			[this](int selected){
				selected_type = selected;

				const auto& name = types[selected_type].name;

				int idx = 0;
				for (auto& cate : Categories)
				{
					if (name == cate)
					{
						selected_rhi_type = idx;
					}
					idx++;
				}
			}
		},
		// subcategory
		{
			header,
			[grps, this]() {
				auto selected = selected_type;
				std::vector<TableList::TableDescriptor::item> items;
				int idx = 0;
				for (auto& info : types[selected].infos)
				{
					items.emplace_back(idx++, std::vector<std::string>{info.name, size_tostring(info.size), std::to_string(info.count)});
				}
				return items;
			},
			[&](int col, int dir) {
				auto selected = tables.tables[0].selected;
				auto& infos = types[selected].infos;
				std::stable_sort(infos.begin(), infos.end(), [&](auto& a, auto& b) {
					return cmp2(col, dir, a, b);
				});
			},
			[this](int selected) {
				selected_subtype = selected;
			}
		},
		// rhigroup
		{
			header,
			[grps, this]() {
				show_size = 0;
				auto type = selected_type;
				auto subtype = selected_subtype;
				std::vector<TableList::TableDescriptor::item> items;
				auto key = to_lower(name_filter.GetString());
				int idx = 0;
				for (auto& info : types[type].infos[subtype].rhis)
				{


					bool skip = !key.empty() && to_lower(info.name).find(key) == std::string::npos;

					
					if (!skip && selected_rhi_type < 5)
					{
						skip = true;
						for (auto& rhi : info.rhis)
						{
							auto fmt = rhi.attrs[RhiInfo::Format];
							bool cond = !selected_texture_format || (fmt == selected_texture_format);
							//if (inverse_filter)
								//cond = !cond;
							cond ^= inverse_filter;
							if (cond)
							{

								auto is_rt = rhi.attrs[RhiInfo::RenderTarget] != 0;
								if (!selected_texture_type || (is_rt == (selected_texture_type == 2)))
								{
									skip = false;
									break;
								}
							}

						
						}
					}
				

					if (!skip && (info.count != 0 || info.size != 0))
					{
						show_size += info.size;
						items.emplace_back(idx, std::vector<std::string>{info.name, size_tostring(info.size), std::to_string(info.count)});
					}	
					idx++;
				}
				return items;
			},
			[&](int col, int dir) {
				auto type = tables.tables[0].selected;
				auto subtype = tables.tables[1].selected;
				auto& rhis = types[type].infos[subtype].rhis;

				std::stable_sort(rhis.begin(), rhis.end(), [&](auto& a, auto& b) {
					return cmp2(col, dir, a, b);
				});
			},
			[this](int selected) {
				selected_rhi = selected;

			},
			[&](auto& tbl){



				if (name_filter.Show())
				{
					tbl.Refresh();
				}

				if (selected_rhi_type < 5)
				{
					const char* type_filters[] = { "All", "Normal","RenderTarget" };
					if (ImGui::Combo("type", &selected_texture_type, type_filters, sizeof(type_filters) / sizeof(const char*)))
					{
						tbl.Refresh();
					}

					if (ImGui::Combo("format", &selected_texture_format, +[](void*, int idx) { return GPixelFormats[idx].Name.c_str(); }, 0, PF_MAX))
					{
						tbl.Refresh();

					}

					if (ImGui::Checkbox("inverse", &inverse_filter))
					{
						tbl.Refresh();
					}
				}

				ImGui::Text("Total: %s", size_tostring(show_size).c_str());
			}
		},
		// rhi
		{
			[=]() { return std::vector<TableList::TableDescriptor::Header>{{"name", ImGuiTableColumnFlags_NoHide}, { "size",ImGuiTableColumnFlags_WidthFixed, 150 }}; },
			[grps, this]() {
				auto type = selected_type;
				auto subtype = selected_subtype;
				auto rhi = selected_rhi;

				auto& rhis = types[type].infos[subtype].rhis;

				std::vector<TableList::TableDescriptor::item> items;
				if (rhis.size() > 0)
				{
					int idx = 0;
					for (auto& info : rhis[rhi].rhis)
					{
						items.emplace_back(idx++, std::vector<std::string>{std::format("{:#x}",info.ptr), size_tostring(info.size)});
					}
				}
				return items;
			},
			[&](int col, int dir) {
				auto type = tables.tables[0].selected;
				auto subtype = tables.tables[1].selected;
				auto rhi = selected_rhi;
				auto& rhis = types[type].infos[subtype].rhis;

				if (rhis.size() > 0)
				{
					std::stable_sort(rhis[rhi].rhis.begin(), rhis[rhi].rhis.end(), [&](auto& a, auto& b) {
						switch (col)
						{
						case 0: return cmp(a.name, b.name, dir);
						case 1: return cmp(a.size, b.size, dir);
						}
						return false;
					});
				}
			},
			[this](int selected) {
				auto type = tables.tables[0].selected;
				auto subtype = tables.tables[1].selected;
				auto rhi = selected_rhi;
				auto& rhis = types[type].infos[subtype].rhis;

				if (rhis.size() > 0)
				{
					selected_rhi_res = &rhis[rhi].rhis[selected];
					//std::stable_sort(rhis[rhi].rhis.begin(), rhis[rhi].rhis.end(), [&](auto& a, auto& b) {
					//	return cmp2(col, dir, a, b);
					//});
				}
				else
					selected_rhi_res = 0;
			}
		},
	});

	tables.Refresh();

	idx = 0;
	for (auto& type : types)
	{
		if (oldselect == type.type)
		{
			selected_type = idx;
			break;
		}
		idx++;
	}


	selected_rhi_res = 0;
}


void RHIView::MakeInfos()
{
	//auto& rhis = types[selected_type].infos;
	//if (rhis.empty())
	//	return;
	//auto& type = types[selected_type].type;
	//auto& rhi = rhis[selected_rhi];

	if (selected_rhi_res == 0)
		return;
	auto& rhi = *selected_rhi_res;

	auto make_item = [&](const std::string & key, auto& value){
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Selectable(key.c_str());
		ImGui::TableNextColumn();
		ImGui::Text(std::format("{}", value).c_str());
	};

	ImGui::TableNextRow();
	ImGui::TableNextColumn();
	ImGui::Selectable("ptr");
	ImGui::TableNextColumn();
	ImGui::Text(std::format("{:#x}", rhi.ptr).c_str());

	make_item("begin",rhi.begin);
	make_item("end", rhi.end);
	make_item("size", rhi.size);
	//if (type < RhiInfo::Vertex)
	if (rhi.attrs.size() > 0)
	{
		make_item("width", rhi.attrs[RhiInfo::SizeX]);
		make_item("height", rhi.attrs[RhiInfo::SizeY]);
		make_item("depth", rhi.attrs[RhiInfo::SizeZ]);
		make_item("format", GPixelFormats[rhi.attrs[RhiInfo::Format]].Name);
		make_item("is rendertarget", (rhi.attrs[RhiInfo::RenderTarget]));
	}

}



void RHIView::ShowImpl()
{
	if (ImGui::BeginTable("tables", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit))
	{
		ImGui::TableSetupColumn("RHI", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Info", ImGuiTableColumnFlags_WidthFixed,300);

		ImGui::TableHeadersRow();
		ImGui::TableNextRow();


		ImGui::TableSetColumnIndex(0);




		tables.Show();
		//categories.Show();

		ImGui::TableSetColumnIndex(1);

		//ImGui::BeginChild("info", ImVec2(500, 0), 0);
		if (ImGui::BeginTable("info", 2, tbl_flags))
		{

			ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthFixed, 100);
			ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 200);
			ImGui::TableHeadersRow();


			if (types.size() > 0)
			{
				MakeInfos();
			}

			ImGui::EndTable();
		}
		//ImGui::EndChild();
		ImGui::EndTable();
	}
}
