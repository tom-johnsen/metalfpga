# VCD Viewer Notes

MetalFPGA emits standard VCD, including real-valued signals. Some third-party
viewers either ignore real types or render them poorly, which can make traces
look flat or missing even when the VCD is correct. This document is the plan
for a native macOS waveform viewer to remove that dependency.

## Apple App Semantics (SwiftUI)

Use SwiftUI App/Scene semantics for a modern macOS app:
- Main window uses `WindowGroup` for the primary UI.
- Document-based workflow uses `DocumentGroup` for opening VCD files and
  leveraging macOS document behaviors (Open Recent, windowing, autosave).
- Menu and commands use SwiftUI `Commands` with placements (notably
  `CommandGroupPlacement.importExport` for file import/export actions).
- Preferences use a dedicated `Settings` scene on macOS.

These are based on SwiftUI docs for WindowGroup/DocumentGroup, command group
placements, and Settings scenes.

## Product Goals

- Trustworthy: accurately displays 2-state, 4-state, and real signals.
- Fast: opens large VCDs without freezing; interactive zoom/pan stays smooth.
- Discoverable: common actions are visible (Open, Export, Print, Play, Step).
- Professional: fits macOS conventions and keyboard shortcuts.

## MVP Features (Must-Haves)

File workflow:
- Open VCD (File > Open, drag/drop onto window).
- Open Recent (document-based app behavior).
- Export (File > Export): CSV for selected signals, PNG/PDF for waveform view.
- Print current view (File > Print).

Waveform interaction:
- Zoom in/out, pan, and seek.
- Play/Pause simulation time cursor.
- Step forward/backward (one tick, one event, or fixed time quantum).
- Scrubbable time ruler with a visible time cursor.
- Signal list with search/filter and per-signal value readout at cursor.
- Support for real signals with numeric formatting controls.

UI layout:
- Left: signal tree (hierarchy + filter + pin/favorites).
- Right: waveform pane.
- Bottom: overview/minimap timeline for quick seek.
- Toolbar: Open, Export, Print, Play/Pause, Step, Zoom, Search.

## UX Details

Menus and shortcuts:
- File: Open (Cmd+O), Close, Export (Cmd+E), Print (Cmd+P).
- View: Zoom In/Out (Cmd+= / Cmd+-), Zoom to Fit, Toggle Minimap.
- Navigate: Jump to time, Next/Prev edge, Next/Prev X/Z.
- Playback: Play/Pause (Space), Step Forward (Right), Step Backward (Left).

Waveform interactions:
- Scroll wheel/pinch to zoom; drag background to pan.
- Click to place cursor; drag cursor to scrub.
- Shift-drag to select time range for zoom/export.

Signal interactions:
- Expand/collapse scopes; filter by name or by toggling activity.
- Add/remove signals from view quickly (double-click to pin).
- Optional bus formatting (bin/hex/dec) and real precision control.

## Data Model

Document:
- File path, timescale, parse index, signal tree, and value-change list.
- Time cursor and view state persisted with the document.

Signal:
- Type (wire, reg, real), width, scope path, and change list.
- Cache last value for fast cursor reads.

## Architecture Sketch

Parsing:
- Streaming VCD parser with incremental indexing (time -> list of changes).
- Memory-map large files and build a sparse index for fast seeks.
- Background parse task with progress UI and cancel option.

Rendering:
- Tile-based waveform renderer; only draw visible time range and rows.
- Cache downsampled levels for fast zoom transitions.
- Use Core Animation or Metal for large datasets if needed.

Export:
- CSV: signal values at sampled intervals or event-based export.
- Image/PDF: current viewport, or selected time range.

Print:
- AppKit print pipeline (NSPrintOperation) for waveform view.

## MVP Milestones

Phase 0: App shell
- SwiftUI App with WindowGroup + DocumentGroup.
- Basic menu/toolbar with Open/Export/Print entries.

Phase 1: Parser
- Streaming VCD parser, signal tree, time index, basic cursor readout.

Phase 2: Waveform view
- Draw single-bit and vector signals, zoom/pan/seek, cursor.

Phase 3: Playback
- Play/Pause, Step (event and fixed-time step), time ruler.

Phase 4: Export/Print
- Export CSV and image/PDF, Print view.

## Pro Viewer Extensions (Post-MVP)

Analysis:
- Measurement cursors with delta readout.
- Edge and pattern search (e.g., find X->0 transitions).
- Trigger rules and bookmarks.

Visualization:
- Glitch/highlight mode, unknown-state heatmap.
- Bus decode (signed/unsigned, fixed-point, IEEE 754 display).
- Multi-cursor compare and signal grouping.

Performance:
- Adaptive level-of-detail based on zoom.
- Persistent indexes per VCD file for fast reopen.

Integration:
- Session files (saved layouts, signal groups).
- External scripting hooks (future).

## Open Questions

- Document-based app vs. single-window with manual file loading?
- Use SwiftUI-only or embed AppKit for advanced waveform rendering?
- Preferred export formats beyond CSV/PDF (FST, JSON)?
