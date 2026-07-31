import React, { useState, useMemo, useRef, useEffect } from "react";

// =====================================================================
// ZedLab FM -- 4-operator FM synth  v9
// Panel 1024 x 600 (Elecrow Smart Panel native resolution)
//
// Algorithms: canonical Yamaha 4-op set (DX21/DX27/DX100/TX81Z,
// YM2151 "OPM" connections 0-7), named for what each is good for.
//
// GRADIENT DEFERRED. Floyd's brightness gradient is not wired in this
// build -- pending his answer on how the metric was derived. The hook
// is `brightnessAt()` below; it is the ONLY place that needs to change.
// Nothing in the DSP, envelope editor, or layout depends on it.
//
// Operator identity: each operator owns a colour, and it NEVER changes.
// Selection is carried by weight, not hue -- the selected line renders
// 60% thicker with a stronger bloom, and its tab oval + number go white.
// Colour therefore always answers "which line is which".
// =====================================================================

const ALGORITHMS = [
  { n: 1, name: "STACK",      label: "1>2>3>4",     edges: [[0,1],[1,2],[2,3]], carriers: [3] },
  { n: 2, name: "TWIN FEED",  label: "(1+2)>3>4",   edges: [[0,2],[1,2],[2,3]], carriers: [3] },
  { n: 3, name: "SPLIT MOD",  label: "1+(2>3)>4",   edges: [[1,2],[0,3],[2,3]], carriers: [3] },
  { n: 4, name: "BRANCH",     label: "(1>2)+3>4",   edges: [[0,1],[1,3],[2,3]], carriers: [3] },
  { n: 5, name: "TWIN PAIR",  label: "(1>2)+(3>4)", edges: [[0,1],[2,3]],       carriers: [1,3] },
  { n: 6, name: "FAN",        label: "1>2,3,4",     edges: [[0,1],[0,2],[0,3]], carriers: [1,2,3] },
  { n: 7, name: "PAIR+SINE",  label: "1>2 +3+4",    edges: [[0,1]],             carriers: [1,2,3] },
  { n: 8, name: "ADDITIVE",   label: "1+2+3+4",     edges: [],                  carriers: [0,1,2,3] },
];

const clamp = (v, lo, hi) => Math.max(lo, Math.min(hi, v));

// --- GEOMETRY --------------------------------------------------------
const PANEL_W = 1024, PANEL_H = 600;
const EDGE = 16, GAP = 14;
const TOP_PAD = 12, TAB_H = 42, TAB_GAP = 9;
const STRIP_H = 40;
const ALG_COL = 138, SLIDER_COL = 132;

const CANVAS_Y = TOP_PAD + TAB_H + 14;
const CANVAS_W = PANEL_W - EDGE * 2 - ALG_COL - SLIDER_COL - GAP * 2;
const CANVAS_H = PANEL_H - CANVAS_Y - STRIP_H - 8;

const PAD = 24;
const PLOT_W = CANVAS_W - PAD * 2;
const PLOT_H = CANVAS_H - PAD * 2;

const AD_WINDOW = 5.0, REL_WINDOW = 4.0;
const AD_FRAC = 0.46, SUS_FRAC = 0.22, REL_FRAC = 0.32;
const SUS_SWEEP = 0.45;

const STROKE = 9;

const OP_COLOUR = ["#3fa9ff", "#ff8f2e", "#34d058", "#c86bff"];
const SELECTED = "#ffffff";
const TAB_IDLE = "#5b7fb5";
const LIGHT = "#dfe7f3";
const MONO = "'VT323', 'Consolas', monospace";

const xAD = t => PAD + (t / AD_WINDOW) * AD_FRAC * PLOT_W;
const xSusStart = PAD + AD_FRAC * PLOT_W;
const xSusEnd   = PAD + (AD_FRAC + SUS_FRAC) * PLOT_W;
const xREL = t => xSusEnd + (t / REL_WINDOW) * REL_FRAC * PLOT_W;
const yOf  = v => PAD + PLOT_H - v * PLOT_H;

function envShape(op, x) {
  if (x <= AD_FRAC) {
    const t = (x / AD_FRAC) * AD_WINDOW;
    if (t < op.a) return op.a <= 0 ? 1 : t / op.a;
    if (t < op.a + op.d) return 1 - (1 - op.s) * (op.d <= 0 ? 1 : (t - op.a) / op.d);
    return op.s;
  }
  if (x <= AD_FRAC + SUS_FRAC) return op.s;
  const t = ((x - AD_FRAC - SUS_FRAC) / REL_FRAC) * REL_WINDOW;
  if (t >= op.r) return 0;
  return op.s * (1 - (op.r <= 0 ? 1 : t / op.r));
}

// --- RESERVED: brightness metric -------------------------------------
// Not called in v9. This is the single integration point for Floyd's
// gradient once he confirms how the value was derived. Current best
// reading of the transcript: mean y-axis value of the operators
// feeding `target`, with amplitude already baked into the y-value.
// eslint-disable-next-line no-unused-vars
function brightnessAt(target, alg, ops, x) {
  const feeders = alg.edges.filter(e => e[1] === target).map(e => e[0]);
  if (feeders.length === 0) return 0;
  let sum = 0;
  for (const m of feeders) sum += envShape(ops[m], x) * ops[m].amp;
  return clamp(sum / feeders.length, 0, 1);
}

function breakpoints(op) {
  return {
    peak:  { x: xAD(op.a),        y: yOf(op.amp) },
    susIn: { x: xAD(op.a + op.d), y: yOf(op.amp * op.s) },
    end:   { x: xREL(op.r),       y: yOf(0) },
  };
}

function envPath(op) {
  const b = breakpoints(op);
  return `M ${xAD(0)} ${yOf(0)} L ${b.peak.x} ${b.peak.y} ` +
         `L ${b.susIn.x} ${b.susIn.y} L ${xSusEnd} ${b.susIn.y} ` +
         `L ${b.end.x} ${b.end.y}`;
}

// --- FACTORY PRESETS -------------------------------------------------
const O = (a, d, s, r, amp, ratio) => ({ a, d, s, r, amp, ratio });

const FACTORY = [
  { name: "GLASS BELL", alg: 4, ops: [
      O(0.00, 0.90, 0.00, 0.90, 0.80, 3.50), O(0.00, 1.80, 0.00, 1.80, 1.00, 1.00),
      O(0.00, 0.60, 0.00, 0.60, 0.65, 7.00), O(0.00, 2.20, 0.00, 2.20, 0.85, 1.00)] },
  { name: "E PIANO",    alg: 4, ops: [
      O(0.00, 0.45, 0.00, 0.35, 0.72, 14.00), O(0.00, 2.40, 0.18, 0.80, 1.00, 1.00),
      O(0.00, 0.30, 0.00, 0.30, 0.40, 1.00),  O(0.00, 2.80, 0.22, 0.90, 0.70, 1.00)] },
  { name: "BRASS",      alg: 0, ops: [
      O(0.18, 0.60, 0.55, 0.30, 0.55, 1.00), O(0.12, 0.50, 0.62, 0.30, 0.62, 1.00),
      O(0.10, 0.45, 0.70, 0.28, 0.70, 1.00), O(0.06, 0.35, 0.82, 0.35, 1.00, 1.00)] },
  { name: "WOOD FLUTE", alg: 6, ops: [
      O(0.14, 0.50, 0.35, 0.30, 0.28, 2.00), O(0.10, 0.40, 0.85, 0.30, 0.90, 1.00),
      O(0.16, 0.60, 0.80, 0.35, 0.30, 2.00), O(0.20, 0.70, 0.75, 0.40, 0.20, 4.00)] },
  { name: "METAL HIT",  alg: 1, ops: [
      O(0.00, 0.22, 0.00, 0.22, 0.95, 5.25), O(0.00, 0.18, 0.00, 0.18, 0.85, 8.75),
      O(0.00, 0.30, 0.00, 0.30, 0.90, 3.00), O(0.00, 0.55, 0.00, 0.55, 1.00, 1.00)] },
  { name: "SUB BASS",   alg: 6, ops: [
      O(0.00, 0.30, 0.20, 0.20, 0.35, 1.00), O(0.00, 0.80, 0.70, 0.25, 1.00, 0.50),
      O(0.00, 0.60, 0.55, 0.25, 0.45, 0.50), O(0.00, 0.40, 0.30, 0.20, 0.20, 1.00)] },
  { name: "ORGAN",      alg: 7, ops: [
      O(0.02, 0.20, 1.00, 0.15, 0.85, 1.00), O(0.02, 0.20, 1.00, 0.15, 0.55, 2.00),
      O(0.02, 0.20, 1.00, 0.15, 0.35, 4.00), O(0.02, 0.20, 1.00, 0.15, 0.22, 8.00)] },
  { name: "PLUCK",      alg: 0, ops: [
      O(0.00, 0.28, 0.00, 0.20, 0.75, 2.00), O(0.00, 0.35, 0.00, 0.25, 0.70, 1.00),
      O(0.00, 0.45, 0.00, 0.30, 0.65, 1.00), O(0.00, 0.70, 0.00, 0.45, 1.00, 1.00)] },
  { name: "GROWL",      alg: 2, ops: [
      O(0.05, 0.90, 0.45, 0.40, 0.80, 2.00), O(0.00, 0.40, 0.30, 0.30, 0.70, 3.00),
      O(0.08, 1.10, 0.50, 0.45, 0.60, 1.00), O(0.02, 0.60, 0.70, 0.50, 1.00, 1.00)] },
  { name: "AIR PAD",    alg: 5, ops: [
      O(1.20, 2.00, 0.55, 1.80, 0.30, 2.00), O(0.90, 1.60, 0.80, 2.20, 0.85, 1.00),
      O(1.40, 2.40, 0.70, 2.40, 0.55, 1.00), O(1.10, 2.20, 0.65, 2.00, 0.45, 4.00)] },
];

const cloneOps = arr => arr.map(o => ({ ...o }));

// --- ALGORITHM DIAGRAM -----------------------------------------------
function levelsOf(alg) {
  const lv = [0, 0, 0, 0];
  for (let p = 0; p < 4; p++)
    for (const [s, d] of alg.edges) lv[s] = Math.max(lv[s], lv[d] + 1);
  return lv;
}

const BOX = 26, VGAP = 22;

function AlgDiagram({ alg, selOp, width, height }) {
  const lv = levelsOf(alg);
  const maxLv = Math.max(...lv);
  const rows = [];
  for (let l = 0; l <= maxLv; l++) rows.push([0,1,2,3].filter(i => lv[i] === l));

  const totalH = (maxLv + 1) * BOX + maxLv * VGAP;
  const yTop = Math.max(4, (height - totalH) / 2);
  const pos = {};
  rows.forEach((row, l) => {
    const y = yTop + (maxLv - l) * (BOX + VGAP);
    const span = row.length * BOX + (row.length - 1) * 12;
    const x0 = (width - span) / 2;
    row.forEach((op, k) => { pos[op] = { x: x0 + k * (BOX + 12), y }; });
  });

  return (
    <svg width={width} height={height} style={{ display: "block" }}>
      {alg.edges.map(([s, d], i) => {
        const a = pos[s], b = pos[d];
        const x1 = a.x + BOX / 2, y1 = a.y + BOX;
        const x2 = b.x + BOX / 2, y2 = b.y;
        const mx = (x1 + x2) / 2, my = (y1 + y2) / 2;
        return (
          <g key={i}>
            <line x1={x1} y1={y1} x2={x2} y2={y2} stroke="#4a5570" strokeWidth="1.6" />
            <polygon points={`${mx - 4},${my - 4} ${mx + 4},${my - 4} ${mx},${my + 4}`}
                     fill="#6a7a9c" />
          </g>
        );
      })}
      {[0,1,2,3].map(i => {
        const p = pos[i];
        const isCar = alg.carriers.includes(i);
        const on = i === selOp;
        return (
          <g key={i}>
            <rect x={p.x} y={p.y} width={BOX} height={BOX} rx="4"
                  fill={isCar ? "#1b2436" : "transparent"}
                  stroke={on ? SELECTED : OP_COLOUR[i]}
                  strokeWidth={on ? 2.2 : 1.4} opacity={on ? 1 : 0.8} />
            <text x={p.x + BOX / 2} y={p.y + BOX / 2 + 5} fontSize="15"
                  textAnchor="middle" fontFamily={MONO}
                  fill={on ? SELECTED : OP_COLOUR[i]}>{i + 1}</text>
          </g>
        );
      })}
    </svg>
  );
}

// --- SLIDER ----------------------------------------------------------
const SL_W = 54, SL_TRACK = 10, SL_THUMB = 14;

function VSlider({ label, value, min, max, step, height, onChange }) {
  const ref = useRef(null);
  const [dragging, setDragging] = useState(false);
  const top = SL_THUMB + 4;
  const trackH = height - top * 2;
  const cy = top + (1 - (value - min) / (max - min)) * trackH;

  const apply = e => {
    if (!ref.current) return;
    const r = ref.current.getBoundingClientRect();
    const n = clamp(1 - (e.clientY - r.top - top) / trackH, 0, 1);
    onChange(Math.round((min + n * (max - min)) / step) * step);
  };

  return (
    <div style={{ textAlign: "center" }}>
      <div style={{ fontSize: 16, color: LIGHT, letterSpacing: 1, marginBottom: 5 }}>{label}</div>
      <svg ref={ref} width={SL_W} height={height}
           style={{ display: "block", touchAction: "none",
                    cursor: dragging ? "grabbing" : "pointer" }}
           onPointerDown={e => { e.target.setPointerCapture(e.pointerId); setDragging(true); apply(e); }}
           onPointerMove={e => dragging && apply(e)}
           onPointerUp={() => setDragging(false)}
           onPointerLeave={() => setDragging(false)}>
        <rect x={(SL_W - SL_TRACK) / 2} y={top} width={SL_TRACK} height={trackH}
              rx={SL_TRACK / 2} fill={LIGHT} opacity="0.9" />
        <circle cx={SL_W / 2} cy={cy} r={SL_THUMB}
                fill="rgba(0,0,0,0.55)" stroke="#ffffff" strokeWidth={3.2} />
      </svg>
      <div style={{ fontSize: 15, color: LIGHT, marginTop: 5 }}>{value.toFixed(2)}</div>
    </div>
  );
}

export default function ZedLabFM() {
  const [presetIdx, setPresetIdx] = useState(0);
  const [ops, setOps] = useState(() => cloneOps(FACTORY[0].ops));
  const [algIdx, setAlgIdx] = useState(FACTORY[0].alg);
  const [selOp, setSelOp] = useState(3);
  const [drag, setDrag] = useState(null);
  const [phX, setPhX] = useState(null);
  const svgRef = useRef(null);
  const opsRef = useRef(ops);
  opsRef.current = ops;

  const alg = ALGORITHMS[algIdx];
  const isCarrier = i => alg.carriers.includes(i);

  const setOp = (i, patch) =>
    setOps(p => p.map((o, j) => (j === i ? { ...o, ...patch } : o)));

  const loadPreset = idx => {
    const p = FACTORY[idx];
    setPresetIdx(idx);
    setOps(cloneOps(p.ops));
    setAlgIdx(p.alg);
  };

  const nudgePreset = d => loadPreset((presetIdx + d + FACTORY.length) % FACTORY.length);
  const nudgeAlg = d => setAlgIdx((algIdx + d + ALGORITHMS.length) % ALGORITHMS.length);

  // --- playhead ---
  const note = useRef({ active: false, held: false, t0: 0, rel0: 0 });
  useEffect(() => {
    let raf = 0;
    const tick = () => {
      const n = note.current;
      if (!n.active) { setPhX(null); return; }
      const o = opsRef.current;
      const adEnd = Math.max(...o.map(p => p.a + p.d), 0.01);
      const maxR  = Math.max(...o.map(p => p.r), 0.01);
      const now = performance.now() / 1000;
      if (n.held) {
        const e = now - n.t0;
        if (e < adEnd) setPhX(xAD(e));
        else {
          const f = clamp((e - adEnd) / SUS_SWEEP, 0, 1);
          setPhX(xAD(adEnd) + (xSusEnd - xAD(adEnd)) * f);
        }
      } else {
        const e = now - n.rel0;
        if (e >= maxR) { n.active = false; setPhX(null); return; }
        setPhX(xREL(e));
      }
      raf = requestAnimationFrame(tick);
    };
    raf = requestAnimationFrame(tick);
    return () => cancelAnimationFrame(raf);
  }, [phX === null]);

  const noteOn = () => {
    note.current = { active: true, held: true, t0: performance.now() / 1000, rel0: 0 };
    setPhX(xAD(0));
  };
  const noteOff = () => {
    if (!note.current.active) return;
    note.current.held = false;
    note.current.rel0 = performance.now() / 1000;
  };

  const onMove = e => {
    if (!drag || !svgRef.current) return;
    const r = svgRef.current.getBoundingClientRect();
    const mx = e.clientX - r.left, my = e.clientY - r.top;
    const op = ops[selOp];
    const yv = clamp(1 - (my - PAD) / PLOT_H, 0, 1);
    if (drag === "peak") {
      const t = clamp(((mx - PAD) / (AD_FRAC * PLOT_W)) * AD_WINDOW, 0, op.a + op.d);
      setOp(selOp, { a: t, d: op.a + op.d - t, amp: yv });
    } else if (drag === "susIn") {
      const t = ((mx - PAD) / (AD_FRAC * PLOT_W)) * AD_WINDOW;
      setOp(selOp, {
        d: clamp(t - op.a, 0, AD_WINDOW - op.a),
        s: op.amp <= 0 ? 0 : clamp(yv / op.amp, 0, 1),
      });
    } else if (drag === "end") {
      setOp(selOp, { r: clamp(((mx - xSusEnd) / (REL_FRAC * PLOT_W)) * REL_WINDOW, 0, REL_WINDOW) });
    }
  };

  const tabStyle = i => {
    const on = i === selOp;
    const c = OP_COLOUR[i];
    return {
      flex: 1, height: TAB_H, borderRadius: TAB_H / 2, background: "transparent",
      border: `2px solid ${on ? SELECTED : c}`,
      color: on ? SELECTED : c,
      fontFamily: MONO, fontSize: 19, letterSpacing: 2, cursor: "pointer",
      boxShadow: on ? "0 0 15px #ffffff66" : `0 0 9px ${c}44`,
    };
  };

  const nudgeBtn = {
    background: "transparent", color: "#8e96a8", border: "1px solid #2b3040",
    fontFamily: MONO, fontSize: 16, padding: "2px 8px", cursor: "pointer",
  };

  const bp = breakpoints(ops[selOp]);
  const sliderH = CANVAS_H - 118;
  const ampLabel = isCarrier(selOp) ? "LEVEL" : "DEPTH";

  return (
    <div style={{
      width: PANEL_W, height: PANEL_H, background: "#08080a", fontFamily: MONO,
      color: "#8e8e96", boxSizing: "border-box",
      padding: `${TOP_PAD}px ${EDGE}px 0`, userSelect: "none",
    }}>
      <div style={{ display: "flex", gap: TAB_GAP }}>
        {ops.map((_, i) => (
          <button key={i} style={tabStyle(i)} onClick={() => setSelOp(i)}>OP{i + 1}</button>
        ))}
      </div>

      <div style={{ display: "flex", gap: GAP, marginTop: 14 }}>

        {/* ALGORITHM */}
        <div style={{ width: ALG_COL }}>
          <div style={{ display: "flex", alignItems: "center", justifyContent: "space-between" }}>
            <button style={nudgeBtn} onClick={() => nudgeAlg(-1)}>&lt;</button>
            <div style={{ fontSize: 17, color: LIGHT, letterSpacing: 1 }}>ALG {alg.n}</div>
            <button style={nudgeBtn} onClick={() => nudgeAlg(1)}>&gt;</button>
          </div>
          <div style={{ textAlign: "center", fontSize: 15, color: "#8fa3c8", marginTop: 2 }}>
            {alg.name}
          </div>
          <AlgDiagram alg={alg} selOp={selOp} width={ALG_COL} height={CANVAS_H - 76} />
          <div style={{ textAlign: "center", fontSize: 13, color: "#5b6272" }}>
            {alg.label}
          </div>
        </div>

        {/* ENVELOPES */}
        <svg ref={svgRef} width={CANVAS_W} height={CANVAS_H}
             style={{ display: "block", touchAction: "none",
                      cursor: drag ? "grabbing" : "default" }}
             onPointerMove={onMove} onPointerUp={() => setDrag(null)}
             onPointerLeave={() => setDrag(null)}>
          <defs>
            <filter id="glow" x="-40%" y="-40%" width="180%" height="180%">
              <feGaussianBlur stdDeviation="5" result="b" />
              <feMerge><feMergeNode in="b" /><feMergeNode in="SourceGraphic" /></feMerge>
            </filter>
            <filter id="glowStrong" x="-60%" y="-60%" width="220%" height="220%">
              <feGaussianBlur stdDeviation="10" result="b" />
              <feMerge><feMergeNode in="b" /><feMergeNode in="b" />
                       <feMergeNode in="SourceGraphic" /></feMerge>
            </filter>
          </defs>

          <rect x={xSusStart} y={PAD} width={xSusEnd - xSusStart} height={PLOT_H}
                fill="#ffffff" opacity="0.018" />

          {ops.map((op, i) => i === selOp ? null : (
            <path key={i} d={envPath(op)} fill="none" stroke={OP_COLOUR[i]}
                  strokeWidth={STROKE} strokeLinecap="round" strokeLinejoin="round"
                  opacity="0.7" filter="url(#glow)" />
          ))}
          <path d={envPath(ops[selOp])} fill="none" stroke={OP_COLOUR[selOp]}
                strokeWidth={STROKE * 1.6} strokeLinecap="round" strokeLinejoin="round"
                filter="url(#glowStrong)" />

          {phX !== null && (
            <line x1={phX} y1={PAD - 6} x2={phX} y2={PAD + PLOT_H + 6}
                  stroke="#ff8f2e" strokeWidth="1.8" opacity="0.85" />
          )}

          {[["peak", bp.peak], ["susIn", bp.susIn], ["end", bp.end]].map(([k, p]) => (
            <circle key={k} cx={p.x} cy={p.y} r={17} fill="rgba(0,0,0,0.45)"
                    stroke="#ffffff" strokeWidth={3.2} style={{ cursor: "grab" }}
                    onPointerDown={e => { e.target.setPointerCapture(e.pointerId); setDrag(k); }} />
          ))}
        </svg>

        {/* AMPLITUDE / RATIO */}
        <div style={{ width: SLIDER_COL }}>
          <div style={{ display: "flex", justifyContent: "space-around" }}>
            <VSlider label={ampLabel} value={ops[selOp].amp} min={0} max={1} step={0.01}
                     height={sliderH} onChange={v => setOp(selOp, { amp: v })} />
            <VSlider label="RATIO" value={ops[selOp].ratio} min={0.25} max={12} step={0.25}
                     height={sliderH} onChange={v => setOp(selOp, { ratio: v })} />
          </div>
          <button onPointerDown={noteOn} onPointerUp={noteOff} onPointerLeave={noteOff}
                  style={{
                    width: "100%", marginTop: 10, padding: "7px 0",
                    background: phX !== null ? "#2a1a0c" : "transparent",
                    color: phX !== null ? "#ff8f2e" : "#8e8e96",
                    border: `1px solid ${phX !== null ? "#ff8f2e" : "#2b2b30"}`,
                    fontFamily: MONO, fontSize: 16, letterSpacing: 2, cursor: "pointer",
                  }}>HOLD</button>
        </div>
      </div>

      {/* PRESET BAR */}
      <div style={{ height: STRIP_H, display: "flex", alignItems: "center", gap: 8 }}>
        <button style={nudgeBtn} onClick={() => nudgePreset(-1)}>&lt;</button>
        <div style={{
          minWidth: 210, padding: "3px 12px", background: "#0e1218",
          border: "1px solid #2b3040", fontSize: 17, color: "#9fd6c8",
          letterSpacing: 2, textAlign: "center",
        }}>
          {FACTORY[presetIdx].name}
        </div>
        <button style={nudgeBtn} onClick={() => nudgePreset(1)}>&gt;</button>
        <span style={{ fontSize: 13, color: "#3e3e46", marginLeft: 6 }}>
          {presetIdx + 1}/{FACTORY.length}
        </span>
        <div style={{ flex: 1 }} />
        <span style={{ fontSize: 13, color: "#3e3e46" }}>
          OP{selOp + 1} {isCarrier(selOp) ? "CARRIER" : "MODULATOR"}
        </span>
        <span style={{ fontSize: 13, color: "#3e3e46", marginLeft: 14 }}>
          gradient deferred -- concept: Floyd Steinberg
        </span>
      </div>
    </div>
  );
}
