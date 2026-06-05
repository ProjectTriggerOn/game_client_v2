// mock_bridge.js
// ==============
// 浏览器侧 stand-in for the C++ JS Bridge. 让你在 Chrome/Edge 里直接调 UI，
// 不用启动游戏。被 dev.html 加载，**不被** index.html 加载。
//
// 提供：
//   1. window.game.* —— mock 实现，行为对齐 docs §8.1 的 API 契约
//   2. window.on*    —— 暂时不主动触发；Slice D 接入数据推送时会加定时器
//   3. 右上角 dev overlay：显示当前页 + 按钮切页
//   4. 键盘快捷键：F1=hud, F2=title（对齐 main.cpp 里的临时调试键）

(function () {
    'use strict';

    const STORAGE_KEY = 'triggeron_ui_mock_config';

    // 对齐 docs §9.1 的 Config Schema 默认值
    const DEFAULT_CONFIG = {
        'input.sensitivity':    0.005,
        'input.invert_y':       false,
        'audio.master_volume':  1.0,
        'audio.bgm_volume':     0.8,
        'audio.sfx_volume':     1.0,
        'display.fov':          90.0,
        'display.width':        1920,
        'display.height':       1080,
        'display.fullscreen':   false,
        'display.vsync':        true,
        'network.mode':         'remote',
        'network.server_port':  7777,
        'network.local_host':   '127.0.0.1',
        'network.remote_host':  '127.0.0.1',
    };

    function loadConfig() {
        try {
            const raw = localStorage.getItem(STORAGE_KEY);
            return raw ? { ...DEFAULT_CONFIG, ...JSON.parse(raw) } : { ...DEFAULT_CONFIG };
        } catch (e) {
            return { ...DEFAULT_CONFIG };
        }
    }

    let config = loadConfig();

    // -------------------------- window.game.* --------------------------
    window.game = {
        setState(name) {
            console.log('[mock] game.setState', name);
            window.Router?.show?.(name);
        },
        quit() {
            console.log('[mock] game.quit');
            alert('[mock] game.quit() called');
        },
        startLocalGame() {
            console.log('[mock] game.startLocalGame');
            window.Router?.show?.('hud');
        },
        returnToTitle() {
            console.log('[mock] game.returnToTitle');
            window.Router?.show?.('title');
        },
        getConfig(key) {
            return key in config ? config[key] : null;
        },
        setConfig(key, value) {
            console.log('[mock] game.setConfig', key, '=', value);
            config[key] = value;
        },
        saveConfig() {
            console.log('[mock] game.saveConfig → localStorage');
            localStorage.setItem(STORAGE_KEY, JSON.stringify(config));
        },
        getPlayerList() {
            return [
                { id: 1, name: 'You',   team: 'RED',  ready: true  },
                { id: 2, name: 'Bot 1', team: 'RED',  ready: true  },
                { id: 3, name: 'Bot 2', team: 'BLUE', ready: false },
                { id: 4, name: 'Bot 3', team: 'BLUE', ready: true  },
            ];
        },
        getVersion() {
            return 'mock-dev / ' + new Date().toISOString().slice(0, 10);
        },
        log(msg) {
            console.log('[ui]', msg);
        },
    };

    // -------------------------- Keyboard shortcuts --------------------------
    window.addEventListener('keydown', (e) => {
        if (e.key === 'F1') { e.preventDefault(); window.game.setState('hud');   }
        if (e.key === 'F2') { e.preventDefault(); window.game.setState('title'); }
    });

    // -------------------------- Dev overlay --------------------------
    function buildOverlay() {
        const div = document.createElement('div');
        div.id = 'mock-dev-overlay';
        div.style.cssText = `
            position: fixed; top: 8px; right: 8px; z-index: 99999;
            background: rgba(0, 0, 0, 0.78); color: #6cf;
            padding: 10px 14px; min-width: 180px;
            font: 11px/1.45 'Consolas', monospace;
            border: 1px solid #6cf; border-radius: 4px;
            pointer-events: auto; user-select: none;
        `;
        div.innerHTML = `
            <div style="color:#6cf; font-weight:bold; margin-bottom:6px;">MOCK BRIDGE</div>
            <div>page: <span id="mock-cur" style="color:#fff;">?</span></div>
            <div style="margin-top:6px;">
                <button data-page="title" style="margin-right:4px;">title</button>
                <button data-page="hud">hud</button>
            </div>
            <div style="margin-top:6px; color:#888;">F1=hud  F2=title</div>
        `;
        document.body.appendChild(div);

        div.querySelectorAll('button').forEach((b) => {
            b.style.cssText = `
                background: #222; color: #6cf; border: 1px solid #6cf;
                padding: 2px 8px; font: 11px monospace; cursor: pointer;
            `;
            b.addEventListener('click', () => window.game.setState(b.dataset.page));
        });

        setInterval(() => {
            const el = document.getElementById('mock-cur');
            if (el) el.textContent = window.Router?.current || '?';
        }, 200);
    }

    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', buildOverlay);
    } else {
        buildOverlay();
    }

    console.log('[mock_bridge] loaded — game.* stubbed, F1/F2 bound, overlay attached.');
})();
