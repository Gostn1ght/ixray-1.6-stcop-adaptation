#include "stdafx.h"
#include "TerrainData.h"

namespace
{
	constexpr u32 TERRAIN_VERSION = 1;

	enum ETerrainChunk : u32
	{
		chVersion = 0x0001,
		chParams = 0x0002,
		chHeights = 0x0003,
		chWeights = 0x0004,
		chLayers = 0x0005,
	};

	IC float lerpf(float a, float b, float t) { return a + (b - a) * t; }
}

CTerrainData::CTerrainData()
{
	Layers[0] = { "grass",  0xff4f6b2e };
	Layers[1] = { "earth",  0xff6b5a3e };
	Layers[2] = { "asphalt", 0xff505050 };
	Layers[3] = { "sand",   0xffa08c64 };
}

void CTerrainData::Create(u32 resolution, float size, float height)
{
	Resolution = std::clamp(resolution, MinResolution, MaxResolution);
	Size = std::max(size, 1.f);
	Heights.assign(Resolution * Resolution, height);
	Weights.assign(Resolution * Resolution * LayerCount, 0);

	for (u32 i = 0; i < Resolution * Resolution; ++i)
		Weights[i * LayerCount] = 255;
}

void CTerrainData::VertexPos(u32 x, u32 z, Fvector& out) const
{
	const float cell = CellSize();
	out.set(-HalfSize() + x * cell, H(x, z), -HalfSize() + z * cell);
}

bool CTerrainData::WorldToGrid(float wx, float wz, float& gx, float& gz) const
{
	const float cell = CellSize();
	gx = (wx + HalfSize()) / cell;
	gz = (wz + HalfSize()) / cell;
	return gx >= 0.f && gz >= 0.f && gx <= float(Resolution - 1) && gz <= float(Resolution - 1);
}

float CTerrainData::HeightAt(float wx, float wz) const
{
	float gx, gz;
	WorldToGrid(wx, wz, gx, gz);
	gx = std::clamp(gx, 0.f, float(Resolution - 1));
	gz = std::clamp(gz, 0.f, float(Resolution - 1));

	const u32 x0 = std::min(u32(gx), Resolution - 2);
	const u32 z0 = std::min(u32(gz), Resolution - 2);
	const float fx = gx - x0;
	const float fz = gz - z0;

	const float h0 = lerpf(H(x0, z0), H(x0 + 1, z0), fx);
	const float h1 = lerpf(H(x0, z0 + 1), H(x0 + 1, z0 + 1), fx);
	return lerpf(h0, h1, fz);
}

void CTerrainData::Normal(u32 x, u32 z, Fvector& out) const
{
	const u32 xl = x > 0 ? x - 1 : x;
	const u32 xr = x + 1 < Resolution ? x + 1 : x;
	const u32 zd = z > 0 ? z - 1 : z;
	const u32 zu = z + 1 < Resolution ? z + 1 : z;

	const float cell = CellSize();
	const float dx = (H(xr, z) - H(xl, z)) / (float(xr - xl) * cell);
	const float dz = (H(x, zu) - H(x, zd)) / (float(zu - zd) * cell);

	out.set(-dx, 1.f, -dz);
	out.normalize();
}

void CTerrainData::WeightsAt(float gx, float gz, float w[LayerCount]) const
{
	gx = std::clamp(gx, 0.f, float(Resolution - 1));
	gz = std::clamp(gz, 0.f, float(Resolution - 1));

	const u32 x0 = std::min(u32(gx), Resolution - 2);
	const u32 z0 = std::min(u32(gz), Resolution - 2);
	const float fx = gx - x0;
	const float fz = gz - z0;

	const u8* w00 = W(x0, z0);
	const u8* w10 = W(x0 + 1, z0);
	const u8* w01 = W(x0, z0 + 1);
	const u8* w11 = W(x0 + 1, z0 + 1);

	for (u32 l = 0; l < LayerCount; ++l)
		w[l] = lerpf(lerpf(w00[l], w10[l], fx), lerpf(w01[l], w11[l], fx), fz) / 255.f;
}

void CTerrainData::BlendedColorAt(float gx, float gz, float rgb[3]) const
{
	float w[LayerCount];
	WeightsAt(gx, gz, w);

	float total = 0.f;
	rgb[0] = rgb[1] = rgb[2] = 0.f;
	for (u32 l = 0; l < LayerCount; ++l)
	{
		const u32 c = Layers[l].PreviewColor;
		rgb[0] += w[l] * color_get_R(c);
		rgb[1] += w[l] * color_get_G(c);
		rgb[2] += w[l] * color_get_B(c);
		total += w[l];
	}

	if (total > EPS)
	{
		rgb[0] /= total;
		rgb[1] /= total;
		rgb[2] /= total;
	}
}

u32 CTerrainData::BlendedColor(u32 x, u32 z) const
{
	float rgb[3];
	BlendedColorAt(float(x), float(z), rgb);
	return color_rgba(u32(rgb[0]), u32(rgb[1]), u32(rgb[2]), 255);
}

bool CTerrainData::RayPick(const Fvector& start, const Fvector& dir, float max_dist, Fvector& hit) const
{
	if (!IsValid())
		return false;

	// March in half-cell steps, then refine the crossing by bisection
	const float step = CellSize() * 0.5f;
	Fvector prev = start;
	float prev_diff = start.y - HeightAt(start.x, start.z);

	for (float t = step; t <= max_dist; t += step)
	{
		Fvector p;
		p.mad(start, dir, t);

		float gx, gz;
		const bool inside = WorldToGrid(p.x, p.z, gx, gz);
		const float diff = p.y - HeightAt(p.x, p.z);

		if (inside && prev_diff > 0.f && diff <= 0.f)
		{
			float lo = t - step, hi = t;
			for (u32 i = 0; i < 16; ++i)
			{
				const float mid = (lo + hi) * 0.5f;
				Fvector m;
				m.mad(start, dir, mid);
				if (m.y - HeightAt(m.x, m.z) > 0.f)
					lo = mid;
				else
					hi = mid;
			}

			hit.mad(start, dir, hi);
			hit.y = HeightAt(hit.x, hit.z);
			return true;
		}

		prev = p;
		prev_diff = diff;
	}

	return false;
}

void CTerrainData::GetBox(Fbox& box) const
{
	box.invalidate();
	if (!IsValid())
		return;

	auto mm = std::minmax_element(Heights.begin(), Heights.end());
	box.set(-HalfSize(), *mm.first, -HalfSize(), HalfSize(), *mm.second, HalfSize());
}

bool CTerrainData::Save(const char* path) const
{
	IWriter* F = FS.w_open(path);
	if (!F)
		return false;

	F->open_chunk(chVersion);
	F->w_u32(TERRAIN_VERSION);
	F->close_chunk();

	F->open_chunk(chParams);
	F->w_stringZ(Name.c_str());
	F->w_u32(Resolution);
	F->w_float(Size);
	F->w_stringZ(Shader.c_str());
	F->w_stringZ(GameMtl.c_str());
	F->w_stringZ(ThmTemplate.c_str());
	F->close_chunk();

	F->open_chunk(chHeights);
	F->w(Heights.data(), Heights.size() * sizeof(float));
	F->close_chunk();

	F->open_chunk(chWeights);
	F->w(Weights.data(), Weights.size());
	F->close_chunk();

	F->open_chunk(chLayers);
	for (const STerrainLayer& L : Layers)
	{
		F->w_stringZ(L.Name.c_str());
		F->w_u32(L.PreviewColor);
	}
	F->close_chunk();

	FS.w_close(F);
	return true;
}

bool CTerrainData::Load(const char* path)
{
	IReader* F = FS.r_open(path);
	if (!F)
		return false;

	bool ok = false;
	IReader* C = F->open_chunk(chVersion);
	if (C && C->r_u32() == TERRAIN_VERSION)
	{
		C->close();

		shared_str str;
		C = F->open_chunk(chParams);
		C->r_stringZ(str); Name = str.c_str();
		Resolution = C->r_u32();
		Size = C->r_float();
		C->r_stringZ(str); Shader = str.c_str();
		C->r_stringZ(str); GameMtl = str.c_str();
		C->r_stringZ(str); ThmTemplate = str.c_str();
		C->close();

		const u32 count = Resolution * Resolution;
		Heights.resize(count);
		Weights.resize(count * LayerCount);

		C = F->open_chunk(chHeights);
		C->r(Heights.data(), count * sizeof(float));
		C->close();

		C = F->open_chunk(chWeights);
		C->r(Weights.data(), count * LayerCount);
		C->close();

		C = F->open_chunk(chLayers);
		for (STerrainLayer& L : Layers)
		{
			C->r_stringZ(str);
			L.Name = str.c_str();
			L.PreviewColor = C->r_u32();
		}
		C->close();
		ok = true;
	}
	else if (C)
	{
		C->close();
	}

	FS.r_close(F);
	return ok;
}

bool CTerrainData::ExportObject(const char* path, const char* object_name, const char* texture, u32 step) const
{
	if (!IsValid())
		return false;

	step = std::max<u32>(step, 1);
	const u32 n = (Resolution - 1) / step + 1;

	xr_vector<Fvector> vertices(n * n);
	for (u32 z = 0; z < n; ++z)
		for (u32 x = 0; x < n; ++x)
			VertexPos(std::min(x * step, Resolution - 1), std::min(z * step, Resolution - 1), vertices[z * n + x]);

	xr_vector<st_Face> faces;
	faces.reserve((n - 1) * (n - 1) * 2);
	for (u32 z = 0; z + 1 < n; ++z)
	{
		for (u32 x = 0; x + 1 < n; ++x)
		{
			const u32 v0 = z * n + x;
			const u32 v1 = v0 + 1;
			const u32 v2 = v0 + n;
			const u32 v3 = v2 + 1;

			st_Face f0, f1;
			f0.pv[0].pindex = v0; f0.pv[1].pindex = v2; f0.pv[2].pindex = v1;
			f1.pv[0].pindex = v1; f1.pv[1].pindex = v2; f1.pv[2].pindex = v3;
			faces.push_back(f0);
			faces.push_back(f1);
		}
	}

	CEditableObject* object = new CEditableObject(object_name);
	CEditableMesh* mesh = new CEditableMesh(object);
	object->AppendMesh(mesh);
	mesh->SetName("terrainShape");
	mesh->Create(faces.data(), u32(faces.size()), vertices.data(), u32(vertices.size()), nullptr, 0);

	st_VMap* uv = new st_VMap("Texture", vmtUV, false);
	for (u32 z = 0; z < n; ++z)
		for (u32 x = 0; x < n; ++x)
			uv->appendUV(float(x) / float(n - 1), 1.f - float(z) / float(n - 1));
	mesh->m_VMaps.push_back(uv);

	mesh->m_VMRefs.resize(vertices.size());
	for (u32 i = 0; i < vertices.size(); ++i)
	{
		st_VMapPtLst& ref = mesh->m_VMRefs[i];
		ref.count = 1;
		ref.pts = xr_alloc<st_VMapPt>(1);
		ref.pts[0].vmap_index = 0;
		ref.pts[0].index = i;
	}

	st_Face* mesh_faces = mesh->Faces();
	for (u32 f = 0; f < faces.size(); ++f)
		for (u32 j = 0; j < 3; ++j)
			mesh_faces[f].pv[j].vmref = mesh_faces[f].pv[j].pindex;

	CSurface* surface = mesh->GetSurfaceByFaceID(0);
	surface->SetName("terrain");
	surface->SetShader(Shader.c_str());
	surface->SetShaderXRLC("default");
	surface->SetGameMtl(GameMtl.c_str());
	surface->SetTexture(texture);
	surface->SetVMap("Texture");
	surface->SetFVF(D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1);

	mesh->GenerateFNormals();
	mesh->GenerateVNormals(nullptr, true);
	object->UpdateBox();

	const bool ok = object->Save(path);
	xr_delete(object);
	return ok;
}

bool CTerrainData::ExportTextures(const char* base_dds, const char* mask_dds, u32 tex_size) const
{
	if (!IsValid())
		return false;

	xr_vector<u32> base(tex_size * tex_size);
	xr_vector<u32> mask(tex_size * tex_size);
	const float scale = float(Resolution - 1) / float(tex_size - 1);

	for (u32 ty = 0; ty < tex_size; ++ty)
	{
		// texture v runs opposite to grid z (see the exported UVs)
		const float gz = float(tex_size - 1 - ty) * scale;
		for (u32 tx = 0; tx < tex_size; ++tx)
		{
			const float gx = float(tx) * scale;

			float rgb[3];
			BlendedColorAt(gx, gz, rgb);
			base[ty * tex_size + tx] = color_rgba(u32(rgb[0]), u32(rgb[1]), u32(rgb[2]), 255);

			float w[LayerCount];
			WeightsAt(gx, gz, w);
			mask[ty * tex_size + tx] = color_rgba(u32(w[0] * 255.f), u32(w[1] * 255.f), u32(w[2] * 255.f), u32(w[3] * 255.f));
		}
	}

	return WriteDDS_A8R8G8B8(base_dds, tex_size, base) && WriteDDS_A8R8G8B8(mask_dds, tex_size, mask);
}

bool WriteDDS_A8R8G8B8(const char* path, u32 size, const xr_vector<u32>& argb)
{
	IWriter* F = FS.w_open(path);
	if (!F)
		return false;

	u32 mip_count = 1;
	for (u32 s = size; s > 1; s >>= 1)
		++mip_count;

	// DDS_HEADER for an uncompressed A8R8G8B8 texture with a full mip chain
	F->w_u32(0x20534444);						// "DDS "
	F->w_u32(124);
	F->w_u32(0x1 | 0x2 | 0x4 | 0x8 | 0x1000 | 0x20000);
	F->w_u32(size);								// height
	F->w_u32(size);								// width
	F->w_u32(size * 4);							// pitch
	F->w_u32(0);
	F->w_u32(mip_count);
	for (u32 i = 0; i < 11; ++i)
		F->w_u32(0);
	F->w_u32(32);								// DDS_PIXELFORMAT
	F->w_u32(0x41);								// RGB | ALPHAPIXELS
	F->w_u32(0);
	F->w_u32(32);
	F->w_u32(0x00ff0000);
	F->w_u32(0x0000ff00);
	F->w_u32(0x000000ff);
	F->w_u32(0xff000000);
	F->w_u32(0x1000 | 0x8 | 0x400000);			// TEXTURE | COMPLEX | MIPMAP
	F->w_u32(0);
	F->w_u32(0);
	F->w_u32(0);
	F->w_u32(0);

	xr_vector<u32> level = argb;
	u32 level_size = size;
	for (u32 m = 0; m < mip_count; ++m)
	{
		F->w(level.data(), level.size() * sizeof(u32));
		if (level_size == 1)
			break;

		const u32 next = std::max<u32>(level_size / 2, 1);
		xr_vector<u32> down(next * next);
		for (u32 y = 0; y < next; ++y)
		{
			for (u32 x = 0; x < next; ++x)
			{
				u32 sum[4] = {};
				for (u32 dy = 0; dy < 2; ++dy)
				{
					for (u32 dx = 0; dx < 2; ++dx)
					{
						const u32 c = level[std::min(y * 2 + dy, level_size - 1) * level_size + std::min(x * 2 + dx, level_size - 1)];
						sum[0] += color_get_A(c);
						sum[1] += color_get_R(c);
						sum[2] += color_get_G(c);
						sum[3] += color_get_B(c);
					}
				}
				down[y * next + x] = color_rgba(sum[1] / 4, sum[2] / 4, sum[3] / 4, sum[0] / 4);
			}
		}
		level.swap(down);
		level_size = next;
	}

	FS.w_close(F);
	return true;
}
