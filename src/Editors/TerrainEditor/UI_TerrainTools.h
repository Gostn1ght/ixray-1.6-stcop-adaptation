#pragma once
#include "TerrainData.h"

enum class ETerrainBrush : int
{
	Raise,
	Lower,
	Smooth,
	Flatten,
	Noise,
	Paint,
};

class CTerrainTool : public CToolCustom
{
	typedef CToolCustom inherited;

	struct SChunk
	{
		u32 X0 = 0, Z0 = 0;			// first cell
		u32 Step = 0;				// grid step the cached triangles were built with
		bool Dirty = true;
		Fbox Box;
		xr_vector<Fvector> Tris;
		xr_vector<u32> Colors;		// one per triangle
	};

	struct SSnapshot
	{
		xr_vector<float> Heights;
		xr_vector<u8> Weights;
	};

	static constexpr u32 ChunkCells = 32;
	static constexpr u32 MaxUndo = 24;

	bool m_bModified = false;

	xr_vector<SChunk> m_Chunks;
	u32 m_ChunksPerSide = 0;

	xr_vector<SSnapshot> m_Undo;
	xr_vector<SSnapshot> m_Redo;

	bool m_Stroking = false;
	bool m_CursorValid = false;
	Fvector m_Cursor = {};
	float m_FlattenHeight = 0.f;
	u32 m_NoiseSeed = 1;

	void RebuildChunks();
	void BuildChunk(SChunk& chunk, u32 step);
	void MarkDirty(u32 x0, u32 z0, u32 x1, u32 z1);
	void UpdateCursor();
	void ApplyBrush(float dt);
	SSnapshot TakeSnapshot() const;
	void RestoreSnapshot(const SSnapshot& snapshot);
	void PushUndo();

public:
	CTerrainData Data;

	// brush
	ETerrainBrush Brush = ETerrainBrush::Raise;
	float BrushRadius = 12.f;		// meters
	float BrushStrength = 4.f;		// meters per second for sculpting, rate for painting
	float BrushFalloff = 0.6f;		// 0 - hard edge, 1 - fully soft
	int PaintLayer = 0;

	// new terrain
	int NewResolution = 257;
	float NewSize = 256.f;
	float NewHeight = 0.f;

	// export
	int ExportStep = 1;
	int ExportTexSize = 2048;

	CTerrainTool();
	virtual ~CTerrainTool();

	virtual void Render();
	virtual void RenderEnvironment() {}
	virtual void OnFrame();
	virtual bool OnCreate();
	virtual void OnDestroy();
	virtual bool IfModified();
	virtual bool IsModified() { return m_bModified; }
	virtual void Modified() { m_bModified = true; }
	virtual const char* GetInfo() { return nullptr; }
	virtual void ZoomObject(bool bSelOnly);
	virtual bool Load(const char* name);
	virtual bool Save(const char* name, bool bInternal = false);
	virtual void Reload() {}
	virtual void OnDeviceCreate() {}
	virtual void OnDeviceDestroy() {}
	virtual void Clear() { inherited::Clear(); }
	virtual void OnShowHint(AStringVec& SS) {}
	virtual bool MouseStart(TShiftState Shift);
	virtual bool MouseEnd(TShiftState Shift);
	virtual void MouseMove(TShiftState Shift);
	virtual bool Pick(TShiftState Shift) { return false; }
	virtual bool RayPick(const Fvector& start, const Fvector& dir, float& dist, Fvector* pt, Fvector* n);
	virtual void ShowProperties(const char* focused_item) {}
	virtual void UpdateProperties(bool bForced = false) {}
	virtual bool GetSelectionPosition(Fmatrix& result) { return false; }

	void NewTerrain();
	bool SaveProject();
	bool LoadProject(const char* name);
	void ListProjects(xr_vector<xr_string>& names) const;
	bool Export();
	bool OpenInLevelEditor();

	void Undo();
	void Redo();
	bool CanUndo() const { return !m_Undo.empty(); }
	bool CanRedo() const { return !m_Redo.empty(); }

	void InvalidateRender() { for (SChunk& chunk : m_Chunks) chunk.Dirty = true; }
	bool CursorValid() const { return m_CursorValid; }
	const Fvector& Cursor() const { return m_Cursor; }

	// paths
	xr_string ProjectPath(const char* name) const;
	xr_string ObjectRef() const;

	CCommandVar CommandSave(CCommandVar p1, CCommandVar p2);
	CCommandVar CommandUndo(CCommandVar p1, CCommandVar p2);
	CCommandVar CommandRedo(CCommandVar p1, CCommandVar p2);
	CCommandVar CommandClear(CCommandVar p1, CCommandVar p2);
};

extern CTerrainTool* TTools;
