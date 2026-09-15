<p align="center">
  <a href="./README.md">English</a> | <a href="./README_JP.md">日本語</a> | 中文
</p>

# 浏览器 UI 开发

`ui_src/` 是不带构建步骤、不依赖框架的单页应用（SPA），所以整套游戏内 UI 都可以在浏览器里
迭代 —— 不用编译游戏，也不用起服务器。浏览器侧的入口是 `dev.html`，它会先于其他脚本加载
一份 C++ bridge 的替身，因此调用 `game.*` 的页面在这里的行为和在游戏里一致。

## 只有这三个文件不一样

| 文件 | 用途 | 由谁加载 |
|------|------|---------|
| `index.html` | 生产入口 | 游戏内的 Ultralight |
| `dev.html` | 浏览器入口，额外挂 mock bridge 和调试浮层 | 浏览器 |
| `dev/mock_bridge.js` | `window.game.*` 的替身，同时负责浮层和快捷键 | 仅 `dev.html` |

其余都是两边共用的同一份代码：`router.js`、`shared.css`、`flood.js`，以及 `pages/`
下的全部页面。

## 怎么跑

**VS Code Live Server（推荐）**

1. 安装 *Live Server* 扩展（作者 Ritwick Dey）
2. 右键 `ui_src/dev.html` → **Open with Live Server**
3. 浏览器自动打开 `http://127.0.0.1:5500/.../dev.html`，改动任何 `.html` / `.css` /
   `.js` 都会自动刷新

**或者任意静态服务器**

```bash
cd game_client/ui_src
python -m http.server 8000
# http://127.0.0.1:8000/dev.html
```

### 不要直接双击打开 `dev.html`

`file://` 协议下 `fetch('pages/…')` 会被 CORS 拒绝，`router.js` 的页面加载全部失败，
屏幕一片空白。必须走 HTTP。

## `dev.html` 的 page div 要和 `index.html` 保持同步

`router.js` 里有一张 `PAGES` 表，启动时逐个拉取页面标记并写进 `#page-<名字>`。
如果 `PAGES` 里有某个名字但没有对应的 div，后果不是"少一个页面"这么轻：
`el(name).innerHTML` 会在 `null` 上抛异常 → `Promise.all` reject → `init()` 的
`catch` 把它吞掉 → `Router.show()` 永远不会执行。所有页面都保持 `hidden`，
**整个环境黑屏**，控制台只留下一行 `[Router] init failed: TypeError`，再没有别的线索。

这不是假设。加计分系统时 `result` 进了 `PAGES`，但 `dev.html` 没跟着改，从那次提交起
浏览器开发环境就一直是死的，直到被发现为止。**新增页面时，两个文件都要加 div。**

## mock 覆盖了哪些

`dev/mock_bridge.js` 实现的 `game.*`：

- **页面跳转** —— `setState`、`startLocalGame`、`returnToTitle`、`nextMatch`、
  `quit`、`getBootPage`
- **配置** —— `getConfig`、`setConfig`、`saveConfig`，写进 `localStorage`，刷新后还在
- **显示设置** —— `getDisplayInfo`（显示器列表是假的）、`applyDisplaySettings`、
  `confirmDisplaySettings`、`revertDisplaySettings`，包含 15 秒自动回滚的倒计时
- **其它** —— `getPlayerList`、`getVersion`、`log`

## 没覆盖哪些

| 未实现 | 在浏览器里的表现 |
|--------|----------------|
| `resume`、`openSettings`、`backToPause` | 暂停菜单的 RESUME 和 SETTINGS 点了没反应。页面侧是用 `window.game?.resume?.()` 调的，所以不会报错，只是静默地什么都不做 |
| `uiClick` | 没有 UI 点击音（这里本来也没有音频引擎） |
| `setFloodDebug`、`getFloodStats` | flood 面板只是一堆可以调样式的标记；`flood.js` 每次调 bridge 前都会做特性检测 |

**C++ → JS 的推送**更薄。游戏里 C++ 会随着对局进行调用 `window.onHealthChanged`、
`onAmmoChanged`、`onScoresChanged`、`onKillFeed`、`onMatchTimerChanged`、
`onMatchPhaseChanged`、`onScoreboardVisible`、`onScoreboardData`、`onMatchResult`，
这里一个都不会触发 —— 只有 `onDisplayRevertTick` 会被 mock 的回滚倒计时驱动。所以
HUD 和结算页显示的就是静态标记。想看某个状态，从控制台手动调：

```js
onHealthChanged(35)
onMatchPhaseChanged(1, 4.2)      // COUNTDOWN，剩 4.2 秒
```

## 调试浮层

右上角显示当前页名，外加每个页面一个按钮。`F1` 切到 HUD，`F2` 切到标题页 —— 这两个键
是浮层自己的，只在这里有（游戏里的 F1 是切换碰撞调试显示）。

## 浏览器和游戏的差别

| | 浏览器 | 游戏 |
|---|--------|------|
| 字体 | 系统里有什么用什么 | FreeType + `ui_src/fonts/` 下的 TTF |
| 文字渲染 | 浏览器引擎 | WebKit + FreeType |
| JS 引擎 | V8 / SpiderMonkey | JavaScriptCore（Ultralight 自带） |
| DPI | 浏览器自己处理 | 物理像素，device scale 由 `UI::Initialize` 决定 |
| `console.log` | DevTools | 经 `ui_manager.cpp` 的 `UIViewListener::OnAddConsoleMessage` 加上 `[UI:console]` 前缀转发到 `OutputDebugString`，落在 VS 输出窗口；引擎自身的日志在 `logs/ultralight.log` |

所以渲染结果不会完全一致。浏览器能解决的是布局、配色、间距和动效时序 —— 工作量上的大头 ——
而这些都不值得等一次游戏编译。最终验收仍然要在游戏里做。

## 不要依赖这些

- **WebGL** —— Ultralight 没有提供。
- **`requestAnimationFrame`** —— 游戏里会被 `Renderer::Update` 节流，浏览器没有这个限制。
- **`visualViewport`** —— 行为不同，不要依赖。
- **真实网络 I/O** —— 游戏里的 `fetch` / WebSocket 没有后端可以应答。

凡是超出纯 DOM 和 CSS 的东西，在页面开始依赖它之前，都值得先在游戏里确认一遍。
