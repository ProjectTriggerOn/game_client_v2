#include "ball.h"

#include <random>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <queue>
#include <algorithm>
#include <cmath>
#include <limits>

Sphere Ball::GetCollisionSphere() const
{
	return { m_position, m_Radius };
}

void Ball::Update(double elapsed_time)
{
	(void)elapsed_time;
}

bool Ball::isDestroyed() const
{
	return m_Hp <= 0;
}

void Ball::Draw() const
{
	if (m_pModel) {
		XMMATRIX mtxW = XMMatrixTranslation(m_position.x, m_position.y + 1, m_position.z);
		ModelDraw(m_pModel, mtxW);
	}
}

namespace
{
	constexpr int MAX_BALL = 6;

	// XY 平面：z 固定（这里固定为 0）
	XMFLOAT3 g_BallAreaMin = { -7.0f,  2.0f, 0.0f };
	XMFLOAT3 g_BallAreaMax = { 7.0f, 7.0f, 0.0f };

	Ball* g_pBalls[MAX_BALL]{};
	int    g_BallCount{ 0 };

	// 全局只加载一次模型，所有 Ball 共享
	MODEL* g_pBallModel = nullptr;

	// -------------------------
	// 整数格点（XY）
	// -------------------------
	struct Int2 {
		int x = 0;
		int y = 0;
		bool operator==(const Int2& o) const { return x == o.x && y == o.y; }
	};

	struct Int2Hash {
		size_t operator()(const Int2& p) const noexcept {
			uint64_t ux = (uint32_t)(p.x * 73856093);
			uint64_t uy = (uint32_t)(p.y * 19349663);
			return (size_t)(ux ^ (uy << 1));
		}
	};

	static thread_local std::mt19937 s_rng{ std::random_device{}() };

	static inline std::vector<Int2> Neigh4(const Int2& p) {
		return { {p.x + 1, p.y}, {p.x - 1, p.y}, {p.x, p.y + 1}, {p.x, p.y - 1} };
	}

	static inline bool InArea(const Int2& p) {
		return (p.x >= (int)std::lround(g_BallAreaMin.x) && p.x <= (int)std::lround(g_BallAreaMax.x) &&
			p.y >= (int)std::lround(g_BallAreaMin.y) && p.y <= (int)std::lround(g_BallAreaMax.y));
	}

	static inline XMFLOAT3 ToPosXYPlane(const Int2& p) {
		return XMFLOAT3((float)p.x, (float)p.y, g_BallAreaMin.z);
	}

	static int Degree4(const std::unordered_set<Int2, Int2Hash>& S, const Int2& p) {
		int d = 0;
		for (auto nb : Neigh4(p)) if (S.find(nb) != S.end()) ++d;
		return d;
	}

	static void BoundingBox(const std::unordered_set<Int2, Int2Hash>& S,
		int& minX, int& maxX, int& minY, int& maxY)
	{
		auto it = S.begin();
		minX = maxX = it->x;
		minY = maxY = it->y;
		for (auto& p : S) {
			minX = std::min(minX, p.x); maxX = std::max(maxX, p.x);
			minY = std::min(minY, p.y); maxY = std::max(maxY, p.y);
		}
	}

	static bool IsFull2x3(const std::unordered_set<Int2, Int2Hash>& S)
	{
		if (S.size() != 6) return false;

		int minX, maxX, minY, maxY;
		BoundingBox(S, minX, maxX, minY, maxY);
		int w = maxX - minX + 1;
		int h = maxY - minY + 1;

		if (!((w == 3 && h == 2) || (w == 2 && h == 3))) return false;

		for (int y = minY; y <= maxY; ++y)
			for (int x = minX; x <= maxX; ++x)
				if (S.find({ x, y }) == S.end()) return false;

		return true;
	}

	static std::vector<std::vector<Int2>> ConnectedComponents4(const std::unordered_set<Int2, Int2Hash>& S)
	{
		std::unordered_set<Int2, Int2Hash> vis;
		std::vector<std::vector<Int2>> comps;

		for (auto& start : S) {
			if (vis.find(start) != vis.end()) continue;

			std::queue<Int2> q;
			q.push(start);
			vis.insert(start);

			std::vector<Int2> comp;
			while (!q.empty()) {
				Int2 cur = q.front(); q.pop();
				comp.push_back(cur);

				for (auto nb : Neigh4(cur)) {
					if (S.find(nb) == S.end()) continue;
					if (vis.insert(nb).second) q.push(nb);
				}
			}
			comps.push_back(std::move(comp));
		}
		return comps;
	}

	static std::vector<Int2> CollectCandidateEmptyNeighbors(const std::unordered_set<Int2, Int2Hash>& S)
	{
		std::unordered_set<Int2, Int2Hash> C;
		for (auto& p : S) {
			for (auto nb : Neigh4(p)) {
				if (S.find(nb) != S.end()) continue;
				if (!InArea(nb)) continue;
				C.insert(nb);
			}
		}
		return { C.begin(), C.end() };
	}

	static double ShapeScore(const std::unordered_set<Int2, Int2Hash>& S)
	{
		if (IsFull2x3(S)) return 1e18;

		int minX, maxX, minY, maxY;
		BoundingBox(S, minX, maxX, minY, maxY);
		int w = maxX - minX + 1;
		int h = maxY - minY + 1;

		double bboxArea = (double)w * (double)h;
		double aspectPenalty = (double)std::max(w, h) / (double)std::min(w, h);

		int endpoints = 0;
		int exposedEdges = 0;
		for (auto& p : S) {
			int d = Degree4(S, p);
			if (d == 1) endpoints++;
			exposedEdges += (4 - d);
		}

		double thinBarPenalty = 0.0;
		if (std::min(w, h) == 1 && std::max(w, h) >= 5) thinBarPenalty = 200.0;

		return  3.0 * bboxArea
			+ 20.0 * (aspectPenalty - 1.0)
			+ 10.0 * endpoints
			+ 1.0 * exposedEdges
			+ thinBarPenalty;
	}

	static Int2 RandomOriginInArea()
	{
		int minX = (int)std::lround(g_BallAreaMin.x);
		int maxX = (int)std::lround(g_BallAreaMax.x);
		int minY = (int)std::lround(g_BallAreaMin.y);
		int maxY = (int)std::lround(g_BallAreaMax.y);

		std::uniform_int_distribution<int> dx(minX, maxX);
		std::uniform_int_distribution<int> dy(minY, maxY);
		return { dx(s_rng), dy(s_rng) };
	}

	// 初始生成（带评分引导）
	static std::vector<Int2> Generate6Compact(const Int2& origin, std::mt19937& rng)
	{
		const int MaxTries = 200;

		auto pickWeighted = [&](const std::vector<Int2>& candidates,
			const std::vector<double>& scores)->Int2
			{
				const double k = 0.08;
				std::vector<double> w(candidates.size());
				double sum = 0.0;
				for (size_t i = 0; i < candidates.size(); ++i) {
					double s = scores[i];
					double wi = (s >= 1e17) ? 0.0 : std::exp(-k * s);
					w[i] = wi; sum += wi;
				}
				if (sum <= 1e-12) {
					std::uniform_int_distribution<int> dist(0, (int)candidates.size() - 1);
					return candidates[dist(rng)];
				}
				std::uniform_real_distribution<double> dist(0.0, sum);
				double r = dist(rng);
				for (size_t i = 0; i < candidates.size(); ++i) {
					r -= w[i];
					if (r <= 0) return candidates[i];
				}
				return candidates.back();
			};

		for (int attempt = 0; attempt < MaxTries; ++attempt) {
			std::unordered_set<Int2, Int2Hash> S;
			S.insert(origin);

			while (S.size() < 6) {
				auto candidates = CollectCandidateEmptyNeighbors(S);
				if (candidates.empty()) break;

				std::vector<double> scores;
				scores.reserve(candidates.size());
				for (auto& c : candidates) {
					auto T = S;
					T.insert(c);
					scores.push_back(ShapeScore(T));
				}

				Int2 chosen = pickWeighted(candidates, scores);
				S.insert(chosen);
			}

			if (S.size() == 6 && !IsFull2x3(S)) return { S.begin(), S.end() };
		}

		// 退化：随机扩张
		std::unordered_set<Int2, Int2Hash> S;
		S.insert(origin);
		while (S.size() < 6) {
			auto candidates = CollectCandidateEmptyNeighbors(S);
			if (candidates.empty()) break;
			std::uniform_int_distribution<int> dist(0, (int)candidates.size() - 1);
			S.insert(candidates[dist(rng)]);
		}
		return { S.begin(), S.end() };
	}

	// 从剩余 5 个点补 1 个新点（Top-K 随机 + 禁止原地复活）
	static Int2 RespawnOnePoint_TopK(
		const std::vector<Int2>& alive5,
		const Int2& killedPos,
		std::mt19937& rng)
	{
		std::unordered_set<Int2, Int2Hash> S5(alive5.begin(), alive5.end());
		auto comps = ConnectedComponents4(S5);
		auto candidates = CollectCandidateEmptyNeighbors(S5);

		// 禁止原地复活
		candidates.erase(
			std::remove_if(candidates.begin(), candidates.end(),
				[&](const Int2& c) { return c == killedPos; }),
			candidates.end());

		if (candidates.empty()) {
			// 没路走就重生成一组，然后取一个不在 S5 的点
			auto pts6 = Generate6Compact(*S5.begin(), rng);
			for (auto& p : pts6) if (S5.find(p) == S5.end()) return p;
			return pts6[0];
		}

		auto pickFromTopK = [&](const std::vector<Int2>& candList, int K)->Int2 {
			struct Item { Int2 p; double s; };
			std::vector<Item> v;
			v.reserve(candList.size());

			for (auto& c : candList) {
				auto T = S5;
				T.insert(c);
				v.push_back({ c, ShapeScore(T) });
			}

			std::sort(v.begin(), v.end(),
				[](const Item& a, const Item& b) { return a.s < b.s; });

			int k = std::min(K, (int)v.size());
			std::uniform_int_distribution<int> dist(0, k - 1);
			return v[dist(rng)].p;
			};

		if (comps.size() == 1) {
			return pickFromTopK(candidates, 5); // 你可调：3~6，越大越随机
		}

		// 断开：优先桥接
		std::unordered_map<Int2, int, Int2Hash> compId;
		for (int cid = 0; cid < (int)comps.size(); ++cid)
			for (auto& p : comps[cid]) compId[p] = cid;

		std::vector<Int2> bridging;
		for (auto& c : candidates) {
			bool touched[16] = { false };
			int cnt = 0;
			for (auto nb : Neigh4(c)) {
				auto it = compId.find(nb);
				if (it != compId.end()) {
					int id = it->second;
					if (!touched[id]) { touched[id] = true; cnt++; }
				}
			}
			if (cnt >= 2) bridging.push_back(c);
		}

		if (!bridging.empty()) return pickFromTopK(bridging, 3);

		// 无桥接：重生成取一个新点
		auto pts6 = Generate6Compact(*S5.begin(), rng);
		for (auto& p : pts6) if (S5.find(p) == S5.end()) return p;
		return pts6[0];
	}

	static void CreateBallsFromPoints(const std::vector<Int2>& pts)
	{
		for (int i = 0; i < MAX_BALL; ++i) {
			delete g_pBalls[i];
			g_pBalls[i] = nullptr;
		}

		g_BallCount = 0;
		for (auto& p : pts) {
			if (g_BallCount >= MAX_BALL) break;
			Ball* b = new Ball(ToPosXYPlane(p), 1.0f);
			b->SetModel(g_pBallModel);
			g_pBalls[g_BallCount++] = b;
		}
	}
}

void Ball_Initialize()
{
	if (!g_pBallModel) {
		g_pBallModel = ModelLoad("resource/model/ball.fbx", 0.005f, false);
	}

	Int2 origin = RandomOriginInArea();
	auto pts = Generate6Compact(origin, s_rng);
	CreateBallsFromPoints(pts);
}

void Ball_Finalize()
{
	for (int i = 0; i < MAX_BALL; ++i) {
		delete g_pBalls[i];
		g_pBalls[i] = nullptr;
	}
	g_BallCount = 0;

	// 若你的引擎有卸载接口，可在这里释放 g_pBallModel
	// ModelUnload(g_pBallModel);
	// g_pBallModel = nullptr;
}

Sphere Ball_GetSphere(int index)
{
	if (index < 0 || index >= g_BallCount || !g_pBalls[index]) {
		return { {0,0,0}, 0.0f };
	}
	return g_pBalls[index]->GetCollisionSphere();
}

Ball* Ball_GetBall(int index)
{
	if (index < 0 || index >= g_BallCount) return nullptr;
	return g_pBalls[index];
}

void Ball_UpdateAll(double elapsed_time)
{
	for (int i = 0; i < g_BallCount; ++i) {
		if (!g_pBalls[i]) continue;
		g_pBalls[i]->Update(elapsed_time);
	}

	// 一帧只处理一个击杀
	for (int killed = 0; killed < g_BallCount; ++killed) {
		if (!g_pBalls[killed]) continue;
		if (!g_pBalls[killed]->isDestroyed()) continue;

		// killed 的旧位置（整数格）
		Sphere ks = g_pBalls[killed]->GetCollisionSphere();
		Int2 killedPos{ (int)std::lround(ks.center.x), (int)std::lround(ks.center.y) };

		// 剩余 5 个点
		std::vector<Int2> alive5;
		alive5.reserve(5);
		for (int i = 0; i < g_BallCount; ++i) {
			if (i == killed) continue;
			Sphere s = g_pBalls[i]->GetCollisionSphere();
			alive5.push_back({ (int)std::lround(s.center.x), (int)std::lround(s.center.y) });
		}

		// 算一个新点，并只移动被击杀的那个球
		Int2 newP = RespawnOnePoint_TopK(alive5, killedPos, s_rng);

		g_pBalls[killed]->SetPosition(ToPosXYPlane(newP));
		g_pBalls[killed]->ResetHp(1);
		g_pBalls[killed]->SetModel(g_pBallModel);

		break;
	}
}

void Ball_DrawAll()
{
	for (int i = 0; i < g_BallCount; ++i) {
		if (!g_pBalls[i]) continue;
		g_pBalls[i]->Draw();
	}
}
