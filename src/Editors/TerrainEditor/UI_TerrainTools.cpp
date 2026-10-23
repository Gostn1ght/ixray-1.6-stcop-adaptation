#include "stdafx.h"
#include "UI_TerrainTools.h"

CTerrainTool* TTools = nullptr;

namespace
{
	IC float Smooth01(float t)
	{
		t = std::clamp(t, 0.f, 1.f);
		return t * t * (3.f - 2.f * t);
	}

	IC float Hash01(u32 x, u32 z, u32 seed)
	{
		u32 h = x * 374761393u + z * 668265263u + seed * 2246822519u;
		h = (h ^ (h >> 13)) * 1274126177u;
		return float((h ^ (h >> 16)) & 0xffff) / 65535.f;
	}

	u32 ShadeColor(u32 color, const Fvector& n)
	{
		static Fvector light = []() { Fvector l; l.set(0.45f, 1.f, 0.3f); l.normalize(); return l; }();
		const float k = 0.3f + 0.7f * std::max(n.dotproduct(light), 0.f);
		return color_rgba(u32(color_get_R(color) * k), u32(color_get_G(color) * k), u32(color_get_B(color) * k), 255);
	}
}

CTerrainTool::CTerrainTool()
{
	TTools = this;
}

CTerrainTool::~CTerrainTool()
{
	TTools = nullptr;
}

bool CTerrainTool::OnCreate()
{
	m_bReady = true;
	SetAction(etaSelect);
	NewTerrain();
	m_bModified = false;
	return true;
}

void CTerrainTool::OnDestroy()
{
	m_bReady = false;
	m_Chunks.clear();
}

bool CTerrainTool::IfModified()
{
	if (!m_bModified)
		return true;

	const int res = ELog.DlgMsg(mtConfirmation, TMsgDlgButtons() | mbYes | mbNo | mbCancel, "Terrain '%s' was modified. Save it?", Data.Name.c_str());
	if (res == mrCancel)
		return false;
	if (res == mrYes)
		return SaveProject();
	return true;
}

//------------------------------------------------------------------------------
// Project
//------------------------------------------------------------------------------
xr_string CTerrainTool::ProjectPath(const char* name) const
{
	string_path path;
	xr_string rel = xr_string("terrain\\") + name + ".terrain";
	FS.update_path(path, "$server_data_root$", rel.c_str());
	return path;
}

xr_string CTerrainTool::ObjectRef() const
{
	return xr_string("levels\\") + Data.Name + "\\" + Data.Name + "_terrain";
}

void CTerrainTool::NewTerrain()
{
	Data.Create(u32(NewResolution), NewSize, NewHeight);
	m_Undo.clear();
	m_Redo.clear();
	RebuildChunks();
	ZoomObject(false);
	m_bModified = true;
}

bool CTerrainTool::SaveProject()
{
	const xr_string path = ProjectPath(Data.Name.c_str());
	if (!Data.Save(path.c_str()))
	{
		ELog.DlgMsg(mtError, "Can't save terrain '%s'", path.c_str());
		return false;
	}

	ELog.Msg(mtInformation, "Terrain saved: %s", path.c_str());
	m_bModified = false;
	return true;
}

bool CTerrainTool::LoadProject(const char* name)
{
	if (!IfModified())
		return false;

	const xr_string path = ProjectPath(name);
	CTerrainData loaded;
	if (!loaded.Load(path.c_str()) || !loaded.IsValid())
	{
		ELog.DlgMsg(mtError, "Can't load terrain '%s'", path.c_str());
		return false;
	}

	Data = loaded;
	m_Undo.clear();
	m_Redo.clear();
	RebuildChunks();
	ZoomObject(false);
	m_bModified = false;
	return true;
}

void CTerrainTool::ListProjects(xr_vector<xr_string>& names) const
{
	names.clear();
	FS_FileSet files;
	FS.file_list(files, "$server_data_root$", FS_ListFiles | FS_ClampExt, "terrain\\*.terrain");
	for (const FS_File& file : files)
	{
		xr_string name = file.name;
		const size_t slash = name.find_last_of('\\');
		names.push_back(slash == xr_string::npos ? name : name.substr(slash + 1));
	}
}

bool CTerrainTool::Save(const char* name, bool bInternal)
{
	return SaveProject();
}

bool CTerrainTool::Load(const char* name)
{
	return name && LoadProject(name);
}

//------------------------------------------------------------------------------
// Export
//------------------------------------------------------------------------------
bool CTerrainTool::Export()
{
	if (!Data.IsValid())
		return false;

	const xr_string& name = Data.Name;
	const xr_string texture = xr_string("terrain\\terrain_") + name;

	string_path object_path, base_dds, mask_dds;
	FS.update_path(object_path, _objects_, (ObjectRef() + ".object").c_str());
	FS.update_path(base_dds, "$game_textures$", (texture + ".dds").c_str());
	FS.update_path(mask_dds, "$game_textures$", (texture + "_mask.dds").c_str());

	if (!Data.ExportObject(object_path, ObjectRef().c_str(), texture.c_str(), u32(ExportStep)))
	{
		ELog.DlgMsg(mtError, "Can't export terrain object '%s'", object_path);
		return false;
	}

	if (!Data.ExportTextures(base_dds, mask_dds, u32(ExportTexSize)))
	{
		ELog.DlgMsg(mtError, "Can't export terrain textures '%s'", base_dds);
		return false;
	}

	// Texture parameters (detail texture, type, flags) come from a game terrain of the same kind
	auto CopyThm = [](const xr_string& from, const xr_string& to)
	{
		string_path src;
		FS.update_path(src, "$game_textures$", (from + ".thm").c_str());
		IReader* R = FS.r_open(src);
		if (!R)
		{
			Msg("! Terrain: thumbnail template '%s' not found", src);
			return;
		}

		string_path dst;
		FS.update_path(dst, "$game_textures$", (to + ".thm").c_str());
		IWriter* W = FS.w_open(dst);
		if (W)
		{
			W->w(R->pointer(), R->length());
			FS.w_close(W);
		}
		FS.r_close(R);
	};

	CopyThm(Data.ThmTemplate, texture);
	CopyThm(Data.ThmTemplate + "_mask", texture + "_mask");

	ELog.Msg(mtInformation, "Terrain exported: %s", object_path);
	return true;
}

bool CTerrainTool::OpenInLevelEditor()
{
	if (!SaveProject() || !Export())
		return false;

	string_path exe;
	GetModuleFileNameA(nullptr, exe, sizeof(exe));
	if (char* slash = strrchr(exe, '\\'))
		slash[1] = 0;

	xr_string folder = exe;
	xr_string cmd = "\"" + folder + "LevelEditor.exe\" -nosound -scene " + Data.Name + ".level -add_object " + ObjectRef();

	STARTUPINFOA si = {};
	si.cb = sizeof(si);
	PROCESS_INFORMATION pi = {};
	if (!CreateProcessA(nullptr, cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, folder.c_str(), &si, &pi))
	{
		ELog.DlgMsg(mtError, "Can't start Level Editor: %s", cmd.c_str());
		return false;
	}

	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);
	return true;
}

//------------------------------------------------------------------------------
// Undo
//------------------------------------------------------------------------------
CTerrainTool::SSnapshot CTerrainTool::TakeSnapshot() const
{
	return { Data.Heights, Data.Weights };
}

void CTerrainTool::RestoreSnapshot(const SSnapshot& snapshot)
{
	Data.Heights = snapshot.Heights;
	Data.Weights = snapshot.Weights;
	for (SChunk& chunk : m_Chunks)
		chunk.Dirty = true;
	m_bModified = true;
}

void CTerrainTool::PushUndo()
{
	m_Undo.push_back(TakeSnapshot());
	if (m_Undo.size() > MaxUndo)
		m_Undo.erase(m_Undo.begin());
	m_Redo.clear();
}

void CTerrainTool::Undo()
{
	if (m_Undo.empty())
		return;

	m_Redo.push_back(TakeSnapshot());
	RestoreSnapshot(m_Undo.back());
	m_Undo.pop_back();
}

void CTerrainTool::Redo()
{
	if (m_Redo.empty())
		return;

	m_Undo.push_back(TakeSnapshot());
	RestoreSnapshot(m_Redo.back());
	m_Redo.pop_back();
}

//------------------------------------------------------------------------------
// Brush
//------------------------------------------------------------------------------
void CTerrainTool::UpdateCursor()
{
	Fvector hit;
	m_CursorValid = Data.RayPick(UI->m_CurrentRStart, UI->m_CurrentRDir, UI->ZFar(), hit);
	if (m_CursorValid)
		m_Cursor = hit;
}

bool CTerrainTool::MouseStart(TShiftState Shift)
{
	if (!(Shift & ssLeft) || !Data.IsValid())
		return false;

	UpdateCursor();
	if (!m_CursorValid)
		return false;

	PushUndo();
	m_FlattenHeight = m_Cursor.y;
	m_NoiseSeed++;
	m_Stroking = true;
	ApplyBrush(EDevice->fTimeDelta);
	return true;
}

void CTerrainTool::MouseMove(TShiftState Shift)
{
	UpdateCursor();
}

bool CTerrainTool::MouseEnd(TShiftState Shift)
{
	m_Stroking = false;
	return true;
}

void CTerrainTool::ApplyBrush(float dt)
{
	if (!m_CursorValid || !Data.IsValid())
		return;

	dt = std::clamp(dt, 0.f, 0.1f);
	const float cell = Data.CellSize();
	const float radius = std::max(BrushRadius, cell);
	const float inner = 1.f - std::clamp(BrushFalloff, 0.f, 1.f);

	float gx, gz;
	Data.WorldToGrid(m_Cursor.x, m_Cursor.z, gx, gz);
	const float rc = radius / cell;
	const u32 last = Data.Resolution - 1;
	const u32 x0 = u32(std::clamp(gx - rc, 0.f, float(last)));
	const u32 x1 = u32(std::clamp(gx + rc + 1.f, 0.f, float(last)));
	const u32 z0 = u32(std::clamp(gz - rc, 0.f, float(last)));
	const u32 z1 = u32(std::clamp(gz + rc + 1.f, 0.f, float(last)));

	// Smoothing reads the untouched neighbourhood
	xr_vector<float> source;
	if (Brush == ETerrainBrush::Smooth)
	{
		source.resize((x1 - x0 + 1) * (z1 - z0 + 1));
		for (u32 z = z0; z <= z1; ++z)
			for (u32 x = x0; x <= x1; ++x)
				source[(z - z0) * (x1 - x0 + 1) + (x - x0)] = Data.H(x, z);
	}

	auto Src = [&](u32 x, u32 z)
	{
		x = std::clamp(x, x0, x1);
		z = std::clamp(z, z0, z1);
		return source[(z - z0) * (x1 - x0 + 1) + (x - x0)];
	};

	for (u32 z = z0; z <= z1; ++z)
	{
		for (u32 x = x0; x <= x1; ++x)
		{
			const float dx = (float(x) - gx) * cell;
			const float dz = (float(z) - gz) * cell;
			const float d = _sqrt(dx * dx + dz * dz) / radius;
			if (d > 1.f)
				continue;

			const float w = (d <= inner) ? 1.f : 1.f - Smooth01((d - inner) / std::max(1.f - inner, EPS));
			const float k = BrushStrength * dt * w;
			float& h = Data.H(x, z);

			switch (Brush)
			{
			case ETerrainBrush::Raise:
				h += k;
				break;
			case ETerrainBrush::Lower:
				h -= k;
				break;
			case ETerrainBrush::Smooth:
			{
				const float avg = (Src(x - 1, z) + Src(x + 1, z) + Src(x, z - 1) + Src(x, z + 1) +
					Src(x - 1, z - 1) + Src(x + 1, z - 1) + Src(x - 1, z + 1) + Src(x + 1, z + 1) + Src(x, z)) / 9.f;
				h += (avg - h) * std::min(k * 0.5f, 1.f);
				break;
			}
			case ETerrainBrush::Flatten:
				h += (m_FlattenHeight - h) * std::min(k * 0.5f, 1.f);
				break;
			case ETerrainBrush::Noise:
				h += (Hash01(x, z, m_NoiseSeed) - 0.5f) * k * 2.f;
				break;
			case ETerrainBrush::Paint:
			{
				const float a = std::min(k * 0.5f, 1.f);
				u8* weights = Data.W(x, z);
				for (u32 l = 0; l < CTerrainData::LayerCount; ++l)
				{
					const float target = (int(l) == PaintLayer) ? 255.f : 0.f;
					weights[l] = u8(std::clamp(weights[l] + (target - weights[l]) * a + 0.5f, 0.f, 255.f));
				}
				break;
			}
			}
		}
	}

	MarkDirty(x0 > 0 ? x0 - 1 : 0, z0 > 0 ? z0 - 1 : 0, std::min(x1 + 1, last), std::min(z1 + 1, last));
	m_bModified = true;
}

void CTerrainTool::OnFrame()
{
	if (!m_bReady)
		return;

	if (m_Stroking)
	{
		UpdateCursor();
		ApplyBrush(EDevice->fTimeDelta);
	}
	else if (!UI->IsMouseCaptured())
	{
		UpdateCursor();
	}
}

bool CTerrainTool::RayPick(const Fvector& start, const Fvector& dir, float& dist, Fvector* pt, Fvector* n)
{
	Fvector hit;
	if (!Data.RayPick(start, dir, dist, hit))
		return false;

	dist = start.distance_to(hit);
	if (pt)
		pt->set(hit);
	if (n)
		n->set(0.f, 1.f, 0.f);
	return true;
}

void CTerrainTool::ZoomObject(bool bSelOnly)
{
	Fbox box;
	Data.GetBox(box);
	if (box.is_valid())
		UI->CurrentView().m_Camera.ZoomExtents(box);
}

CCommandVar CTerrainTool::CommandSave(CCommandVar p1, CCommandVar p2)
{
	return SaveProject();
}

CCommandVar CTerrainTool::CommandUndo(CCommandVar p1, CCommandVar p2)
{
	Undo();
	return true;
}

CCommandVar CTerrainTool::CommandRedo(CCommandVar p1, CCommandVar p2)
{
	Redo();
	return true;
}

CCommandVar CTerrainTool::CommandClear(CCommandVar p1, CCommandVar p2)
{
	if (!IfModified())
		return false;
	NewTerrain();
	return true;
}

//------------------------------------------------------------------------------
// Render
//------------------------------------------------------------------------------
void CTerrainTool::RebuildChunks()
{
	m_Chunks.clear();
	if (!Data.IsValid())
		return;

	const u32 cells = Data.Resolution - 1;
	m_ChunksPerSide = (cells + ChunkCells - 1) / ChunkCells;
	m_Chunks.resize(m_ChunksPerSide * m_ChunksPerSide);

	for (u32 cz = 0; cz < m_ChunksPerSide; ++cz)
	{
		for (u32 cx = 0; cx < m_ChunksPerSide; ++cx)
		{
			SChunk& chunk = m_Chunks[cz * m_ChunksPerSide + cx];
			chunk.X0 = cx * ChunkCells;
			chunk.Z0 = cz * ChunkCells;
		}
	}
}

void CTerrainTool::MarkDirty(u32 x0, u32 z0, u32 x1, u32 z1)
{
	if (m_ChunksPerSide == 0)
		return;

	const u32 cx0 = std::min(x0 / ChunkCells, m_ChunksPerSide - 1);
	const u32 cx1 = std::min(x1 / ChunkCells, m_ChunksPerSide - 1);
	const u32 cz0 = std::min(z0 / ChunkCells, m_ChunksPerSide - 1);
	const u32 cz1 = std::min(z1 / ChunkCells, m_ChunksPerSide - 1);

	for (u32 cz = cz0; cz <= cz1; ++cz)
		for (u32 cx = cx0; cx <= cx1; ++cx)
			m_Chunks[cz * m_ChunksPerSide + cx].Dirty = true;
}

void CTerrainTool::BuildChunk(SChunk& chunk, u32 step)
{
	chunk.Tris.clear();
	chunk.Colors.clear();
	chunk.Box.invalidate();
	chunk.Step = step;
	chunk.Dirty = false;

	const u32 last = Data.Resolution - 1;
	const u32 xe = std::min(chunk.X0 + ChunkCells, last);
	const u32 ze = std::min(chunk.Z0 + ChunkCells, last);

	for (u32 z = chunk.Z0; z < ze; z += step)
	{
		for (u32 x = chunk.X0; x < xe; x += step)
		{
			const u32 xn = std::min(x + step, xe);
			const u32 zn = std::min(z + step, ze);

			Fvector p00, p10, p01, p11;
			Data.VertexPos(x, z, p00);
			Data.VertexPos(xn, z, p10);
			Data.VertexPos(x, zn, p01);
			Data.VertexPos(xn, zn, p11);

			Fvector n;
			Data.Normal(x, z, n);
			const u32 color = ShadeColor(Data.BlendedColor(x, z), n);

			chunk.Tris.insert(chunk.Tris.end(), { p00, p01, p10, p10, p01, p11 });
			chunk.Colors.push_back(color);
			chunk.Colors.push_back(color);

			chunk.Box.modify(p00);
			chunk.Box.modify(p10);
			chunk.Box.modify(p01);
			chunk.Box.modify(p11);
		}
	}
}

void CTerrainTool::Render()
{
	if (!m_bReady || !Data.IsValid())
		return;

	const Fvector& cam = EDevice->vCameraPosition;
	CFrustum& frustum = ::Render->ViewBase;

	RCache.set_xform_world(Fidentity);
	DU_impl.DD_DrawFace_begin(false);
	GRHI->StateManager->SetCullMode(ERHI_CULLMODE::NONE);
	EDevice->SetShader(EDevice->m_WireShader);

	const float chunk_size = ChunkCells * Data.CellSize();
	for (SChunk& chunk : m_Chunks)
	{
		// Distance based level of detail, rebuilt lazily
		Fvector center;
		center.set(-Data.HalfSize() + (chunk.X0 + ChunkCells * 0.5f) * Data.CellSize(), cam.y,
			-Data.HalfSize() + (chunk.Z0 + ChunkCells * 0.5f) * Data.CellSize());
		const float dist = center.distance_to(cam) / std::max(chunk_size, 1.f);
		const u32 step = dist < 4.f ? 1 : dist < 8.f ? 2 : dist < 16.f ? 4 : 8;

		if (chunk.Dirty || chunk.Step != step)
			BuildChunk(chunk, step);

		if (chunk.Tris.empty())
			continue;

		float aabb[6] = { chunk.Box.min.x, chunk.Box.min.y, chunk.Box.min.z, chunk.Box.max.x, chunk.Box.max.y, chunk.Box.max.z };
		u32 mask = 0xff;
		if (frustum.testAABB(aabb, mask) == fcvNone)
			continue;

		for (size_t i = 0, t = 0; i < chunk.Tris.size(); i += 3, ++t)
			DU_impl.DD_DrawFace_push(chunk.Tris[i], chunk.Tris[i + 1], chunk.Tris[i + 2], chunk.Colors[t]);
	}

	DU_impl.DD_DrawFace_end();

	// Brush outline on the surface
	if (m_CursorValid)
	{
		constexpr int segments = 48;
		Fvector ring[segments + 1];
		for (int i = 0; i <= segments; ++i)
		{
			const float a = PI_MUL_2 * float(i) / float(segments);
			ring[i].set(m_Cursor.x + BrushRadius * std::cos(a), 0.f, m_Cursor.z + BrushRadius * std::sin(a));
			ring[i].y = Data.HeightAt(ring[i].x, ring[i].z) + 0.15f;
		}
		DU_impl.DrawPrimitiveL(ERHI_PRIMITIVE_TOPOLOGY::LINE_STRIP, segments, ring, segments + 1, 0xffffff00, false, false);
	}
}
