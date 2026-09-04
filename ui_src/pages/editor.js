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

    // ---- inspector ----------------------------------------------------------

    // ROT is exchanged in DEGREES; C++ stores radians and converts on both
    // sides of the bridge, so this file never sees a radian.
    const FIELDS = ['pos', 'rot', 'scale', 'cmin', 'cmax'];

    function rowInputs(field) {
        const row = root.querySelector('.ed-row[data-field="' + field + '"]');
        return row ? Array.from(row.querySelectorAll('input')) : [];
    }

    function fmt(v) {
        // Trailing-zero-free fixed point: 2.5 not 2.5000, 0 not 0.0000.
        return String(Math.round(v * 10000) / 10000);
    }

    function fillRow(field, values) {
        const inputs = rowInputs(field);
        inputs.forEach((inp, i) => {
            // RULE 2: never clobber the field being typed into. The gizmo pushes
            // a payload every frame while dragging.
            if (document.activeElement === inp) return;
            inp.value = fmt(values[i]);
        });
    }

    window.onEditorSelection = function (json) {
        let s;
        try { s = JSON.parse(json); } catch (e) { console.error('[PageEditor] bad selection', e); return; }

        // Null-checked (unlike a naive dereference): before router.js has
        // injected this page's markup these all resolve to null, and this
        // handler must degrade to a no-op instead of throwing inside a
        // C++->JS push.
        const xform  = el('ed-xform');
        const colSec = el('ed-collider');
        const nosel  = el('ed-nosel');
        const id     = el('ed-sel-id');

        if (!s.has) {
            if (xform)  xform.hidden = true;
            if (colSec) colSec.hidden = true;
            if (nosel)  nosel.hidden = false;
            if (id)     id.textContent = 'none';
            return;
        }

        if (nosel) nosel.hidden = true;
        if (id)    id.textContent = s.kind.toUpperCase() + ' #' + s.index;

        if (s.kind === 'collider') {
            if (xform)  xform.hidden = true;
            if (colSec) colSec.hidden = false;
            fillRow('cmin', s.collider.min);
            fillRow('cmax', s.collider.max);
            const g = el('ed-isground');
            if (g && document.activeElement !== g) g.checked = !!s.collider.isGround;
        } else {
            if (colSec) colSec.hidden = true;
            if (xform)  xform.hidden = false;
            fillRow('pos',   s.pos);
            fillRow('rot',   s.rot);
            fillRow('scale', s.scale);
        }
    };

    // RULE 1: commit on change (fires on blur-with-edit and on Enter), never per
    // keystroke — one user edit must equal one undo step.
    root.addEventListener('change', (e) => {
        const inp = e.target;
        if (inp.tagName !== 'INPUT' || inp.type !== 'text' || !ed()) return;
        const row = inp.closest('.ed-row');
        if (!row) return;

        const field = row.dataset.field;
        if (!FIELDS.includes(field)) return;

        const v = rowInputs(field).map((i) => parseFloat(i.value));
        if (v.some((n) => !isFinite(n))) return;      // reject garbage; C++ keeps the old value
        ed().setTransform(field, v[0], v[1], v[2]);
    });

    root.addEventListener('change', (e) => {
        if (e.target.id === 'ed-isground' && ed()) ed().setColliderGround(e.target.checked);
    });

    // Enter commits immediately (blur fires `change`), Escape abandons the edit.
    // Restore the value cached on focusin rather than blanking it: blanking wins
    // against the commit handler's isFinite guard (so no bad command is created)
    // but leaves the field empty forever afterward — PublishSelection dedupes on
    // content, so nothing ever pushes a refill, and the commit handler reads all
    // three axes of the row, so a sibling-axis edit is then silently rejected too.
    root.addEventListener('keydown', (e) => {
        if (e.target.tagName !== 'INPUT') return;
        if (e.key === 'Enter')  { e.target.blur(); }
        if (e.key === 'Escape') { e.target.value = e.target.dataset.prev || ''; e.target.blur(); }
    });

    // Focus gating: while a TEXT field has focus, C++ must ignore EVERY editor
    // hotkey — otherwise typing 5 also drops a box (B) and Ctrl+Z undoes the map
    // instead of the text. focusin/focusout bubble, unlike focus/blur.
    //
    // Gated on type === 'text', not tagName === 'INPUT': #ed-snap/#ed-isground are
    // <input type="checkbox">, and WebKit focuses a checkbox on click, so gating on
    // tagName alone latches textFocus=true on every checkbox click and never clears
    // it (a checkbox only ever consumes Space, never fires the char input this gate
    // exists for) — killing every editor hotkey until the user clicks elsewhere.
    root.addEventListener('focusin',  (e) => {
        if (e.target.type === 'text') {
            e.target.dataset.prev = e.target.value;   // Escape restore target
            ed()?.setTextFocus?.(true);
        }
    });
    root.addEventListener('focusout', (e) => {
        if (e.target.type === 'text') ed()?.setTextFocus?.(false);
    });

    function onEnter() {
        // Clear any focus flag stranded by a previous visit or a hot reload — a
        // stale `true` disables every editor hotkey in C++.
        ed()?.setTextFocus?.(false);
        // Router.show() guarantees onEnter runs after this page's markup has been
        // injected (router.js fetches it asynchronously, which resolves after
        // OnDOMReady already re-armed C++'s dirty flags and flushed into the then-
        // empty page). Force a fresh push now that #ed-tools/#ed-counts/#ed-xform
        // actually exist, or the panels sit stale (no .active highlight, counts
        // stuck at "--", UNDO/REDO not greyed out) until something changes.
        ed()?.refresh?.();
    }

    function onExit() {
        ed()?.setTextFocus?.(false);
        ed()?.setPointerCapture?.(false);
    }

    window.PageEditor = { onEnter, onExit };
})();
