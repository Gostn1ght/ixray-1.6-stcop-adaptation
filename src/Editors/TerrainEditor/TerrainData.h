#pragma once

// Terrain project: a regular height grid centered at the world origin plus
// per-vertex weights of up to four texture layers.
struct STerrainLayer
{
	xr_string Name;
	u32 PreviewColor = 0xff808080;	// ARGB, used by the viewport and the exported base texture
};

class CTerrainData
{
public:
	static constexpr u32 LayerCount = 4;
	static constexpr u32 MinResolution = 33;
	static constexpr u32 MaxResolution = 2049;

	xr_string Name = "new_terrain";
	u32 Resolution = 0;					// vertices per side
	float Size = 0.f;					// side length, meters
	xr_vector<float> Heights;			// meters, Resolution * Resolution, row = z
	xr_vector<u8> Weights;				// LayerCount bytes per vertex
	STerrainLayer Layers[LayerCount];
	xr_string Shader = "levels\\escape_grass";
	xr_string GameMtl = "materials\\earth";
	xr_string ThmTemplate = "terrain\\terrain_escape";

	CTerrainData();

	void Create(u32 resolution, float size, float height);
	bool IsValid() const { return Resolution >= 2 && Heights.size() == Resolution * Resolution; }
	float CellSize() const { return Size / float(Resolution - 1); }
	float HalfSize() const { return Size * 0.5f; }

	IC float& H(u32 x, u32 z) { return Heights[z * Resolution + x]; }
	IC float H(u32 x, u32 z) const { return Heights[z * Resolution + x]; }
	IC u8* W(u32 x, u32 z) { return &Weights[(z * Resolution + x) * LayerCount]; }
	IC const u8* W(u32 x, u32 z) const { return &Weights[(z * Resolution + x) * LayerCount]; }

	void VertexPos(u32 x, u32 z, Fvector& out) const;
	bool WorldToGrid(float wx, float wz, float& gx, float& gz) const;
	float HeightAt(float wx, float wz) const;
	void Normal(u32 x, u32 z, Fvector& out) const;
	u32 BlendedColor(u32 x, u32 z) const;
	void BlendedColorAt(float gx, float gz, float rgb[3]) const;
	void WeightsAt(float gx, float gz, float w[LayerCount]) const;

	bool RayPick(const Fvector& start, const Fvector& dir, float max_dist, Fvector& hit) const;
	void GetBox(Fbox& box) const;

	bool Save(const char* path) const;
	bool Load(const char* path);

	bool ExportObject(const char* path, const char* object_name, const char* texture, u32 step) const;
	bool ExportTextures(const char* base_dds, const char* mask_dds, u32 tex_size) const;
};

bool WriteDDS_A8R8G8B8(const char* path, u32 size, const xr_vector<u32>& argb);
