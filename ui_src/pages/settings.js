// settings.js — self-contained IIFE; exposes only PageSettings = { onEnter, onExit }
//
// Two kinds of settings:
//   - LIVE (sensitivity, invert_y, vsync, fov, max_fps, aspect filter): apply
//     immediately via game.setConfig — C++ subscribers fire on the spot. SAVE
//     persists the user layer to user_settings.toml.
//   - DISPLAY MODE / RESOLUTION / MONITOR: risky (a bad mode can black-screen),
//     so they go through APPLY → game.applyDisplaySettings, which changes the
//     window live and starts a C++-owned 15s revert countdown. window.on
//     DisplayRevertTick(sec) drives the confirm dialog; KEEP persists, REVERT /
//     timeout restores the previous mode.
//
// The resolution list is built from game.getDisplayInfo() (real DXGI enumeration
// of the player's monitor), filtered by the aspect-ratio dropdown.
//
// Listeners are delegated on the always-present #page-settings container — bound
// once at load, so they never depend on the injected controls existing yet and
// survive hot-reload re-injection.

(function () {
    const root = document.getElementById('page-settings');
    let displayInfo = null;

    const ASPECTS = { '16:9': 16 / 9, '16:10': 16 / 10, '21:9': 21 / 9, '4:3': 4 / 3 };
    const ASPECT_TOL = 0.06;   // wide enough for real ultrawide (2560x1080 ≈ 2.37)

    function el(id) { return document.getElementById(id); }

    function aspectMatches(w, h, key) {
        if (key === 'all' || !ASPECTS[key] || !h) return true;
        return Math.abs((w / h) - ASPECTS[key]) < ASPECT_TOL;
    }

    // ---- custom dropdown ------------------------------------------------------
    // Ultralight renders the native <select> popup as an unstyled white OS list,
    // so we hide the <select> and drive a styled list from it. The <select> stays
    // the value source: picking an option sets sel.value + dispatches 'change', so
    // every existing handler (rebuild / apply / setConfig) runs unchanged.
    function closeDropdowns() {
        document.querySelectorAll('#page-settings .dd.open').forEach((d) => d.classList.remove('open'));
    }
    function openDropdown(sel) {
        const dd = sel._dd;
        if (!dd || sel.disabled) return;
        const r = dd.trigger.getBoundingClientRect();
        const list = dd.list;
        list.style.left = Math.round(r.left) + 'px';
        list.style.width = Math.round(r.width) + 'px';
        list.style.maxHeight = 'none';
        dd.wrap.classList.add('open');                 // visible now → measurable
        const full = list.offsetHeight, gap = 6, cap = 280;
        const below = window.innerHeight - r.bottom - gap, above = r.top - gap;
        let h = Math.min(full, cap);
        if (h <= below || below >= above) { h = Math.min(h, below); list.style.top = Math.round(r.bottom) + 'px'; }
        else { h = Math.min(h, above); list.style.top = Math.round(r.top - h) + 'px'; }
        list.style.maxHeight = Math.max(h, 40) + 'px';
    }
    function syncDropdown(sel) {
        const dd = sel._dd;
        if (!dd) return;
        const cur = sel.options[sel.selectedIndex];
        dd.label.textContent = cur ? cur.textContent : '';
        dd.list.innerHTML = '';
        [...sel.options].forEach((o) => {
            const item = document.createElement('div');
            item.className = 'dd-opt' + (o.selected ? ' sel' : '');
            item.setAttribute('role', 'option');
            item.dataset.value = o.value;
            item.textContent = o.textContent;
            dd.list.appendChild(item);
        });
        dd.wrap.classList.toggle('dd-disabled', sel.disabled);
        if (sel.disabled) dd.wrap.classList.remove('open');
    }
    function enhanceSelect(sel) {
        if (sel._dd) return;
        const wrap = document.createElement('div');
        wrap.className = 'dd';
        const trigger = document.createElement('button');
        trigger.type = 'button'; trigger.className = 'dd-trigger';
        const label = document.createElement('span'); label.className = 'dd-label';
        trigger.appendChild(label);
        const list = document.createElement('div');
        list.className = 'dd-list'; list.setAttribute('role', 'listbox');
        wrap.appendChild(trigger); wrap.appendChild(list);
        sel.classList.add('dd-native');
        sel.parentNode.insertBefore(wrap, sel.nextSibling);
        sel._dd = { wrap, trigger, list, label };

        trigger.addEventListener('click', (e) => {
            e.stopPropagation();
            if (sel.disabled) return;
            const open = wrap.classList.contains('open');
            closeDropdowns();
            if (!open) openDropdown(sel);
        });
        list.addEventListener('click', (e) => {
            e.stopPropagation();
            const opt = e.target.closest('.dd-opt');
            if (!opt) return;
            if (sel.value !== opt.dataset.value) {
                sel.value = opt.dataset.value;
                sel.dispatchEvent(new Event('change', { bubbles: true }));  // existing logic runs
            }
            syncDropdown(sel);
            closeDropdowns();
        });
        syncDropdown(sel);
    }

    function rebuildMonitorList() {
        const sel = el('set-monitor');
        if (!sel || !displayInfo) return;
        sel.innerHTML = '';
        displayInfo.monitors.forEach((m) => {
            const o = document.createElement('option');
            o.value = String(m.index);
            o.textContent = `${m.index}: ${m.desktop.width}x${m.desktop.height}` + (m.primary ? ' (Primary)' : '');
            sel.appendChild(o);
        });
        sel.value = String(displayInfo.current.monitor);
        syncDropdown(sel);
    }

    function rebuildResList() {
        const sel = el('set-res');
        if (!sel || !displayInfo) return;
        const monIdx = parseInt(el('set-monitor')?.value ?? '0', 10) || 0;
        const mon = displayInfo.monitors[monIdx] || displayInfo.monitors[0];
        const aspect = el('set-aspect')?.value ?? 'all';
        const cur = displayInfo.current;

        sel.innerHTML = '';

        // Native/Auto sentinel (value "0x0" → width=0 in applyDisplaySettings).
        const nativeOpt = document.createElement('option');
        nativeOpt.value = '0x0';
        nativeOpt.textContent = mon ? `Native (${mon.desktop.width}x${mon.desktop.height})` : 'Native';
        sel.appendChild(nativeOpt);

        (mon?.modes || []).forEach((r) => {
            if (!aspectMatches(r.width, r.height, aspect)) return;
            const o = document.createElement('option');
            o.value = `${r.width}x${r.height}`;
            o.textContent = `${r.width} x ${r.height}`;
            sel.appendChild(o);
        });

        // Preselect the current resolution (0/0 = native); add it if filtered out.
        if (!cur.width || !cur.height) {
            sel.value = '0x0';
        } else {
            const want = `${cur.width}x${cur.height}`;
            if (![...sel.options].some((o) => o.value === want)) {
                const o = document.createElement('option');
                o.value = want; o.textContent = `${cur.width} x ${cur.height}`;
                sel.appendChild(o);
            }
            sel.value = want;
        }

        // Borderless is always native → the resolution picker is meaningless.
        sel.disabled = (el('set-mode')?.value === 'borderless');
        syncDropdown(sel);
    }

    function refresh() {
        const g = window.game;
        if (!g) return;

        // --- existing live keys: sensitivity + invert ---
        if (g.getConfig) {
            const sens = el('set-sens'), out = el('set-sens-val');
            const cur = g.getConfig('input.sensitivity');
            if (sens && cur != null) { sens.value = cur; if (out) out.textContent = Number(cur).toFixed(4); }
            const invert = el('set-invert');
            const inv = g.getConfig('input.invert_y');
            if (invert && inv != null) invert.checked = !!inv;
        }

        // --- display settings from real DXGI enumeration ---
        if (!g.getDisplayInfo) return;
        try { displayInfo = JSON.parse(g.getDisplayInfo()); }
        catch (e) { console.log('[PageSettings] getDisplayInfo parse failed', e); return; }

        const c = displayInfo.current;
        if (el('set-mode'))   el('set-mode').value = c.mode;
        if (el('set-aspect')) el('set-aspect').value = c.aspect || 'all';
        if (el('set-vsync'))  el('set-vsync').checked = !!c.vsync;
        if (el('set-maxfps')) el('set-maxfps').value = String(c.maxFps);
        rebuildMonitorList();               // these two sync their own dropdowns
        rebuildResList();
        // sync the static-option dropdowns to their freshly-set values
        ['set-mode', 'set-aspect', 'set-maxfps'].forEach((id) => { const s = el(id); if (s) syncDropdown(s); });
    }

    // ---- live inputs (fire continuously) ----
    root?.addEventListener('input', (e) => {
        if (e.target.id === 'set-sens') {
            const v = parseFloat(e.target.value);
            const out = el('set-sens-val'); if (out) out.textContent = v.toFixed(4);
            window.game?.setConfig?.('input.sensitivity', v);
        }
        // FOV control is temporarily hidden (see settings.html) — no handler.
    });

    // ---- discrete changes ----
    root?.addEventListener('change', (e) => {
        switch (e.target.id) {
        case 'set-invert':  window.game?.setConfig?.('input.invert_y', e.target.checked); break;
        case 'set-vsync':   window.game?.setConfig?.('display.vsync', e.target.checked); break;
        case 'set-maxfps':  window.game?.setConfig?.('display.max_fps', parseInt(e.target.value, 10)); break;
        case 'set-aspect':  window.game?.setConfig?.('display.aspect_ratio', e.target.value); rebuildResList(); break;
        case 'set-monitor': rebuildResList(); break;
        case 'set-mode':    rebuildResList(); break;  // toggles the native-disable state
        }
    });

    // ---- buttons ----
    root?.addEventListener('click', (e) => {
        if (e.target.closest('#set-apply')) {
            const mode = el('set-mode')?.value ?? 'windowed';
            const monitor = parseInt(el('set-monitor')?.value ?? '0', 10) || 0;
            let w = 0, h = 0;
            const resVal = el('set-res')?.value ?? '0x0';
            if (resVal !== '0x0') { const p = resVal.split('x'); w = parseInt(p[0], 10); h = parseInt(p[1], 10); }
            window.game?.applyDisplaySettings?.(mode, w, h, monitor);
        }
        else if (e.target.closest('#set-save')) window.game?.saveConfig?.();
        else if (e.target.closest('#revert-keep')) window.game?.confirmDisplaySettings?.();
        else if (e.target.closest('#revert-undo')) window.game?.revertDisplaySettings?.();
        else if (e.target.closest('#set-back')) {
            // Shared page: from the title via Router.navigate (pop the stack) vs
            // from the in-game pause via state-driven Router.show (backToPause).
            if (window.Router && Router.stack.length) Router.back();
            else window.game?.backToPause?.();
        }
    });

    // C++-owned revert countdown: sec>0 shows/updates the dialog, 0 hides it.
    window.onDisplayRevertTick = (sec) => {
        const modal = el('display-revert');
        if (!modal) return;
        if (sec > 0) {
            modal.classList.remove('hidden');
            const num = el('revert-sec'); if (num) num.textContent = String(sec);
        } else {
            modal.classList.add('hidden');
            refresh();   // resync dropdowns to the kept/reverted state
        }
    };

    // Close any open dropdown on an outside click, on scroll (the fixed list
    // wouldn't follow), or on resize. Triggers/lists stopPropagation, so their
    // own clicks don't reach here.
    document.addEventListener('click', closeDropdowns);
    document.addEventListener('scroll', closeDropdowns, true);   // capture: catches the form's scroll
    window.addEventListener('resize', closeDropdowns);

    function onEnter() {
        console.log('[PageSettings] enter');
        document.querySelectorAll('#page-settings .set-select').forEach(enhanceSelect);
        refresh();
    }
    function onExit()  { console.log('[PageSettings] exit'); closeDropdowns(); }

    window.PageSettings = { onEnter, onExit };
})();
