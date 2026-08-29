# 设计：粒子系统底座 + 弹孔贴花（含命中火花）

- 日期：2026-08-29
- 状态：已评审通过（对话评审 2026-08-29），待转实现计划
- 仓库：`game_client`（分支 `feat/particles`，worktree `.worktrees/client-particles`）
- 来源：LPSP_d3d11 仓库 `particle_sys` 分支（tip `3a3c1f7`）移植 + 适配；两仓库历史不相关，只能拷文件适配，无法 cherry-pick

---

## 1. 需求确认记录（2026-08-29）

| 决策点 | 结论 |
|---|---|
| 移植形态 | **可扩展**：数据驱动的发射配置（`ParticleConfig` 注册表），不引入 LPSP 原版虚函数继承树（Particle/Emitter 基类） |
| 死代码 | 删除 `Graphics/effect.cpp/h` + vcxproj 两条目。**`sprite_anime` 保留**（`main.cpp` 主循环在用：Initialize/Update/Finalize） |
| 第一批效果 | 弹孔贴花 + 命中火花粒子（贴花与粒子技术路径不同，分别独立模块） |
| 触发来源 | **仅本地玩家射击**（轮询 `fireCounter` 变化）；机器人/远端弹孔属后续批次 |
| 贴花容量 | 128 个固定池，环形覆写最老贴花 |
| 贴花寿命 | 永久，直到被池环形挤掉（无定时淡出） |
| 验证 | standalone 单测（`Game/tests/` 惯例，不进 vcxproj）+ F8 调试面板计数 + 游戏内肉眼验证 |
| 贴图来源 | LPSP Unity 包贴图拷入 `resource/texture/`（仓库已有 LPSP 资产先例：`lpsp_tpc_*.fbx`、`ak_002.fbx`） |

## 2. 架构总览

```
【基础设施】
  Core/direct3d       +第三个深度状态：测试开、写入关（照 LPSP direct3d.cpp 移植）
                      + Direct3D_SetDepthWriteEnable(bool)
                      （与现有 Direct3D_SetDepthEnable 并排，直接改 direct3d.cpp/h）

【渲染底座（Graphics/）】
  billboard.cpp/h     单位四边形 + Billboard_Draw(texId, pos, scale, pivot, color)
  shader_billboard    专用 shader：b0 world / b1 view / b2 proj / b3 uv变换
                      PS b0: 颜色 tint（最终色 = 纹理 × 顶点色 × tint）
  shader_pixel_billboard.hlsl / shader_vertex_billboard.hlsl  → Shaders/ 目录

【游戏逻辑（Graphics/）】
  particle.cpp/h      POD 粒子池（1024 固定静态数组）+ ParticleConfig 注册表
  decal.cpp/h         弹孔贴花池（128 固定环形），复用 billboard shader

【业务粘合（Game/）】
  impact_fx.cpp/h     轮询 fireCounter → 射线求交(带法线) → Decal_Create + Particle_Emit
  collision_world     + Collision_RayAABB(origin, dir, aabb, outT, outNormal)
```

模块边界：`billboard`/`shader_billboard` 不懂游戏只画四边形；`particle`/`decal` 不懂射击只管池生命周期；`impact_fx` 是唯一知道"开枪→命中→该生成什么"的模块。

## 3. 模块规格

### 3.1 billboard（移植自 LPSP，做相机适配）

- 单位四边形顶点缓冲（4 顶点 triangle strip：`{posL:xyz, color:rgba, uv:xy}`），照抄 LPSP `billboard.cpp`
- 接口：
  - `Billboard_Initialize(device, context)` / `Billboard_Finalize()`
  - `Billboard_SetCamera(const XMMATRIX& view)` —— 每帧由 `Game_Draw` 注入当前激活相机 view（FPS/TPS 切换自动正确）。**与 LPSP 的差异**：LPSP 写死 `PlayerCamFps_GetWorldMatrix()`，TriggerOn 的 billboard 不直接依赖任何相机模块，内部取 view 转置得相机基向量
  - `Billboard_Draw(texID, pos, scale, pivot, color)` —— 世界朝向矩阵 = `pivot偏移 × scale × 相机朝向 × 平移`
- YAGNI 砍掉 LPSP 的：Laser_Billboard_Draw、UV 切分、`tex_cut` 变体重载

### 3.2 shader_billboard（照抄 LPSP + 工程适配）

- hlsl 放 `Shaders/`，进 vcxproj `FxCompile`（现有 .cso 构建后 xcopy 管线自动产出到 `resource/shader/`）
- VS：`cbuffer b0 world / b1 view / b2 proj / b3 {scale, translation}`（UV 变换），结构体/寄存器布局照抄 LPSP
- PS：`b0 g_Color` tint，`outColor = tex × 顶点色 × g_Color`
- cpp 接口照 LPSP：`Shader_Billboard_Initialize/SetWorldMatrix/SetViewMatrix/SetProjectMatrix/SetColor/SetUVParameter/Begin/Finalize`

### 3.3 particle（可扩展核心：数据驱动配置注册）

- 池：`Particle` POD × 1024 静态数组（对齐仓库 C 风格：静态池 + 句柄，日文注释，tab 缩进）
- 字段：`position/velocity/color(XMFLOAT4)/age/lifeTime/size/sizeGrowRate/gravity/damping/active/configId`
- `ParticleConfig`（数据结构，表达 LPSP SparkEmitter/SmokeEmitter 的随机参数）：
  - 寿命区间 `[lifeMin, lifeMax]`、初速：基准方向 + 扇形张角 `spreadAngle` + 速度区间 `[speedMin, speedMax]`（水平面内均匀采样方向，垂直速度独立区间——沿用 SparkEmitter 语义）
  - 尺寸区间 `[sizeMin, sizeMax]`、`sizeGrowRate`、重力加速度、阻尼系数、颜色（tint）、alpha 淡出开关
  - `textureId`（创建时由调用方传入已加载贴图）
- 接口：
  - `Particle_Initialize()` / `Finalize()` / `Update(dt)` / `Draw()`
  - `int Particle_RegisterConfig(const ParticleConfig&)` 返回 configId（仿 `SpriteAnime_PatternRegister`，满了返回 -1）
  - `void Particle_Emit(int configId, const XMFLOAT3& pos, int count)` —— 每粒子按 config 随机化参数，从池取空槽；池满则本次 emit 部分或全部丢弃（不覆盖存活粒子）
- Update 逻辑（照 LPSP）：age 累计 → 超寿命失活；`pos += vel*dt`；`vel.y -= gravity*dt`；`vel *= (1-damping*dt)`；`size += sizeGrowRate*dt`；alpha = `1 - age/lifeTime`（淡出开时）
- Draw：遍历激活粒子逐个 `Billboard_Draw`（1024 上限下不做合批——YAGNI）
- 内部随机：`RandomFloat(min,max)`（LPSP 简版同款，`static` 化）

### 3.4 decal（弹孔贴花，独立于粒子）

- 池：128 × `{position, worldMatrix}` + 环形游标 `g_DecalCursor`；写入第 N 个即 `cursor = N % 128` 覆写最老
- 朝向：命中法线张成正切基（法线为主轴，任选稳定副轴）→ world 矩阵；沿法线抬 0.01m 防 z-fighting
- 创建：`void Decal_Create(const XMFLOAT3& hitPos, const XMFLOAT3& normal)`（贴图/尺寸模块内常量：`bullet_hole.png`，0.15m 见方）
- Draw：深度测试开、写入关，逐个用 billboard shader 画（world 用 decal 自身矩阵，不用相机朝向——贴花贴在表面，不面向相机）
- 无 Update 逻辑（永久存活、无动画），但仍提供 `Decal_Initialize/Finalize/Draw` 四件套，无 Update

### 3.5 direct3d 深度状态扩展

- `Core/direct3d.cpp` 现有 `g_pDepthStencilStateDepthDisable/Enable` 两个状态（第 21-22 行）→ 照 LPSP 直接加第三个 `g_pDepthStencilStateDepthNoWrite`（DepthEnable=TRUE, DepthWriteMask=ZERO）
- 新 API：`void Direct3D_SetDepthWriteEnable(bool enable)`；约定：该函数只切写入位，`Direct3D_SetDepthEnable` 保持原语义不动
- 影响：只加不改，现有调用零变化

### 3.6 collision_world 射线求交（带法线）

- 新增自由函数 `bool Collision_RayAABB(const XMFLOAT3& origin, const XMFLOAT3& dir, const AABB& box, float& outT, XMFLOAT3& outNormal)`
- slab 实现吸收 `mock_server.cpp:639 RayAABB`（静态函数，保持原样不动），加法线输出：tMin 命中的轴向 ± 法线
- 放 `Game/collision_world.cpp/h`（独立于 CollisionWorld 类，因为命中判定调用方既有类内也有类外场景）
- **单测目标**：轴向平行、射线起点在盒内、法线方向正确性

### 3.7 impact_fx（业务粘合：射弹孔 + 火花）

- `ImpactFx_Initialize`：加载贴图 `bullet_hole.png` + `sparks.png`，注册火花 ParticleConfig
- 每帧 `ImpactFx_Update`：轮询本地玩家 fireCounter（mock server 确认本地射击最终走 `m_FireCounter++`，`mock_server.cpp:719`；`g_PlayerFps->GetFireCounter()` 同步于快照校正——实现时以 game.cpp 实际可轮询到的计数器为准，若 GetFireCounter 不随本地开火即时变化则改轮询 player_fps 的开火事件点）
  - 计数变化 → 构造射线：`g_PlayerFps->GetEyePosition()` + yaw/pitch 方向（照 `mock_server.cpp:731` rayDir 公式）
  - 对 `g_CollisionWorld.GetColliders()` 全量求交取最近 hit → `Decal_Create(hitPos, normal)` + `Particle_Emit(sparkConfigId, hitPos, N)`
- 常量：每命中火花 12 颗（可调）
- `ImpactFx_DebugInfo(int& decalCount, int& particleCount)` —— 给 F8 面板
- `Game_Draw` 挂接（`Map_Draw()` 之后，与调试线绘制前后无冲突）：
  ```
  Direct3D_SetDepthWriteEnable(false);
  Decal_Draw();
  Particle_Draw();
  Direct3D_SetDepthWriteEnable(true);
  ```
  时序：贴花/粒子在地图之后画（要被建筑遮挡但不写深度），在 2D overlay（准星/Fade）之前

### 3.8 资源

| 源（LPSP Unity 包） | 目标 | 用途 |
|---|---|---|
| `Art/Textures/Effects/T_Bullet_D.png`（512×512 RGBA） | `resource/texture/bullet_hole.png` | 弹孔贴花 |
| `Art/Textures/Effects/T_Sparks_D.png`（256×256 RGBA） | `resource/texture/sparks.png` | 命中火花 |

### 3.9 删除清单

- `Graphics/effect.cpp` / `Graphics/effect.h`
- `TriggerOn.vcxproj`：`<ClCompile Include="Graphics\effect.cpp" />` 与 `<ClInclude Include="Graphics\effect.h" />` 两行
- 确认前提（已核实）：`Effect_*` 全仓库无调用；`sprite_anime` 被 main.cpp 使用，保留

## 4. 测试

- **standalone 单测** `Game/tests/test_particle.cpp`（惯例：`g++ -std=c++17 -Wall -Wextra` 编译，不进 vcxproj，无引擎依赖——粒子池逻辑须不依赖 D3D 才可测；随机采样逻辑拆成可注入种子的纯函数）：
  - 配置注册返回递增 ID，池满返回 -1
  - Emit 占用空槽、池满时丢弃不覆盖
  - Update：寿命到期失活、位置积分、重力、阻尼、alpha 单调递减
- `Game/tests/test_decal.cpp`：环形覆写顺序（写 129 次后最老槽位是 #1）、槽位回收
- `Game/tests/test_collision_ray.cpp`：`Collision_RayAABB`——轴向平行退化、起点在盒内、各轴命中法线方向（+x/-x/+y/-y/+z/-z 六面）
- F8 调试面板：现有 Debug-only 面板加一节激活弹孔数/粒子数（沿用现有面板代码风格）
- 手动验收：游戏内扫射墙面——弹孔朝向贴合表面、无 z-fighting、火花随机散布并淡出；TPS 视角确认无闪面；池满 128 后最老弹孔消失
- **基线**：本仓库无自动化渲染测试，编译通过（现有 MSBuild 流程）+ 上两项单测通过即为绿的最低标准

## 5. 不做的（YAGNI）

- 机器人/远端玩家弹孔与火花（后续批次，需网络事件或远端视角重建）
- 弹孔命中音效（LPSP 的 se.wav 路线不搬）
- 贴花定时淡出、贴花与动态物体贴合（表面移动不跟随）
- 粒子拖尾、光照交互、GPU 粒子、合批渲染
- Muzzle flash（枪口火焰，下一批效果）
- LPSP 的 OOP Emitter 继承树、billboard 的激光/UV 变体