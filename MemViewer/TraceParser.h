#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <map>
#include "Utils.h"


//#define Check(x) {if (!(x)) {*((int*)0) = 0;}}

struct Calltree;
struct Node;
struct NodeRef
{
	std::shared_ptr<Calltree> tree;
	int index = 0;


	Node* getNode()const;

	Node* operator->()const
	{
		return getNode();
	}

	Node* operator*()const
	{
		return getNode();
	}

	operator bool()const
	{
		return !!tree;
	}

	bool operator==(NodeRef other)const
	{
		return index == other.index && tree == other.tree;
	}

	void operator << (std::shared_ptr<Calltree> newtree)
	{
		tree = newtree;
	}

	NodeRef() = default;

	NodeRef(std::shared_ptr<Calltree> parent, int idx) : tree(parent), index(idx)
	{

	}

	NodeRef(std::shared_ptr<Calltree> parent, Node* node);



};
struct Node
{
	NodeRef parent ;
	int64_t basic_size = 0;
	int64_t size = 0;
	int32_t basic_count = 0;
	int32_t count = 0;
	std::string name;
	
	std::vector<NodeRef> children;

	bool is_matched = false;

};

struct Calltree
{
	using Ptr = std::shared_ptr<Calltree>;
	Node* root = 0;
	int node_count = 0;


	Calltree(int count): node_count(count)
	{
		root = (Node*) malloc(sizeof(Node) * count);
		new (root)Node();
		root->name = "root";
	}

	~Calltree()
	{
		for (int i = 0; i < node_count; ++i)
		{
			delete (root + i);
		}
	}

	Node* operator[](int idx) const
	{
		return get(idx);
	}

	Node* get(int idx)const
	{
		return root + idx;
	}

	int getIndex(Node* node)
	{
		Assert(node >= root && node < root + node_count, "invalid ptr");
		return int(node - root);
	}

	Ptr clone()const;
	static Ptr create(int count)
	{
		auto tree = std::make_shared<Calltree>(count);
		return tree;
	}

	void resetNodes()
	{
		for (int i = 0; i < node_count; ++i)
		{
			root[i].basic_count = 0;
			root[i].basic_size = 0;

			root[i].count = 0;
			root[i].size = 0;

		}
	}

	static Calltree::Ptr diff(Ptr tree1, Ptr tree2);
};



struct AllocInfo
{
	NodeRef node;

	int32_t size;
	uint32_t start;
	uint32_t end;

	uint32_t user_tag = 0;
};

struct TotalInfo
{
	uint64_t used;
	uint64_t available;
	int64_t overhead = 0;

	std::map<std::string, int64_t> custom_datas;
};

struct ObjClassInfo
{
	using Ptr = std::shared_ptr<ObjClassInfo>;
	std::vector<std::string> name_chain;
	int index = 0;
};


struct ObjectInfo
{
	std::string name;
	NodeRef node;
	ObjClassInfo::Ptr class_chain;
	uint32_t begin;
	uint32_t end;
	int state = 0;// 0 : normal, 1: add, -1 : delete
};


struct RhiInfo
{
	enum
	{
		SizeX ,
		SizeY,
		SizeZ,
		Format,
		Mips,
		RenderTarget
	};

	enum
	{
		Texture,
		TextureArray,
		Texture3D,
		TextureCube,
		TextureCubeArray,

		Vertex,
		Index,
		Struct,
		Uniform,


		Max
	};
	uint64_t ptr;
	std::string name;
	uint32_t begin;
	uint32_t end;
	uint32_t node_index;
	uint32_t type;
	int32_t size;
	std::vector<uint32_t> attrs;
};

struct Checkpoint
{
	std::string text;
	int frameid;
};


struct CustomData
{
	uint32_t type;

	std::string name;
	uint32_t size;
	uint32_t begin;
	uint32_t end;

	struct read
	{
		const char*& begin;
		read(const char*& input) :begin(input)
		{
		}


		template<class T>
		operator T()
		{
			T val;
			memcpy(&val, begin, sizeof(T));
			begin += sizeof(T);
			return val;
		}

		operator std::string()
		{
			std::string val;
			uint32_t len;
			len = read(begin);
			val.resize(len);
			memcpy(val.data(), begin, len);
			begin += len;
		}

	};


	void serialize(const char* buffer, uint32_t len)
	{
		auto end = buffer + len;
		int ver = read(buffer);
		serialize_impl(ver, buffer, len);
		Assert(buffer == end,"out of range");
	}

	virtual void serialize_impl(uint32_t version, const char* buffer, uint32_t len) {};

	using Ptr = std::shared_ptr<CustomData>;
	static Ptr createCustomData(int type);
};



struct ParsedData
{
	Node* root ;
	int node_count = 0;
	std::vector<AllocInfo> allocs;
	std::vector<TotalInfo> totals;
	std::vector<ObjectInfo> objects;
	std::vector< RhiInfo> rhis;
	std::vector< Checkpoint> checkpoints;
	std::vector<CustomData::Ptr> customs;
	uint32_t frame_begin;
	uint32_t frame_end;
};


extern NodeRef CloneTree(NodeRef Root);
