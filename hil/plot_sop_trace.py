#!/usr/bin/env python3
"""Plots the Bms_Sop startup trace as an interactive Plotly page.

Reads the CSV that sop_init_hil.py writes and produces one HTML file with six
stacked panels on a shared time axis: published limits, derate factors, cell
voltages, temperature, SOC, and a strip of validity and derate flags. The
page loads plotly.js 3.1.0 from cdnjs, so it needs no Python package, but the
browser needs internet access to open it.

Usage:
  python hil/plot_sop_trace.py [hil/reports/sop_init_trace.csv] [--out hil/reports/sop_init_trace.html]
"""

from __future__ import annotations

import argparse
import csv
import html
import json
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
DEFAULT_CSV = REPO / "hil" / "reports" / "sop_init_trace.csv"

PLOTLY_URL = "https://cdnjs.cloudflare.com/ajax/libs/plotly.js/3.1.0/plotly.min.js"

# Flag strip rows, top to bottom: (CSV column, label, True when the CSV value is ...).
FLAG_ROWS = [
    ("cell_valid",    "CellVoltageValid",        "1"),
    ("temp_valid",    "TemperatureSummaryValid", "1"),
    ("soc_min_valid", "SOC Min.Valid",           "1"),
    ("soc_max_valid", "SOC Max.Valid",           "1"),
    ("init_source",   "SOC init PENDING",        "PENDING"),
    ("vlow",          "VLow derate active",      "1"),
    ("vhigh",         "VHigh derate active",     "1"),
    ("thigh",         "THigh derate active",     "1"),
    ("tlow",          "TLow cut active",         "1"),
    ("inputs_valid",  "SOP InputsValid",         "1"),
]


def load(csv_path: Path) -> list[dict[str, str]]:
    with csv_path.open(newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f))


def series(rows: list[dict[str, str]]) -> dict:
    """Converts the CSV to plot units: limits in A, factors 0 to 1, degC, %, cell mV."""
    def col(name: str, scale: float = 1.0) -> list[float]:
        return [round(float(r[name]) * scale, 3) for r in rows]

    data = {
        "t": col("time_ms"),
        "minCell": col("min_cell_mV"),
        "maxCell": col("max_cell_mV"),
        "temp": col("max_temp_dC", 0.1),
        "socMin": col("soc_min_x10", 0.1),
        "socMax": col("soc_max_x10", 0.1),
        "initSource": [r["init_source"] for r in rows],
        "flagLabels": [label for _, label, _ in FLAG_ROWS],
        "flags": [[1 if r[key] == true_value else 0 for r in rows]
                  for key, _, true_value in FLAG_ROWS],
    }
    # Heatmap cell edges: each flag cell spans from its sample to the next one, like the steps.
    step = (data["t"][-1] - data["t"][0]) / (len(rows) - 1) if len(rows) > 1 else 100.0
    data["tEdges"] = data["t"] + [round(data["t"][-1] + step, 3)]
    for prefix in "DRC":
        data[f"{prefix}final"] = col(f"{prefix}_final", 0.1)
        data[f"{prefix}table"] = col(f"{prefix}_table", 0.1)
        data[f"{prefix}factor"] = col(f"{prefix}_factor", 0.001)
    return data


def events(rows: list[dict[str, str]]) -> list[dict]:
    """Moments worth a vertical marker: only the ones that end the startup sequence."""
    def first(pred) -> dict | None:
        return next((r for r in rows if pred(r)), None)

    marks = []
    cell = first(lambda r: r["cell_valid"] == "1")
    all_valid = first(lambda r: r["cell_valid"] == "1" and r["temp_valid"] == "1"
                      and r["soc_min_valid"] == "1" and r["soc_max_valid"] == "1"
                      and r["init_source"] != "PENDING")
    if cell:
        marks.append({"t": float(cell["time_ms"]),
                      "text": f"Cell voltages valid ({float(cell['time_ms']):.0f} ms)"})
    if all_valid and all_valid is not cell:
        marks.append({"t": float(all_valid["time_ms"]),
                      "text": f"All inputs valid ({float(all_valid['time_ms']):.0f} ms)"})
    return marks


PAGE = """<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Bms_Sop startup trace</title>
<style>
  :root {
    color-scheme: light;
    --page: #f9f9f7;
    --surface-1: #fcfcfb;
    --text-primary: #0b0b0b;
    --text-secondary: #52514e;
    --muted: #898781;
    --grid: #e1e0d9;
    --axis: #c3c2b7;
    --neutral: #f0efec;
    --series-1: #2a78d6;
    --series-2: #eb6834;
    --series-3: #1baf7a;
    --flag-on: #2a78d6;
    --border: rgba(11,11,11,0.10);
  }
  @media (prefers-color-scheme: dark) {
    :root:where(:not([data-theme="light"])) {
      color-scheme: dark;
      --page: #0d0d0d;
      --surface-1: #1a1a19;
      --text-primary: #ffffff;
      --text-secondary: #c3c2b7;
      --muted: #898781;
      --grid: #2c2c2a;
      --axis: #383835;
      --neutral: #383835;
      --series-1: #3987e5;
      --series-2: #d95926;
      --series-3: #199e70;
      --flag-on: #3987e5;
      --border: rgba(255,255,255,0.10);
    }
  }
  :root[data-theme="dark"] {
    color-scheme: dark;
    --page: #0d0d0d;
    --surface-1: #1a1a19;
    --text-primary: #ffffff;
    --text-secondary: #c3c2b7;
    --muted: #898781;
    --grid: #2c2c2a;
    --axis: #383835;
    --neutral: #383835;
    --series-1: #3987e5;
    --series-2: #d95926;
    --series-3: #199e70;
    --flag-on: #3987e5;
    --border: rgba(255,255,255,0.10);
  }
  body {
    margin: 0;
    background: var(--page);
    color: var(--text-primary);
    font: 14px/1.45 system-ui, -apple-system, "Segoe UI", sans-serif;
  }
  main { max-width: 1200px; margin: 0 auto; padding: 24px 20px 40px; }
  h1 { font-size: 20px; margin: 0 0 4px; font-weight: 600; }
  .sub { color: var(--text-secondary); margin: 0 0 16px; }
  .controls { display: flex; gap: 8px; flex-wrap: wrap; margin: 0 0 12px; }
  .controls button {
    font: inherit; color: var(--text-primary); background: var(--surface-1);
    border: 1px solid var(--border); border-radius: 6px; padding: 5px 12px; cursor: pointer;
  }
  .controls button[aria-pressed="true"] { border-color: var(--series-1); color: var(--series-1); }
  .card {
    background: var(--surface-1); border: 1px solid var(--border); border-radius: 10px;
    padding: 8px; overflow-x: auto;
  }
  #plot { min-width: 720px; height: 1180px; }
  .note { color: var(--text-secondary); margin: 12px 0 0; }
  .note a { color: var(--series-1); }
</style>
</head>
<body>
<main>
  <h1>Bms_Sop startup trace</h1>
  <p class="sub">__SUBTITLE__</p>
  <div class="controls" role="group" aria-label="Time range">
    <button type="button" data-range="startup" aria-pressed="true">Startup, 0 to 1.5 s</button>
    <button type="button" data-range="full" aria-pressed="false">Full trace</button>
  </div>
  <div class="card"><div id="plot"></div></div>
  <p class="note">__SAMPLE_NOTE__, drawn as steps because each value holds until the next
  sample. Limits are in A, derate factors run from 0 to 1. Every value is also in the table view:
  <a href="__CSV_NAME__">__CSV_NAME__</a>.</p>
</main>
<script src="__PLOTLY_URL__"></script>
<script>
const DATA = __DATA__;
const EVENTS = __EVENTS__;
const T_END = DATA.tEdges[DATA.tEdges.length - 1];
const X_TITLE = __X_TITLE__;

// Panels top to bottom: [y-axis id, title, domain]. Gaps leave room for each title.
const PANELS = [
  ["y",  "Published limit (A)",       [0.845, 1.000]],
  ["y2", "Derate factor",             [0.700, 0.805]],
  ["y3", "Cell voltage (mV)",         [0.545, 0.660]],
  ["y4", "Max pack temperature (°C)", [0.410, 0.505]],
  ["y5", "SOC (%)",                   [0.290, 0.370]],
  ["y6", "Flags",                     [0.000, 0.250]],
];

function css(name) {
  return getComputedStyle(document.documentElement).getPropertyValue(name).trim();
}

function stepLine(y, name, color, axis, legend, hover, customdata) {
  return {
    type: "scatter", mode: "lines", x: DATA.t, y, name, yaxis: axis, legend,
    line: { shape: "hv", width: 2, color },
    customdata, hovertemplate: hover + "<extra></extra>",
  };
}

function buildFigure(range) {
  const c = {
    s1: css("--series-1"), s2: css("--series-2"), s3: css("--series-3"),
    text: css("--text-primary"), text2: css("--text-secondary"), muted: css("--muted"),
    grid: css("--grid"), axis: css("--axis"), surface: css("--surface-1"),
    neutral: css("--neutral"), on: css("--flag-on"),
  };
  const limit = (p) => DATA.t.map((_, i) => [DATA[p + "table"][i], DATA[p + "factor"][i]]);
  const traces = [
    stepLine(DATA.Dfinal, "Discharge", c.s1, "y", "legend",
             "Discharge %{y:.1f} A (table %{customdata[0]:.1f} A, k %{customdata[1]:.3f})", limit("D")),
    stepLine(DATA.Rfinal, "Regen", c.s2, "y", "legend",
             "Regen %{y:.1f} A (table %{customdata[0]:.1f} A, k %{customdata[1]:.3f})", limit("R")),
    stepLine(DATA.Cfinal, "Charge", c.s3, "y", "legend",
             "Charge %{y:.1f} A (table %{customdata[0]:.1f} A, k %{customdata[1]:.3f})", limit("C")),
    stepLine(DATA.Dfactor, "Discharge", c.s1, "y2", "legend2", "Discharge k %{y:.3f}"),
    stepLine(DATA.Rfactor, "Regen", c.s2, "y2", "legend2", "Regen k %{y:.3f}"),
    stepLine(DATA.Cfactor, "Charge", c.s3, "y2", "legend2", "Charge k %{y:.3f}"),
    stepLine(DATA.minCell, "Min cell", c.s1, "y3", "legend3", "Min cell %{y} mV"),
    stepLine(DATA.maxCell, "Max cell", c.s2, "y3", "legend3", "Max cell %{y} mV"),
    Object.assign(stepLine(DATA.temp, "Max pack temperature", c.s1, "y4", "legend4",
                           "Max temp %{y:.1f} °C"), { showlegend: false }),
    stepLine(DATA.socMin, "SOC min", c.s1, "y5", "legend5", "SOC min %{y:.1f} %"),
    stepLine(DATA.socMax, "SOC max", c.s2, "y5", "legend5", "SOC max %{y:.1f} %"),
    {
      // x holds cell edges, so each cell spans its whole 100 ms step like the lines above.
      type: "heatmap", x: DATA.tEdges, y: DATA.flagLabels, z: DATA.flags,
      yaxis: "y6", showlegend: false,
      zmin: 0, zmax: 1, showscale: false, ygap: 2,
      colorscale: [[0, c.neutral], [1, c.on]],
      text: DATA.flags.map((row) => row.map((v) => (v ? "TRUE" : "FALSE"))),
      hovertemplate: "%{y}: %{text}<extra></extra>",
    },
  ];

  const axisBase = {
    gridcolor: c.grid, linecolor: c.axis, zerolinecolor: c.axis, tickfont: { color: c.muted, size: 11 },
    showline: true, ticks: "", automargin: true,
  };
  const layout = {
    paper_bgcolor: c.surface, plot_bgcolor: c.surface,
    font: { family: 'system-ui, -apple-system, "Segoe UI", sans-serif', color: c.text, size: 12 },
    margin: { l: 70, r: 150, t: 44, b: 50 },
    showlegend: true,
    hovermode: "x unified", hoversubplots: "axis",
    hoverlabel: { bgcolor: c.surface, bordercolor: c.axis, font: { color: c.text } },
    xaxis: Object.assign({}, axisBase, {
      anchor: "y6", domain: [0, 1],
      title: { text: X_TITLE, font: { color: c.text2, size: 12 } },
      range: range === "startup" ? [-50, Math.min(1550, T_END)] : [-50, T_END],
      showspikes: true, spikemode: "across", spikesnap: "cursor", spikecolor: c.muted,
      spikethickness: 1, spikedash: "solid",
    }),
    shapes: [], annotations: [],
  };

  PANELS.forEach(([id, title, domain], i) => {
    const key = id === "y" ? "yaxis" : "yaxis" + id.slice(1);
    layout[key] = Object.assign({}, axisBase, { domain, anchor: "x" });
    if (id === "y" || id === "y2") layout[key].rangemode = "tozero";
    if (id === "y2") layout[key].range = [0, 1.05];
    if (id === "y5") layout[key].range = [0, 100];
    if (id === "y6") Object.assign(layout[key], { autorange: "reversed", showgrid: false, showline: false });
    layout.annotations.push({
      text: title, xref: "paper", yref: "paper", x: 0, y: domain[1], xanchor: "left", yanchor: "bottom",
      showarrow: false, font: { color: c.text2, size: 12 },
    });
    const legendKey = i === 0 ? "legend" : "legend" + (i + 1);
    layout[legendKey] = {
      x: 1.01, xanchor: "left", y: domain[1], yanchor: "top", bgcolor: "rgba(0,0,0,0)",
      font: { color: c.text2, size: 12 },
    };
  });

  EVENTS.forEach((ev, i) => {
    layout.shapes.push({
      type: "line", xref: "x", yref: "paper", x0: ev.t, x1: ev.t, y0: 0, y1: 1.03,
      line: { color: c.muted, width: 1 },
    });
    layout.annotations.push({
      // Above the top panel, so the label never covers a data line.
      text: ev.text, xref: "x", yref: "paper", x: ev.t, y: 1, xanchor: "left", yanchor: "bottom",
      xshift: 4, yshift: 14 + 14 * i, showarrow: false, font: { color: c.text2, size: 11 }, bgcolor: c.surface,
    });
  });
  return { traces, layout };
}

let currentRange = "startup";
function render() {
  const { traces, layout } = buildFigure(currentRange);
  Plotly.react("plot", traces, layout, { responsive: true, displaylogo: false });
}

document.querySelectorAll(".controls button").forEach((button) => {
  button.addEventListener("click", () => {
    currentRange = button.dataset.range;
    document.querySelectorAll(".controls button").forEach((b) =>
      b.setAttribute("aria-pressed", String(b === button)));
    render();
  });
});
try {
  window.matchMedia("(prefers-color-scheme: dark)").addEventListener("change", render);
  new MutationObserver(render).observe(document.documentElement, { attributes: true, attributeFilter: ["data-theme"] });
} catch (e) { /* theme changes are optional */ }
render();
</script>
</body>
</html>
"""


def write_html(csv_path: Path, out_path: Path, subtitle: str | None = None,
               x_title: str = "Time (ms)", sample_note: str = "One row of the CSV per sample") -> Path:
    rows = load(csv_path)
    if not rows:
        raise ValueError(f"{csv_path} has no samples")
    if subtitle is None:
        subtitle = f"{len(rows)} samples from {csv_path.name}"
    page = (PAGE.replace("__PLOTLY_URL__", PLOTLY_URL)
                .replace("__SUBTITLE__", html.escape(subtitle))
                .replace("__CSV_NAME__", html.escape(csv_path.name))
                .replace("__SAMPLE_NOTE__", html.escape(sample_note))
                .replace("__X_TITLE__", json.dumps(x_title))
                .replace("__DATA__", json.dumps(series(rows), separators=(",", ":")))
                .replace("__EVENTS__", json.dumps(events(rows))))
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(page, encoding="utf-8")
    return out_path


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("csv", type=Path, nargs="?", default=DEFAULT_CSV)
    parser.add_argument("--out", type=Path, default=None,
                        help="output HTML (default: the CSV path with .html)")
    args = parser.parse_args()
    out = write_html(args.csv, args.out or args.csv.with_suffix(".html"))
    print(f"Plot: {out}")


if __name__ == "__main__":
    main()
