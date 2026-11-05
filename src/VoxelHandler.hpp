#pragma once
#include <algorithm>
#include <array>
#include <unordered_map>
#include <vector>

#include "utils.hpp"

namespace Chunking {
constexpr int CHUNK_DEPTH = 5;
constexpr int CHUNK_SIZE = 1 << CHUNK_DEPTH;
struct Voxel {
	uint8_t type;
};
constexpr Voxel NULL_VOXEL = Voxel{ 0 };
struct Chunk {
	Chunk(Vector3<int> chunkPos) : position(chunkPos) {}

	size_t index(Vector3<int> pos) const {
		SDL_assert(pos.x >= 0 && pos.x < CHUNK_SIZE);
		SDL_assert(pos.y >= 0 && pos.y < CHUNK_SIZE);
		SDL_assert(pos.z >= 0 && pos.z < CHUNK_SIZE);
		return pos.x + pos.y * CHUNK_SIZE + pos.z * CHUNK_SIZE * CHUNK_SIZE;
	}
	const Voxel& getVoxel(Vector3<int> pos) const { return voxels[index(pos)]; }
	Vector3<int> getPosition() const { return position; }
	void setVoxel(Vector3<int> pos, Voxel v) { voxels[index(pos)] = v; }

   private:
	Vector3<int> position;
	std::array<Voxel, CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE> voxels{};
};
}  // namespace Chunking
namespace OctreeHandler {
#define MAX_OCTREE_NODES 100

struct NodeData {
	uint32_t child_pointer;
	uint32_t masks;
};
struct VoxelData {
	uint32_t id;
	uint32_t padding;
};

union Node {
	NodeData node;
	VoxelData voxel;
	Node() : node{ 0, 0 } {}

	static Node makeNode(uint32_t child, uint8_t childMask, uint8_t leafMask) {
		Node n;
		n.node.child_pointer = child;
		n.node.masks = (uint32_t(childMask) << 8) | leafMask;
		return n;
	}
	static Node makeVoxel(uint32_t color) {
		Node n;
		n.voxel.id = color;
		return n;
	}
};
static_assert(sizeof(Node) == 8, "Node must be 64 bits");
struct OctreeHeader {
	Vector3f center;
	int half_len;

	uint32_t root_node;
	uint32_t padding[3];

	OctreeHeader() : center{ 0, 0, 0 }, half_len(2), root_node(0), padding{ 0, 0, 0 } {}
};
struct Octree {
	OctreeHeader header;
	std::vector<Node> nodes;

	void buildTestTree() {
		nodes.clear();
		header.root_node = 0;

		// [0]      root: all 8 octants exist, none are leaves
		// [1..8]   one internal node per octant (octant o lives at 1 + o)
		// [9..64]  voxels, 7 per octant (the 8th, the outer corner, is removed)
		nodes.push_back(Node::makeNode(1, 0xFF, 0x00));

		for (uint32_t o = 0; o < 8; o++) {
			// The outer corner of octant o is the child whose bits equal o's bits.
			uint8_t mask = 0xFF & ~(1u << o);
			nodes.push_back(Node::makeNode(9 + o * 7, mask, mask));
		}

		for (uint32_t o = 0; o < 8; o++) {
			for (uint32_t c = 0; c < 8; c++) {
				if (c == o) continue;  // skip the outer corner

				// Voxel coords in the 4x4x4 grid, same bit convention as the shader
				uint32_t x = (o & 1) * 2 + (c & 1);
				uint32_t y = ((o >> 1) & 1) * 2 + ((c >> 1) & 1);
				uint32_t z = ((o >> 2) & 1) * 2 + ((c >> 2) & 1);

				uint32_t color = 0xFF000000u | ((z * 85) << 16) | ((y * 85) << 8) | (x * 85);
				nodes.push_back(Node::makeVoxel(color));
			}
		}
		// nodes.size() == 65
	}
};
}  // namespace OctreeHandler

class World {
	static constexpr int WORLD_DEPTH = 8;
	static constexpr int WORLD_CHUNKS = 1 << (WORLD_DEPTH - Chunking::CHUNK_DEPTH);
	static constexpr int BIAS = WORLD_CHUNKS / 2;

   public:
	Chunking::Voxel getVoxel(Vector3<int> pos) const {
		auto [chunkPos, localPos] = split(pos);

		auto it = chunks.find(chunkPos);
		if (it == chunks.end()) return Chunking::NULL_VOXEL;

		return it->second.getVoxel(localPos);
	}
	void setVoxel(Vector3<int> pos, Chunking::Voxel v) {
		auto [chunkPos, localPos] = split(pos);

		auto it = chunks.find(chunkPos);
		if (it == chunks.end()) return;

		it->second.setVoxel(localPos, v);
	}

	Vector3<uint32_t> biasedChunkCoord(const Chunking::Chunk& c) {
		Vector3<int> p = c.getPosition();
		return { (uint32_t)(p.x + BIAS), (uint32_t)(p.y + BIAS), (uint32_t)(p.z + BIAS) };
	}
	struct Entry {
		uint64_t key;
		uint32_t type;
	};
	struct LevelEntry {
		uint64_t key;
		uint32_t type;
		uint32_t firstChild;
		uint8_t childMask;
		uint8_t leafMask;
	};
	void buildChunk(const Chunking::Chunk& chunk, std::vector<Entry>& out) {
		const uint64_t chunkBits = morton(biasedChunkCoord(chunk)) << (3 * Chunking::CHUNK_DEPTH);
		const uint32_t total = Chunking::CHUNK_SIZE * Chunking::CHUNK_SIZE * Chunking::CHUNK_SIZE;

		for (uint32_t m = 0; m < total; ++m) {
			Vector3<uint32_t> p = decode(m);
			Chunking::Voxel v = chunk.getVoxel({ (int)p.x, (int)p.y, (int)p.z });
			if (v.type == Chunking::NULL_VOXEL.type) continue;

			out.push_back({ chunkBits | m, v.type });
		}
	}
	std::vector<Entry> buildWorld() {
		std::vector<const Chunking::Chunk*> order;
		order.reserve(chunks.size());

		for (const auto& [coord, chunk] : chunks) order.push_back(&chunk);
		std::sort(order.begin(), order.end(), [&](const Chunking::Chunk* a, const Chunking::Chunk* b) {
			return morton(biasedChunkCoord(*a)) < morton(biasedChunkCoord(*b));
		});

		std::vector<Entry> out;
		for (const Chunking::Chunk* c : order) buildChunk(*c, out);

		return out;
	}
	std::vector<OctreeHandler::Node> buildNodes(const std::vector<Entry>& entries, int worldDepth) {
		using OctreeHandler::Node;
		std::vector<Node> nodes;
		if (entries.empty()) return nodes;

		std::vector<std::vector<LevelEntry>> levels(worldDepth + 1);

		levels[worldDepth].reserve(entries.size());
		for (const auto& e : entries) {
			LevelEntry le;
			le.key = e.key;
			le.type = e.type;
			levels[worldDepth].push_back(le);
		}

		for (int l = worldDepth; l >= 1; --l) {
			const auto& below = levels[l];
			auto& above = levels[l - 1];

			size_t i = 0;
			while (i < below.size()) {
				LevelEntry parent;
				parent.key = below[i].key >> 3;
				parent.firstChild = (uint32_t)i;

				while (i < below.size() && below[i].key >> 3 == parent.key) {
					uint8_t oct = below[i].key & 7;
					parent.childMask |= 1u << oct;
					if (l == worldDepth) parent.leafMask |= 1u << oct;
					++i;
				}
				above.push_back(parent);
			}
		}
		SDL_assert(levels[0].size() == 1);

		std::vector<uint32_t> base(worldDepth + 2, 0);
		for (int l = 0; l <= worldDepth; ++l) base[l + 1] = base[l] + (uint32_t)levels[l].size();
		nodes.reserve(base[worldDepth + 1]);

		for (int l = 0; l < worldDepth; ++l) {
			for (const LevelEntry& e : levels[l]) {
				if (l == worldDepth)
					nodes.push_back(Node::makeVoxel(e.type));
				else
					nodes.push_back(Node::makeNode(base[l + 1] + e.firstChild, e.childMask, e.leafMask));
			}
		}
		return nodes;
	}

	OctreeHandler::Octree buildOctree() {
		OctreeHandler::Octree octree;
		octree.header.half_len = (1 << WORLD_DEPTH) / 2;
		octree.header.center = { 0, 0, 0 };
		octree.header.root_node = 0;
		octree.nodes = buildNodes(buildWorld(), WORLD_DEPTH);
	}

   private:
	int renderDist = 8;
	std::unordered_map<Vector3<int>, Chunking::Chunk> chunks;

	static std::tuple<Vector3<int>, Vector3<int>> split(Vector3<int> pos) {
		Vector3<int> chunkPos = { pos.x >> Chunking::CHUNK_DEPTH, pos.y >> Chunking::CHUNK_DEPTH,
								  pos.z >> Chunking::CHUNK_DEPTH };
		Vector3<int> localPos = { pos.x & (Chunking::CHUNK_SIZE - 1), pos.y & (Chunking::CHUNK_SIZE - 1),
								  pos.z & (Chunking::CHUNK_SIZE - 1) };

		return { chunkPos, localPos };
	}
	uint64_t morton(Vector3<uint32_t> pos) {
		uint64_t code = 0;
		for (int i = 0; i < 21; i++) {
			code |= (uint64_t)((pos.x >> i) & 1) << (3 * i + 0);
			code |= (uint64_t)((pos.y >> i) & 1) << (3 * i + 1);
			code |= (uint64_t)((pos.z >> i) & 1) << (3 * i + 2);
		}
		return code;
	}
	Vector3<uint32_t> decode(uint64_t code) {
		Vector3<uint32_t> pos = { 0, 0, 0 };
		for (int i = 0; i < 21; i++) {
			pos.x |= (uint32_t)((code >> (3 * i + 0)) & 1) << i;
			pos.y |= (uint32_t)((code >> (3 * i + 1)) & 1) << i;
			pos.z |= (uint32_t)((code >> (3 * i + 2)) & 1) << i;
		}
		return pos;
	}
};