# UI 浏览器开发流程

`ui_src/` 是 SPA 风格的 web 工程，可以**完全脱离游戏**在浏览器里迭代。

## 文件分工

| 文件 | 用途 | 加载场景 |
|------|------|---------|
| `index.html` | 生产入口，被 Ultralight 加载 | 游戏运行时 |
| `dev.html` | 浏览器入口，加 mock bridge + dev overlay | 本地浏览器开发 |
| `dev/mock_bridge.js` | mock `window.game.*` API + 浮层 + 快捷键 | 仅被 `dev.html` 加载 |

## 怎么跑

**推荐：VS Code Live Server**

1. VS Code 装 `Live Server` 扩展（作者 Ritwick Dey）
2. 在 `ui_src/dev.html` 上右键 → "Open with Live Server"
3. 浏览器自动开 `http://127.0.0.1:5500/.../dev.html`
4. 改任何 `.html` / `.css` / `.js` → 浏览器自动 reload

**或：Python http.server**

```bash
cd game_client/ui_src
python -m http.server 8000
# 浏览器访问 http://127.0.0.1:8000/dev.html
```

## ⚠️ 不要直接双击 dev.html

`file://` 协议下 fetch 受 CORS 限制，`router.js` 加载 `pages/*.html` 会全部失败、屏幕空白。**必须**走 HTTP 服务器（Live Server 自带 / Python http.server / 其它任意）。

## 调试操作

| 操作 | 效果 |
|------|------|
| `F1` | 切到 HUD 页（对齐 main.cpp 里临时的 F1 调试键） |
| `F2` | 切到 TITLE 页 |
| 右上角 overlay 按钮 | 鼠标点击切页 |
| `game.setConfig(...)` + `game.saveConfig()` 在控制台调 | 写入 `localStorage`，浏览器刷新后还在 |

## Mock 出来的 API

`mock_bridge.js` 覆盖了 `docs/ultralight_integration.md` §8.1 列的全部 `game.*`：

- `setState` / `quit` / `startLocalGame` / `returnToTitle`
- `getConfig` / `setConfig` / `saveConfig` —— 用 `localStorage` 持久化
- `getPlayerList` / `getVersion` / `log`

C++ → JS 推送（`window.onHealthChanged` 等）暂未模拟。Slice D 接入真实数据推送时再加定时器驱动。

## 跟游戏环境的差异

| 项 | 浏览器 | 游戏 |
|----|--------|------|
| 字体 | 系统字体 | Ultralight FreeType + 内嵌 TTF |
| 字体渲染 | 浏览器引擎 | WebKit + FreeType |
| 性能特征 | V8 / SpiderMonkey | JavaScriptCore（WebKit 自带） |
| DPI | 浏览器自己处理 | 物理像素，看 docs §6.4 |
| `console.log` 输出 | 浏览器 DevTools | VS 输出窗口（`[UI:console]` 前缀，由 ui_manager.cpp 的 `UIViewListener::OnAddConsoleMessage` 经 OutputDebugString 转发）；引擎日志另在 `logs/ultralight.log` |

**意味着**：浏览器调出的视觉效果**和游戏不会 100% 一致**。最终验收必须在游戏里跑一遍。但 80% 的"布局对不对、颜色顺不顺、动效卡不卡"在浏览器里能直接判断。

## 不要在浏览器里依赖的特性

- `position: fixed` 在 Ultralight 里行为一致，但避免依赖 `visualViewport` API
- `requestAnimationFrame` 在 Ultralight 里被 `Renderer::Update` 节流，浏览器没有这个限制
- WebGL / Canvas 2D —— Ultralight 1.4 不支持
- WebSocket / fetch 真实 HTTP —— 游戏里没人提供后端
