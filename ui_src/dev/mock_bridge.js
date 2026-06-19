// mock_bridge.js
// ==============
// Browser-side stand-in for the C++ JS Bridge. Lets you iterate on the UI in
// Chrome/Edge without launching the game. Loaded by dev.html, NOT by index.html.
//
// Provides:
//   1. window.game.* — mock implementations matching the docs §8.1 API contract
//   2. window.on*    — not fired automatically yet; Slice D adds timers when
//                      data push lands
//   3. Top-right dev overlay: shows the current page + page-switch buttons
//   4. Keyboard shortcuts: F1=hud, F2=title (mirrors the temporary debug keys
//      in main.cpp)

(function () {
    'use strict';

    const STORAGE_KEY = 'triggeron_ui_mock_config';

    // Defaults matching the docs §9.1 Config Schema
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
        // Mirrors C++ UIPolicy_DerivePage: the browser harness has no scene/
        // GameState, so it always boots to the title sandbox.
        getBootPage() {
            return 'title';
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
