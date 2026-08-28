// editor.js — level editor panels (M4a).
//
// Layout note: the dock SIZES arrive from C++ (onEditorLayout) because C++
// hit-tests the same numbers to route each click to the UI or the 3D viewport.
// This file only writes them into CSS variables; it never invents them.

(function () {
    const root = document.getElementById('page-editor');
    const ed   = () => window.editor;            // absent in the browser preview harness

    function el(id) { return document.getElementById(id); }

    // ---- C++ -> page pushes -------------------------------------------------

    window.onEditorLayout = function (json) {
        let d;
        try { d = JSON.parse(json); } catch (e) { console.error('[PageEditor] bad layout', e); return; }
        const s = document.documentElement.style;
        s.setProperty('--dock-top-h',   d.topH   + 'px');
        s.setProperty('--dock-right-w', d.rightW + 'px');
    };

    window.onEditorStatus = function (json) {
        let s;
        try { s = JSON.parse(json); } catch (e) { console.error('[PageEditor] bad status', e); return; }

        root.querySelectorAll('#ed-tools .ed-btn').forEach((b) => {
            b.classList.toggle('active', b.dataset.tool === s.tool);
        });

        const snap = el('ed-snap');
        if (snap) snap.checked = !!s.snap;

        const undo = el('ed-undo'), redo = el('ed-redo');
        if (undo) undo.disabled = !s.canUndo;
        if (redo) redo.disabled = !s.canRedo;

        const counts = el('ed-counts');
        if (counts) counts.textContent =
            s.boxes + ' BOX   ' + s.models + ' MODEL   ' + s.colliders + ' COL';
    };

    // ---- page -> C++ --------------------------------------------------------

    root.addEventListener('click', (e) => {
        const btn = e.target.closest('button, [data-tool]');
        if (!btn || btn.disabled || !ed()) return;

        if (btn.dataset.tool) { ed().setTool(btn.dataset.tool); return; }

        switch (btn.id) {
            case 'ed-undo':   ed().undo();            break;
            case 'ed-redo':   ed().redo();            break;
            case 'ed-delete': ed().deleteSelection(); break;
            case 'ed-save':   ed().save();            break;
            case 'ed-reload': ed().reload();          break;
        }
    });

    root.addEventListener('change', (e) => {
        if (e.target.id === 'ed-snap' && ed()) ed().setSnap(e.target.checked);
    });

    function onEnter() {
        // Clear any focus flag stranded by a previous visit or a hot reload — a
        // stale `true` disables every editor hotkey in C++.
        ed()?.setTextFocus?.(false);
    }

    function onExit() {
        ed()?.setTextFocus?.(false);
        ed()?.setPointerCapture?.(false);
    }

    window.PageEditor = { onEnter, onExit };
})();
