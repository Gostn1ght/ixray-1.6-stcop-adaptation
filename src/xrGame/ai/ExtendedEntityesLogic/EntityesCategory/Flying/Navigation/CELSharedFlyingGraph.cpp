#include "stdafx.h"
#include <algorithm>
#include "ai/ExtendedEntityesLogic/EntityesCategory/Flying/Navigation/CELSharedFlyingGraph.h"
#include "ai/ExtendedEntityesLogic/EntryPoint.h"
#include "ai/ExtendedEntityesLogic/CELSharedSettings.h"
#include "ai/ExtendedEntityesLogic/Navigation/CELSharedNavigationScheduler.h"
#include "ai/ExtendedEntityesLogic/Perceptions/CELSharedAnomalies.h"

namespace ExtendedEntityesAirGraph
{
struct SCell
{
	s32 Horizontal = 0;
	s32 Vertical = 0;
	s32 Lateral = 0;
	// Упорядочивает координатные ключи для поиска в общем реестре.
	bool operator<(const SCell& Other) const
	{
		if (Horizontal != Other.Horizontal)
		{
			return Horizontal < Other.Horizontal;
		}
		if (Vertical != Other.Vertical)
		{
			return Vertical < Other.Vertical;
		}
		return Lateral < Other.Lateral;
	}
};
struct SNode
{
	SCell Cell;
	float Cost = FLT_MAX;
	bool Expanded = false;
};
struct SCapture
{
	SELNavigationRequest Request;
	xr_vector<SNode> Nodes;
	xr_map<SCell, u32> CellNodes;
	Fbox Bounds;
	Fvector Forward = {0.f, 0.f, 1.f};
	Fvector Side = {1.f, 0.f, 0.f};
	SCell CandidateCell;
	Fvector CandidatePosition = {0.f, 0.f, 0.f};
	float Radius = 0.f;
	float CenterHeight = 0.f;
	float CellSize = 0.f;
	u32 NodeLimit = 0;
	u32 Deadline = 0;
	u32 CurrentNode = 0;
	u32 Edge = 0;
	bool HasEdge = false;
	EELGeometryResult State = EELGeometryResult::Pending;
};
const SCell Directions[] = {{0, 1, 0}, {1, 0, 0}, {0, 0, 1}, {0, 0, -1}, {-1, 0, 0}, {0, -1, 0}};

// Выбирает следующий перспективный узел локального графа.
bool SelectNode(SCapture& Capture)
{
	u32 NodeIndex;
	u32 BestNode = u32(-1);
	float BestScore = FLT_MAX;
	float Score;
	for (NodeIndex = 0; NodeIndex < Capture.Nodes.size(); ++NodeIndex)
	{
		if (NodeIndex == Capture.Request.GoalNode || Capture.Nodes[NodeIndex].Expanded)
		{
			continue;
		}
		Score = Capture.Nodes[NodeIndex].Cost + Capture.Request.Graph[NodeIndex].Position.distance_to(Capture.Request.GoalPosition);
		if (Score < BestScore)
		{
			BestScore = Score;
			BestNode = NodeIndex;
		}
	}
	if (BestNode == u32(-1))
	{
		Capture.State = EELGeometryResult::Blocked;
		return false;
	}
	Capture.CurrentNode = BestNode;
	Capture.Nodes[BestNode].Expanded = true;
	Capture.Edge = 0;
	return true;
}

// Добавляет проверенное ребро и при необходимости соседний узел в снимок.
void AcceptEdge(SCapture& Capture)
{
	if (Capture.Edge == 0)
	{
		Capture.Request.Graph[Capture.CurrentNode].Neighbors[6] = Capture.Request.GoalNode;
		Capture.State = EELGeometryResult::Clear;
		return;
	}
	xr_map<SCell, u32>::iterator Existing = Capture.CellNodes.find(Capture.CandidateCell);
	u32 NodeIndex;
	const float Cost = Capture.Nodes[Capture.CurrentNode].Cost + Capture.CellSize;
	if (Existing == Capture.CellNodes.end())
	{
		if (Capture.Nodes.size() >= Capture.NodeLimit)
		{
			return;
		}
		NodeIndex = u32(Capture.Nodes.size());
		SNode Node;
		Node.Cell = Capture.CandidateCell;
		Node.Cost = Cost;
		Capture.Nodes.push_back(Node);
		SELNavigationNode GraphNode;
		GraphNode.Position = Capture.CandidatePosition;
		Capture.Request.Graph.push_back(GraphNode);
		Capture.CellNodes.emplace(Capture.CandidateCell, NodeIndex);
	}
	else
	{
		NodeIndex = Existing->second;
		Capture.Nodes[NodeIndex].Cost = std::min(Capture.Nodes[NodeIndex].Cost, Cost);
	}
	Capture.Request.Graph[Capture.CurrentNode].Neighbors[Capture.Edge - 1] = NodeIndex;
}

// Принимает результат геометрической проверки текущего ребра.
void FinishEdge(SCapture& Capture)
{
	SELNavigationResult CheckedEdge;
	const EELGeometryResult State = CELFlyingGeometry::Take(Capture.Request.Identity, CheckedEdge);
	if (State == EELGeometryResult::Pending)
	{
		return;
	}
	Capture.HasEdge = false;
	if (State == EELGeometryResult::Clear)
	{
		AcceptEdge(Capture);
	}
	++Capture.Edge;
}

// Начинает бюджетную проверку следующего ребра воздушного графа.
bool StartEdge(SCapture& Capture)
{
	if (!ExtendedEntityes::Navigation().TryCaptureNode())
	{
		return false;
	}
	if (Capture.Edge == 7 && !SelectNode(Capture))
	{
		return true;
	}
	Capture.CandidatePosition = Capture.Request.GoalPosition;
	if (Capture.Edge != 0)
	{
		Capture.CandidateCell = Capture.Nodes[Capture.CurrentNode].Cell;
		const SCell& Direction = Directions[Capture.Edge - 1];
		Capture.CandidateCell.Horizontal += Direction.Horizontal;
		Capture.CandidateCell.Vertical += Direction.Vertical;
		Capture.CandidateCell.Lateral += Direction.Lateral;
		Capture.CandidatePosition = Capture.Request.StartPosition;
		Capture.CandidatePosition.mad(Capture.Forward, float(Capture.CandidateCell.Horizontal) * Capture.CellSize);
		Capture.CandidatePosition.mad(Capture.Side, float(Capture.CandidateCell.Lateral) * Capture.CellSize);
		Capture.CandidatePosition.y += float(Capture.CandidateCell.Vertical) * Capture.CellSize;
	}
	const Fvector& Start = Capture.Request.Graph[Capture.CurrentNode].Position;
	if (!Capture.Bounds.contains(Capture.CandidatePosition) || CELSharedAnomalies::Crosses(Capture.Request.Obstacles, Start, Capture.CandidatePosition))
	{
		++Capture.Edge;
		return true;
	}
	SELNavigationResult EdgeRoute;
	EdgeRoute.Identity = Capture.Request.Identity;
	EdgeRoute.HasPath = true;
	EdgeRoute.Points.push_back(Start);
	EdgeRoute.Points.push_back(Capture.CandidatePosition);
	Capture.HasEdge = CELFlyingGeometry::Submit(std::move(EdgeRoute), Capture.Radius, Capture.CenterHeight);
	return true;
}
} // namespace ExtendedEntityesAirGraph
struct CELSharedFlyingGraph::SState
{
	xr_vector<ExtendedEntityesAirGraph::SCapture> Captures;
	u32 NextCapture = 0;
};
// Возвращает общее хранилище состояния очереди.
CELSharedFlyingGraph::SState& CELSharedFlyingGraph::State()
{
	static SState Runtime;
	return Runtime;
}


// Помещает заявку в очередь с проверкой допустимости и доступного места.
bool CELSharedFlyingGraph::Submit(SELNavigationRequest Request, float Radius, float CenterHeight, float CellSize, u32 NodeLimit, u32 Timeout)
{
	using namespace ExtendedEntityesAirGraph;
	if (State().Captures.size() >= CELSharedSettings::Work().NavigationTaskLimit || !Request.IsFlying || NodeLimit < 2 || !_valid(CellSize) || CellSize <= EPS_L)
	{
		return false;
	}
	SCapture Capture;
	Capture.Request = std::move(Request);
	Capture.Request.Graph.clear();
	Capture.Request.StartNode = 0;
	Capture.Request.GoalNode = 1;
	Capture.Bounds = CELFlyingGeometry::AirBounds(Radius, Radius, CenterHeight);
	Capture.Radius = Radius;
	Capture.CenterHeight = CenterHeight;
	Capture.CellSize = CellSize;
	Capture.NodeLimit = std::min(NodeLimit, CELSharedSettings::Work().NavigationGraphNodeLimit);
	Capture.Deadline = Device.dwTimeGlobal + Timeout;
	Capture.Forward.sub(Capture.Request.GoalPosition, Capture.Request.StartPosition);
	Capture.Forward.y = 0.f;
	if (Capture.Forward.square_magnitude() < EPS_L)
	{
		Capture.Forward = Capture.Request.StartDirection;
		Capture.Forward.y = 0.f;
	}
	if (Capture.Forward.square_magnitude() < EPS_L)
	{
		Capture.Forward.set(0.f, 0.f, 1.f);
	}
	Capture.Forward.normalize_safe();
	Capture.Side.set(Capture.Forward.z, 0.f, -Capture.Forward.x);
	SELNavigationNode StartNode;
	StartNode.Position = Capture.Request.StartPosition;
	SELNavigationNode GoalNode;
	GoalNode.Position = Capture.Request.GoalPosition;
	Capture.Request.Graph.push_back(StartNode);
	Capture.Request.Graph.push_back(GoalNode);
	Capture.Nodes.resize(2);
	Capture.Nodes[0].Cost = 0.f;
	Capture.Nodes[0].Expanded = true;
	Capture.Nodes[1].Expanded = true;
	Capture.CellNodes.emplace(SCell(), 0);
	State().Captures.push_back(std::move(Capture));
	return true;
}

// Порционно захватывает проверенные воздушные рёбра с сохранением курсора очереди.
void CELSharedFlyingGraph::BeginFrame()
{
	using namespace ExtendedEntityesAirGraph;
	u32 Remaining = u32(State().Captures.size());
	SCapture* Capture;
	while (Remaining > 0 && !State().Captures.empty())
	{
		--Remaining;
		State().NextCapture %= u32(State().Captures.size());
		Capture = &State().Captures[State().NextCapture++];
		if (Capture->State != EELGeometryResult::Pending)
		{
			continue;
		}
		if (s32(Device.dwTimeGlobal - Capture->Deadline) >= 0)
		{
			CELFlyingGeometry::Cancel(Capture->Request.Identity);
			Capture->HasEdge = false;
			Capture->State = EELGeometryResult::Blocked;
			continue;
		}
		if (Capture->HasEdge)
		{
			FinishEdge(*Capture);
		}
		if (!Capture->HasEdge && Capture->State == EELGeometryResult::Pending)
		{
			if (!StartEdge(*Capture))
			{
				break;
			}
		}
	}
}

// Возвращает состояние заявки и передаёт готовые данные вызывающему коду.
EELGeometryResult CELSharedFlyingGraph::Take(const SELNavigationIdentity& Identity, SELNavigationRequest& Request)
{
	using namespace ExtendedEntityesAirGraph;
	xr_vector<SCapture>::iterator CaptureIterator;
	EELGeometryResult ResultState;
	for (CaptureIterator = State().Captures.begin(); CaptureIterator != State().Captures.end(); ++CaptureIterator)
	{
		if (CaptureIterator->Request.Identity != Identity)
		{
			continue;
		}
		ResultState = CaptureIterator->State;
		if (ResultState != EELGeometryResult::Pending)
		{
			Request = std::move(CaptureIterator->Request);
			State().Captures.erase(CaptureIterator);
		}
		return ResultState;
	}
	return EELGeometryResult::Blocked;
}

// Отменяет принадлежащую владельцу заявку и очищает промежуточный результат.
void CELSharedFlyingGraph::Cancel(const SELNavigationIdentity& Identity)
{
	using namespace ExtendedEntityesAirGraph;
	xr_vector<SCapture>::iterator CaptureIterator = State().Captures.begin();
	while (CaptureIterator != State().Captures.end())
	{
		if (CaptureIterator->Request.Identity == Identity)
		{
			if (CaptureIterator->HasEdge)
			{
				CELFlyingGeometry::Cancel(Identity);
			}
			CaptureIterator = State().Captures.erase(CaptureIterator);
		}
		else
		{
			++CaptureIterator;
		}
	}
}
// Сбрасывает накопленные данные и незавершённую работу подсистемы.
void CELSharedFlyingGraph::Clear()
{
	using namespace ExtendedEntityesAirGraph;
	xr_vector<SCapture>::const_iterator CaptureIterator;
	for (CaptureIterator = State().Captures.begin(); CaptureIterator != State().Captures.end(); ++CaptureIterator)
	{
		if (CaptureIterator->HasEdge)
		{
			CELFlyingGeometry::Cancel(CaptureIterator->Request.Identity);
		}
	}
	State().Captures.clear();
	State().NextCapture = 0;
}
// Возвращает число заявок, ожидающих обработки или получения результата.
u32 CELSharedFlyingGraph::OutstandingRequests()
{
	return u32(State().Captures.size());
}
