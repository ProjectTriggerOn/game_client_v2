#include "game.h"

#include "scene.h"
#include "collision_world.h"
#include "cube.h"
#include "map.h"
#include "shader.h"
#include "camera.h"
#include "direct3d.h"
#include "infinite_grid.h"
#include "keyboard.h"
#include "key_logger.h"
#include "mesh_field.h"
#include "sampler.h"
#include "light.h"
#include "model.h"
#include "model_ani.h"
#include "ms_logger.h"
#include "player_cam_tps.h"
#include "player_cam_fps.h"
#include "player_fps.h"
#include "i_network.h"
#include "mock_server.h"
#include "remote_player.h"
#include "input_producer.h"
#include "sky_dome.h"
#include "sprite.h"
#include "texture.h"
#include "fade.h"
#include "font.h"
#include "ui_widget.h"
#include "ui_manager.h"
#include "mouse.h"
#include <cwchar>
#include <vector>
#include <cstdio>
#include <cstring>
using namespace DirectX;

namespace{

	double g_AccumulatedTime = 0.0;
	MODEL* g_pModel = nullptr;
	MODEL_ANI* g_pModel0 = nullptr;
	int g_OverlayTexId = -1;  // white texture; reused to draw the native crosshair
#if defined(_DEBUG)
	bool isDebugCam = false;
	bool isDebugCollision = false;
#else
	constexpr bool isDebugCam = false;
	constexpr bool isDebugCollision = false;
#endif
	PlayerFps* g_PlayerFps;
	GameState g_GameState;
	
	// Correction state for debug display
	const char* g_CorrectionMode = "NONE";
	float g_CorrectionError = 0.0f;

	// Collision world
	CollisionWorld g_CollisionWorld;

	// ========================================================================
	// Lag compensation: view-tick tracking
	//
	// Client clock driving remote-player interpolation (advanced every frame
	// in Game_Update). File-scope so Game_GetViewTick() can map the current
	// interp-delayed render time back into server-tick space.
	// ========================================================================
	double g_ClientClock = 0.0;

	constexpr double SERVER_TICK_RATE = 32.0;  // matches PlayerFps/GameServer TICK_RATE (private there)

	// (receiveTime, serverTick) pairs for snapshots, same cadence as the
	// RemotePlayer snapshot buffers — lets us compute which (fractional)
	// server tick the interp-delayed render time corresponds to.
	struct TickTimeEntry
	{
		double receiveTime;
		uint32_t serverTick;
	};
	std::vector<TickTimeEntry> g_TickTimeBuffer;
	constexpr size_t TICK_TIME_BUFFER_MAX = 32;

	// ========================================================================
	// Scoring: latest snapshot cache + kill-feed dedup + scoreboard toggle
	// ========================================================================
	Snapshot g_LastSnapshot{};
	bool     g_HasSnapshot = false;
	uint32_t g_LastShownKillSeq = 0;   // highest kill seq already pushed to the feed
	bool     g_ScoreboardShown = false;

	// Append {"id":I,"k":K,"d":D,"me":bool} rows for one team into a bounded
	// buffer; returns chars written. Iterates localPlayer (under localPlayerTeam)
	// then the clamped remotePlayers[], splitting by teamId.
	int AppendRosterTeam(const Snapshot& s, uint8_t team, char* out, int cap)
	{
		int n = 0;
		bool first = true;
		auto emit = [&](uint8_t id, uint16_t k, uint16_t d) {
			int avail = (cap > n) ? cap - n : 0;
			n += snprintf(out + n, avail,
				"%s{\"id\":%u,\"k\":%u,\"d\":%u,\"me\":%s}",
				first ? "" : ",", id, k, d,
				(id == s.localPlayerId) ? "true" : "false");
			first = false;
		};
		if (s.localPlayerTeam == team)
			emit(s.localPlayerId, s.localPlayer.kills, s.localPlayer.deaths);
		const uint8_t rc = (s.remotePlayerCount < (MAX_PLAYERS - 1))
			? s.remotePlayerCount : static_cast<uint8_t>(MAX_PLAYERS - 1);
		for (uint8_t i = 0; i < rc; ++i)
			if (s.remotePlayers[i].teamId == team)
				emit(s.remotePlayers[i].playerId,
					s.remotePlayers[i].state.kills,
					s.remotePlayers[i].state.deaths);
		return n;
	}

	void BuildScoreboardJson(const Snapshot& s, char* out, int cap)
	{
		int n = snprintf(out, cap, "{\"red\":[");
		n += AppendRosterTeam(s, PlayerTeam::RED, out + n, cap - n);
		n += snprintf(out + n, cap - n, "],\"blue\":[");
		n += AppendRosterTeam(s, PlayerTeam::BLUE, out + n, cap - n);
		snprintf(out + n, cap - n, "]}");
	}

	void BuildResultJson(const Snapshot& s, char* out, int cap)
	{
		int n = snprintf(out, cap,
			"{\"winner\":%u,\"red\":%u,\"blue\":%u,\"scoreboard\":",
			s.winningTeam, s.redScore, s.blueScore);
		BuildScoreboardJson(s, out + n, cap - n);
		int len = static_cast<int>(strlen(out));
		snprintf(out + len, cap - len, "}");
	}
}

// Global network debug info (populated from received snapshots)
NetworkDebugInfo g_NetDebugInfo;

void Game_Initialize()
{
	// Entering the game scene means we're playing. The UI page (HUD) and input
	// level are derived from this by UIPolicy_Apply (ui_policy.h).
	g_GameState = PLAY;

	g_OverlayTexId   = Texture_LoadFromFile(L"resource/texture/white.png");
	Font_Initialize();
	Widget_Initialize();
	Map_Initialize();
	Map_RegisterColliders(g_CollisionWorld);

	SkyDome_Initialize(Map_GetSkyAsset());
	g_PlayerFps = new PlayerFps();
	g_PlayerFps->Initialize({ -7.0f, 0.0f, -7.0f }, { 0.0f, 0.0f, 1.0f }, &g_CollisionWorld);

	Camera_Initialize();
	PlayerCamTps_Initialize();
	PlayerCamFps_Initialize();
	PlayerCamFps_SetInvertY(true);
	// Mouse mode is owned by MousePolicy_Apply (mouse_policy.h) — no direct calls here
	Fade_Initialize();

	// Mock mode: re-arm the authoritative session so each game-scene entry (PLAY
	// from the title, or Restart_Game) starts a FRESH match instead of resuming
	// the frozen state of the previous round. ENet modes own the match
	// server-side (g_pMockServer == nullptr) and are left untouched. On the very
	// first boot into the game scene the mock server isn't created yet (still
	// nullptr); main() initializes it fresh right after, so the guard skips it.
	extern MockServer* g_pMockServer;
	if (g_pMockServer) g_pMockServer->ResetSession();

	// Reset client-side scoring trackers so a fresh match starts clean (the
	// scoreboard overlay hidden, kill-feed dedup re-zeroed).
	g_HasSnapshot = false;
	g_LastShownKillSeq = 0;
	g_ScoreboardShown = false;
	UI::PushScoreboardVisible(false);
}

bool Game_WantsUICursor()
{
	// Free cursor for the debug TPS camera. PAUSE/SETTING are handled via
	// UI::IsModalActive() in MousePolicy_Apply (they set InteractiveLevel::Modal),
	// so they don't need to be listed here.
	return isDebugCam;
}

bool Game_IsGameplayActive()
{
	// Gameplay input/simulation runs only while actually playing in the game
	// scene. Consumed by InputProducer (zero input otherwise) and HUD push.
	return g_GameState == PLAY;
}

void Game_Update(double elapsed_time)
{
	// ========================================================================
	// ESC: single owner of the pause/settings toggle.
	//
	// Only gameplay handles ESC — the UI menus never bind it — so there is no
	// dual-consumer ping-pong and the docs §5.3 frame-start modal snapshot is
	// unnecessary. State flips only; cursor mode, UI input level and the active
	// page all derive from GameState (mouse_policy.h / ui_policy.h).
	// ========================================================================
	// Ignore ESC while a masked scene transition is running: the loading curtain is
	// up and gameplay is frozen, so a PAUSE<->PLAY flip here would re-enable local
	// prediction behind the curtain (e.g. ESC during a return-to-title transition).
	if (!SceneTransition_IsActive() && KeyLogger_IsTrigger(KK_ESCAPE))
	{
		switch (g_GameState)
		{
		case PLAY:    g_GameState = PAUSE; break;
		case PAUSE:   g_GameState = PLAY;  break;
		case SETTING: g_GameState = PAUSE; break;  // ESC backs out of settings to pause
		default: break;
		}
	}

	// Pause / settings freezes the LOCAL player only. We deliberately do NOT
	// early-return: the network must keep being drained below or the server's
	// snapshot stream backs up (unbounded memory growth), and remote players
	// must keep interpolating — an MP pause is a menu overlay, not a world
	// freeze for everyone else.
	//
	// PlayerFps::Update reads keyboard/mouse directly (jump/fire/reload), so it
	// and the camera only run while actually playing; InputProducer already
	// sends zero input when paused, so the server keeps the local player still.
	// Freeze local prediction (PlayerFps::Update reads input directly) while a
	// masked transition's curtain is up, matching the InputProducer gate — the
	// player must not move/fire behind the black loading screen.
	const bool gameplayActive = (g_GameState == PLAY) && !SceneTransition_IsActive();

	if (gameplayActive)
	{
#if defined(_DEBUG)
		if (KeyLogger_IsTrigger(KK_C))  isDebugCam = !isDebugCam;
		if (KeyLogger_IsTrigger(KK_F1)) isDebugCollision = !isDebugCollision;
#endif

		g_PlayerFps->Update(elapsed_time);

		// Push live HUD data to the UI (C++ → JS).
		UI::PushHealth(g_PlayerFps->GetHealth(), 200);
		UI::PushAmmo(g_PlayerFps->GetAmmo(), g_PlayerFps->GetAmmoReserve());
	}

	// ========================================================================
	// Consume Snapshots from Network (works with both Mock and ENet)
	//
	// Local player uses client-side prediction - NO interpolation (causes lag).
	// Correction is handled via render offset inside PlayerFps.
	// ========================================================================
	extern INetwork* g_pNetwork;
	extern RemotePlayer g_RemotePlayers[];
	extern bool g_RemotePlayerActive[];
	extern InputProducer* g_pInputProducer;

	// Increment client clock EVERY FRAME for smooth interpolation
	g_ClientClock += elapsed_time;

	Snapshot snap;
	while (g_pNetwork && g_pNetwork->ReceiveSnapshot(snap))
	{
		// Local-player reconciliation only while playing — frozen when paused
		// (we still drain the snapshot below for remote players / no backlog).
		if (gameplayActive)
		{
			g_PlayerFps->ApplyServerCorrection(snap.localPlayer);
			g_PlayerFps->SetTeam(snap.localPlayerTeam);

			// Feed server state to InputProducer (for jump-pending logic)
			if (g_pInputProducer)
			{
				g_pInputProducer->SetLastServerState(snap.localPlayer);
			}
		}

		// Dispatch remote players from snapshot.
		// Clamp remotePlayerCount to the array bound — a malicious/corrupted
		// snapshot with count > MAX_PLAYERS-1 would otherwise OOB-read past
		// the Snapshot struct.
		const uint8_t remoteCount = (snap.remotePlayerCount < (MAX_PLAYERS - 1))
			? snap.remotePlayerCount
			: static_cast<uint8_t>(MAX_PLAYERS - 1);
		bool seenThisSnap[MAX_PLAYERS] = {};
		for (uint8_t i = 0; i < remoteCount; i++)
		{
			uint8_t rid = snap.remotePlayers[i].playerId;
			if (rid < MAX_PLAYERS)
			{
				seenThisSnap[rid] = true;
				g_RemotePlayerActive[rid] = true;
				g_RemotePlayers[rid].SetActive(true);
				g_RemotePlayers[rid].SetTeam(snap.remotePlayers[i].teamId);
				g_RemotePlayers[rid].PushSnapshot(snap.remotePlayers[i].state, g_ClientClock);
			}
		}
		// Deactivate players absent from this snapshot (disconnected)
		for (int i = 0; i < MAX_PLAYERS; i++)
		{
			if (g_RemotePlayerActive[i] && !seenThisSnap[i])
			{
				g_RemotePlayerActive[i] = false;
				g_RemotePlayers[i].SetActive(false);
			}
		}

		// Cache for debug display
		g_NetDebugInfo.tickDelta = snap.tickId - g_NetDebugInfo.prevServerTick;
		g_NetDebugInfo.prevServerTick = snap.tickId;
		g_NetDebugInfo.lastServerTick = snap.tickId;
		g_NetDebugInfo.lastServerTime = snap.serverTime;
		g_NetDebugInfo.lastServerState = snap.localPlayer;
		g_NetDebugInfo.hasData = true;
		g_NetDebugInfo.snapshotsThisSecond++;

		// Record (receiveTime, serverTick) for lag-comp view-tick computation.
		// A backward tick means the server restarted — stale mapping is invalid.
		if (!g_TickTimeBuffer.empty() && snap.tickId < g_TickTimeBuffer.back().serverTick)
		{
			g_TickTimeBuffer.clear();
		}
		g_TickTimeBuffer.push_back({ g_ClientClock, snap.tickId });
		if (g_TickTimeBuffer.size() > TICK_TIME_BUFFER_MAX)
		{
			g_TickTimeBuffer.erase(g_TickTimeBuffer.begin());
		}

		// --- Scoring ---------------------------------------------------------
		g_LastSnapshot = snap;
		g_HasSnapshot = true;

		// Live HUD: team scores + match timer (dirty-flag dedups to 1 call/frame)
		UI::PushScores(snap.redScore, snap.blueScore);
		UI::PushMatchTimer(snap.matchTimeRemaining);

		// Kill feed: replay new ring entries in (lastShown, latest], clamped to
		// the ring window so a reset or dropped events can't loop/underflow.
		uint32_t latest = snap.latestKillSeq;
		if (latest < g_LastShownKillSeq) g_LastShownKillSeq = latest;  // match/server reset
		uint32_t startSeq = g_LastShownKillSeq;
		if (latest > KILL_FEED_SIZE && startSeq < latest - KILL_FEED_SIZE)
			startSeq = latest - KILL_FEED_SIZE;
		for (uint32_t k = startSeq; k < latest; ++k)
		{
			const KillFeedEntry& e = snap.recentKills[k % KILL_FEED_SIZE];
			UI::PushKillFeed(e.killerId, e.victimId, e.killerTeam, e.victimTeam);
		}
		g_LastShownKillSeq = latest;

		// Match end -> RESULT. Push the result payload first so the page is
		// populated when UIPolicy_Apply shows it next frame.
		if (snap.matchState == MatchState::ENDED && g_GameState != RESULT)
		{
			char rjson[1536];
			BuildResultJson(snap, rjson, sizeof(rjson));
			UI::PushMatchResult(rjson);
			g_GameState = RESULT;
		}
	}

	// Tab scoreboard: an overlay WITHIN the hud page (Display level — no cursor
	// unlock). Driven by held key state; refresh live K/D while held.
	const bool wantScoreboard = gameplayActive && KeyLogger_IsPressed(KK_TAB);
	if (wantScoreboard != g_ScoreboardShown)
	{
		g_ScoreboardShown = wantScoreboard;
		UI::PushScoreboardVisible(wantScoreboard);
	}
	if (wantScoreboard && g_HasSnapshot)
	{
		char sjson[1536];
		BuildScoreboardJson(g_LastSnapshot, sjson, sizeof(sjson));
		UI::PushScoreboard(sjson);
	}

	// Update snapshot receive rate (once per second)
	g_NetDebugInfo.snapshotRateTimer += elapsed_time;
	if (g_NetDebugInfo.snapshotRateTimer >= 1.0)
	{
		g_NetDebugInfo.snapshotsPerSecond = g_NetDebugInfo.snapshotsThisSecond;
		g_NetDebugInfo.snapshotsThisSecond = 0;
		g_NetDebugInfo.snapshotRateTimer -= 1.0;
	}

	// Update debug info from player correction
	g_CorrectionMode = g_PlayerFps->GetCorrectionMode();
	g_CorrectionError = g_PlayerFps->GetCorrectionError();

	SkyDome_SetPosition(g_PlayerFps->GetPosition());

	// Camera follows the local player — frozen while paused (player isn't moving
	// and modal mouse delta is zero anyway).
	if (gameplayActive)
	{
		if (isDebugCam)
		{
			PlayerCamTps_Update_Maya(elapsed_time, g_PlayerFps->GetPosition());
		}
		else {
			PlayerCamFps_Update(elapsed_time, g_PlayerFps->GetEyePosition());
		}
	}

	// (KK_U manual MSLogger UI-mode toggle removed — MousePolicy_Apply now owns
	//  that flag and would override the toggle on the next frame anyway)

	// Update all active RemotePlayer instances (every frame for smooth interpolation)
	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		if (g_RemotePlayerActive[i])
			g_RemotePlayers[i].Update(elapsed_time, g_ClientClock);
	}

	Fade_Update(elapsed_time);
}

void Game_Draw()
{
	Sampler_SetFilterAnisotropic();

	// Ambient comes from the loaded map's MapEnv when one is authored.
	// default.map ships with no env block (visualSize == 0), so fall back to
	// the historical hardcoded ambient to avoid a visible dark regression.
	if (Map_HasEnvironment()) {
		Light_SetAmbient(Map_GetAmbient());
	} else {
		Light_SetAmbient({ 0.5f, 0.5f, 0.5f });
	}

	XMFLOAT4X4 mtxView = isDebugCam ? PlayerCamTps_GetViewMatrix() : PlayerCamFps_GetViewMatrix();
	XMFLOAT4X4 mtxProj = isDebugCam ? PlayerCamTps_GetPerspectiveMatrix() : PlayerCamFps_GetProjectMatrix();
	XMMATRIX view = XMLoadFloat4x4(&mtxView);
	XMMATRIX proj = XMLoadFloat4x4(&mtxProj);
	XMFLOAT3 cam_pos = isDebugCam ? PlayerCamTps_GetPosition() : PlayerCamFps_GetPosition();

	Camera_SetMatrixToShader(view, proj);

	XMVECTOR v{ 0.0f, -1.0f, 0.0f };
	v = XMVector3Normalize(v);
	XMFLOAT4 dir;
	XMStoreFloat4(&dir, v);
	Light_SetDirectionalWorld(dir, { 1.0f, 1.0f, 1.0f, 1.0f });

	Light_SetSpecularWorld(cam_pos, 4.0f, { 0.3f, 0.3f, 0.3f, 1.0f });

	// Fog comes from the map env.  The shader gate is (fogEnd > fogStart), so
	// we pass the values through as-is and only mark the pipeline dirty when
	// the loaded map actually authored them.
	if (Map_HasEnvironment()) {
		Light_SetFog(true, Map_GetFogColor(), Map_GetFogStart(), Map_GetFogEnd());
	} else {
		Light_SetFog(false, { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f);
	}

	// Single upload point for all lighting cbuffers (see light.cpp).
	Light_Flush();

	SkyDome_Draw();

	g_PlayerFps->Draw();

	// Draw all active Remote Players
	extern RemotePlayer g_RemotePlayers[];
	extern bool g_RemotePlayerActive[];
	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		if (g_RemotePlayerActive[i])
			g_RemotePlayers[i].Draw();
	}

	Cube_SetUVMode(CUBE_UV_PER_FACE);
	Map_Draw();

	// Debug draw: collision shapes (F3 toggle)
	if (isDebugCollision)
	{
		// Disable depth test so outlines are visible through map objects
		Direct3D_SetDepthEnable(false);

		Collision_DebugSetViewProj(view * proj);

		// Draw local player capsule (green)
		Capsule playerCapsule = g_PlayerFps->GetCapsule();
		Collision_DebugDraw(playerCapsule, { 0.0f, 1.0f, 0.0f, 1.0f });

		// Draw remote player capsules (red)
		for (int i = 0; i < MAX_PLAYERS; i++)
		{
			if (g_RemotePlayerActive[i] && !g_RemotePlayers[i].IsDead())
			{
				XMFLOAT3 rpos = g_RemotePlayers[i].GetRenderPosition();
				float height = g_RemotePlayers[i].GetHeight();
				float radius = 0.3f;
				Capsule remoteCapsule;
				remoteCapsule.pointA = { rpos.x, rpos.y + radius, rpos.z };
				remoteCapsule.pointB = { rpos.x, rpos.y + height - radius, rpos.z };
				remoteCapsule.radius = radius;
				Collision_DebugDraw(remoteCapsule, { 1.0f, 0.2f, 0.2f, 1.0f });
			}
		}

		// Draw all world AABB colliders
		for (const auto& collider : g_CollisionWorld.GetColliders())
		{
			XMFLOAT4 color = collider.isGround
				? XMFLOAT4{ 0.0f, 0.5f, 1.0f, 1.0f }   // blue for ground
				: XMFLOAT4{ 1.0f, 0.5f, 0.0f, 1.0f };   // orange for walls
			Collision_DebugDraw(collider.aabb, color);
		}

		// Draw shooting ray (yellow)
		XMFLOAT3 eyePos = g_PlayerFps->GetEyePosition();
		XMFLOAT3 fwd = { mtxView._13, mtxView._23, mtxView._33 };
		float rayLen = 200.0f;
		XMFLOAT3 rayEnd = {
			eyePos.x + fwd.x * rayLen,
			eyePos.y + fwd.y * rayLen,
			eyePos.z + fwd.z * rayLen
		};
		Collision_DebugDrawLine(eyePos, rayEnd, { 1.0f, 1.0f, 0.0f, 1.0f });

		// Restore depth test
		Direct3D_SetDepthEnable(true);

		// Restore 2D ortho projection after debug draw overwrote CB0
		Sprite_Begin();

		PlayerCamFps_Debug(*g_PlayerFps);
	}

	float sw = (float)Direct3D_GetBackBufferWidth();
	float sh = (float)Direct3D_GetBackBufferHeight();

	Direct3D_SetDepthEnable(false);

	// Crosshair — native green cross, centered. Drawn natively (not in
	// Ultralight) because the crosshair is frame-locked and pixel-exact; see
	// docs §12.1. Placeholder static cross for now; a spread-driven dynamic
	// crosshair + hitmarker will replace this in the same 2D pass.
	if (g_GameState == PLAY)
	{
		const float cx = sw * 0.5f;
		const float cy = sh * 0.5f;
		constexpr float ARM = 10.0f;   // arm half-length (px)
		constexpr float TH  = 2.0f;    // line thickness (px)
		const XMFLOAT4 GREEN = { 0.1f, 1.0f, 0.1f, 0.9f };
		// horizontal bar
		Sprite_Draw(g_OverlayTexId, cx - ARM, cy - TH * 0.5f, ARM * 2.0f, TH, GREEN);
		// vertical bar
		Sprite_Draw(g_OverlayTexId, cx - TH * 0.5f, cy - ARM, TH, ARM * 2.0f, GREEN);
	}

	// NOTE: the legacy Widget_* HUD panels (HP/Ammo) and the immediate-mode
	// settings overlay (sensitivity slider + "Exit Game") used to be drawn here.
	// They are superseded by the Ultralight HUD/settings pages (Slice D/E) and
	// were removed — the old "Exit Game" button's hit-rect overlapped the new
	// settings checkboxes and quit the app on click (PostQuitMessage).
	//
	// The legacy arr.png crosshair sprite and the software cursor (g_CursorTexId,
	// shown in MSLogger UI mode) were also removed: the OS cursor (MousePolicy
	// makes it visible in menus) replaces the software cursor, and the native
	// green cross above replaces the sprite crosshair. The Ultralight HUD's CSS
	// crosshair was removed too — one crosshair, drawn natively.

	// Fade overlay (depth off so it covers everything including crosshair)
	Fade_Draw();

	Direct3D_SetDepthEnable(true);
}

void Game_Finalize()
{
	Camera_Finalize();
	PlayerCamTps_Finalize();
	PlayerCamFps_Finalize();
	// Game_Initialize new's a fresh PlayerFps every game-scene entry; delete it
	// here so the now-reachable game->title->game cycle doesn't leak one per round.
	// (Only Game_IsPlayerInputLocked derefs it outside the game scene, and that is
	// null-guarded.)
	g_PlayerFps->Finalize();
	delete g_PlayerFps;
	g_PlayerFps = nullptr;
	ModelAni_Release(g_pModel0);
	Map_Finalize();
	Font_Finalize();
	Widget_Finalize();
	// Clear any active death/respawn fade so a full-screen tint can't persist onto
	// the next scene (e.g. quitting to title while dead). Covers a fade that
	// started after SceneTransition_To's reset, during the cover phase.
	Fade_Reset();
	Fade_Finalize();
}

void Game_SetState(GameState state)
{
	g_GameState = state;
}

GameState Game_GetState()
{
	return g_GameState;
}

const char* Game_GetCorrectionMode()
{
	return g_CorrectionMode;
}

float Game_GetCorrectionError()
{
	return g_CorrectionError;
}

CollisionWorld* Game_GetCollisionWorld()
{
	return &g_CollisionWorld;
}

uint32_t Game_GetClientTick()
{
	return g_PlayerFps ? g_PlayerFps->GetClientTick() : 0;
}

bool Game_IsPlayerInputLocked()
{
	return g_PlayerFps ? g_PlayerFps->IsInputLocked() : false;
}

void Game_GetViewTick(uint32_t& outTick, float& outFrac)
{
	// Map the interp-delayed render time (what the player actually SEES of
	// remote players) back into server-tick space, mirroring the bracketing
	// rules of RemotePlayer::Update so the reported tick matches the render.
	outTick = 0;  // sentinel: no data, server must not rewind
	outFrac = 0.0f;
	if (g_TickTimeBuffer.empty()) return;

	const double renderTime = g_ClientClock - RemotePlayer::INTERPOLATION_DELAY;

	// INTERP: find consecutive pair with t0 <= renderTime < t1.
	// Pairs with t0 >= t1 (snapshots drained in the same frame share one
	// receiveTime) are skipped — same rule as RemotePlayer::Update.
	for (size_t i = 0; i + 1 < g_TickTimeBuffer.size(); ++i)
	{
		const TickTimeEntry& a = g_TickTimeBuffer[i];
		const TickTimeEntry& b = g_TickTimeBuffer[i + 1];
		if (a.receiveTime >= b.receiveTime) continue;
		if (a.receiveTime <= renderTime && renderTime < b.receiveTime)
		{
			const double alpha = (renderTime - a.receiveTime) / (b.receiveTime - a.receiveTime);
			// Lerp in TICK space — b.serverTick - a.serverTick may be >1 on drops,
			// matching the position lerp RemotePlayer does across the same pair.
			const double fracTick = static_cast<double>(a.serverTick)
				+ static_cast<double>(b.serverTick - a.serverTick) * alpha;
			outTick = static_cast<uint32_t>(fracTick);
			outFrac = static_cast<float>(fracTick - static_cast<double>(outTick));
			return;
		}
	}

	// EXTRAP / SNAP: renderTime past the newest snapshot
	const TickTimeEntry& newest = g_TickTimeBuffer.back();
	if (renderTime >= newest.receiveTime)
	{
		double ahead = renderTime - newest.receiveTime;
		// Beyond the extrapolation limit RemotePlayer SNAPs to the newest
		// snapshot — report exactly that tick instead of extrapolating on.
		if (ahead > RemotePlayer::MAX_EXTRAPOLATION_TIME) ahead = 0.0;
		const double fracTick = static_cast<double>(newest.serverTick) + ahead * SERVER_TICK_RATE;
		outTick = static_cast<uint32_t>(fracTick);
		outFrac = static_cast<float>(fracTick - static_cast<double>(outTick));
	}
	// renderTime < oldest (WAIT mode) → outTick stays 0 (no compensation)
}

