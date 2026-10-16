import { createEffect, createMemo, createSignal, For, Show, onCleanup, onMount, type JSX } from "solid-js";
import { View, Text, type NodeMirror } from "@pocketjs/framework/solid/components";
import { hostViewport, getOps } from "@pocketjs/framework/host";
import { connectOverlay } from "@pocketjs/framework/overlay-host";
import { theme as t } from "./theme";
import { TextInputHost } from "./text-edit";
import { TextField } from "./text-field";

type Node = { id: string; name: string; depth: number };
type State = {
  kind: "editor"; scene: string; mode: string; dirty: boolean;
  canUndo: boolean; canRedo: boolean; selected: string;
  nodes: Node[]; assets: { path: string; kind: string }[];
  transform: Record<string, number[]> | null; status: string; assetError: string; preview: string;
};
type Command = { type: "editor.command"; action: string; [key: string]: unknown };
type Rect = [number, number, number, number];
const clamp = (n: number, min: number, max: number) => Math.max(min, Math.min(n, max));
const box = (r: Rect) => ({ insetL: r[0], insetT: r[1], width: r[2], height: r[3] });
const short = (s: string, n: number) => s.length > n ? s.slice(0, Math.max(1, n - 1)) + "…" : s;
const normalized = (path: string) => path.replaceAll("\\", "/");

// Native state stays authoritative; this component owns layout and presentation.
export function EditorShell(props: { extension?: JSX.Element; receiveExtension?: (value: unknown) => void;
  game?: boolean; gameStatus?: string; inspector?: (send: (action: string, data?: Record<string, unknown>) => void) => JSX.Element }) {
  const initial = hostViewport(getOps());
  const textInput = new TextInputHost();
  const [size, setSize] = createSignal({ w: initial?.w || 960, h: initial?.h || 540 });
  const [leftWidth, setLeftWidth] = createSignal(232);
  const [rightWidth, setRightWidth] = createSignal(296);
  const [assetHeight, setAssetHeight] = createSignal(220);
  const [nodeOffset, setNodeOffset] = createSignal(0);
  const [assetOffset, setAssetOffset] = createSignal(0);
  const [inspectorOffset, setInspectorOffset] = createSignal(0);
  const [collapsed, setCollapsed] = createSignal(new Set<string>());
  const [folder, setFolder] = createSignal("");
  const [folderOffset, setFolderOffset] = createSignal(0);
  const [assetKind, setAssetKind] = createSignal("All");
  const [state, setState] = createSignal<State>({
    kind: "editor", scene: "Connecting...", mode: "Edit", dirty: false, canUndo: false, canRedo: false,
    selected: "", nodes: [], assets: [], transform: null, status: "Waiting for engine", assetError: "", preview: "",
  });
  const layout = createMemo(() => {
    const { w, h } = size();
    const left = props.game || w < 760 ? 0 : clamp(leftWidth(), 180, w * 0.28);
    const right = w < 580 ? 0 : clamp(rightWidth(), 232, w * 0.35);
    const bottom = props.game ? 0 : clamp(assetHeight(), 112, Math.max(112, h * 0.42));
    const bodyBottom = Math.max(84, h - t.status - bottom - t.gap);
    const center = Math.max(1, w - left - right - (left ? t.gap : 0) - (right ? t.gap : 0));
    const scene: Rect = [left ? left + t.gap : 0, 84, center, Math.max(1, bodyBottom - 84)];
    const assets: Rect = [0, bodyBottom + t.gap, Math.max(1, w - right - (right ? t.gap : 0)), bottom];
    const preview: Rect = right && state().preview ? [w - right + 12, Math.max(84, h - t.status - 174), right - 24, 150] : [0, 0, 0, 0];
    return { w, h, left, right, bodyBottom, scene, assets, preview };
  });
  const visibleNodes = createMemo(() => {
    const result: Node[] = [];
    let hiddenDepth = Infinity;
    for (const node of state().nodes) {
      if (node.depth > hiddenDepth) continue;
      hiddenDepth = collapsed().has(node.id) ? node.depth : Infinity;
      result.push(node);
    }
    return result;
  });
  const folders = createMemo(() => [...new Set(state().assets.map(a => normalized(a.path)).filter(p => p.includes("/")).map(p => p.split("/")[0]))].sort());
  const assets = createMemo(() => state().assets.filter(a =>
    (!folder() || normalized(a.path).startsWith(folder() + "/")) &&
    (assetKind() === "All" || a.kind.toLowerCase() === assetKind().toLowerCase())));
  const nodeRows = () => Math.max(1, Math.floor((layout().bodyBottom - t.toolbar - 92) / t.row));
  const assetRows = () => Math.max(1, Math.floor((layout().assets[3] - 100) / t.row));
  const folderRows = () => Math.max(0, Math.floor((layout().assets[3] - 82) / 32));
  const inspectorHeight = () => Math.max(0, layout().h - 108 - (state().preview ? 214 : props.extension ? 228 : 0));
  // Header/actions + three 120px transform groups, their gaps, and bottom padding.
  const inspectorContentHeight = () => 141 + (state().transform ? 3 * 128 : 0);
  const bridge = connectOverlay<Record<string, unknown>, Command>((value) => {
    if (textInput.receive(value)) return;
    if (value.kind === "editor") setState(value as State);
    else if (value.kind === "viewport" && typeof value.width === "number" && typeof value.height === "number")
      setSize({ w: value.width, h: value.height });
    else if (value.kind === "wheel") {
      const x = Number(value.x), y = Number(value.y), delta = -Math.sign(Number(value.delta)) * 3;
      if (x < layout().left && y > 120 && y < layout().bodyBottom)
        setNodeOffset(v => clamp(v + delta, 0, Math.max(0, visibleNodes().length - nodeRows())));
      else if (x > 160 && x < layout().assets[2] && y > layout().assets[1] + 70 && y < layout().h - t.status)
        setAssetOffset(v => clamp(v + delta, 0, Math.max(0, assets().length - assetRows())));
      else if (x >= 0 && x < 160 && y > layout().assets[1] + 40 && y < layout().h - t.status)
        setFolderOffset(v => clamp(v + delta, 0, Math.max(0, folders().length - folderRows())));
      else if (x > layout().w - layout().right && y > 84 && y < 84 + inspectorHeight())
        setInspectorOffset(v => clamp(v + delta * 12, 0, Math.max(0, inspectorContentHeight() - inspectorHeight())));
    } else props.receiveExtension?.(value);
  });
  onCleanup(() => bridge.dispose());
  const send = (action: string, data: Record<string, unknown> = {}) => bridge.send({ type: "editor.command", action, ...data });
  let lastLayout = "";
  createEffect(() => {
    const data = { scene: layout().scene, preview: layout().preview };
    const key = JSON.stringify(data);
    if (key !== lastLayout) { lastLayout = key; send("layout", data); }
  });
  createEffect(() => setNodeOffset(v => clamp(v, 0, Math.max(0, visibleNodes().length - nodeRows()))));
  createEffect(() => setAssetOffset(v => clamp(v, 0, Math.max(0, assets().length - assetRows()))));
  createEffect(() => setFolderOffset(v => clamp(v, 0, Math.max(0, folders().length - folderRows()))));
  createEffect(() => setInspectorOffset(v => clamp(v, 0, Math.max(0, inspectorContentHeight() - inspectorHeight()))));
  const editable = () => !!state().selected && state().mode === "Edit";

  function Label(p: { children: JSX.Element; accent?: boolean }) {
    return <Text class="text-xs" style={{ textColor: p.accent ? t.accent : t.muted }}>{p.children}</Text>;
  }
  function Button(p: { label: string; action: () => void; disabled?: boolean; primary?: boolean; selected?: boolean; width?: number }) {
    return <View
      class={p.disabled ? "flex-row items-center justify-center px-2 rounded bg-[#22252d]" :
        p.primary ? "flex-row items-center justify-center px-3 rounded bg-[#7966d6] focus:bg-[#8c7ae4] active:bg-[#6655bb]" :
        p.selected ? "flex-row items-center justify-center px-2 rounded bg-[#34304e] focus:bg-[#40385e]" :
        "flex-row items-center justify-center px-2 rounded bg-[#252933] focus:bg-[#353b49] active:bg-[#41495a]"}
      style={{ height: 28, ...(p.width ? { width: p.width } : {}) }}
      focusable={!p.disabled} onPress={() => { if (!p.disabled) p.action(); }}>
      <Text class="text-xs" style={{ textColor: p.disabled ? t.dim : p.primary ? "#ffffff" : p.selected ? t.accent : t.text }}>{p.label}</Text>
    </View>;
  }
  function Divider() { return <View style={{ width: 1, height: 20, bgColor: t.border }} />; }
  function ScrollHint(p: { x: number; y: number; height: number; total: number; visible: number; offset: number }) {
    const thumb = () => Math.max(16, p.height * p.visible / Math.max(1, p.total));
    return <Show when={p.total > p.visible && p.height > 16}>
      <View class="absolute" style={{ insetL: p.x, insetT: p.y, width: 2, height: p.height, bgColor: t.border }}>
        <View class="absolute" style={{ insetL: 0, insetT: (p.height - thumb()) * p.offset / Math.max(1, p.total - p.visible), width: 2, height: thumb(), bgColor: t.dim }} />
      </View>
    </Show>;
  }
  function Splitter(p: { rect: Rect; axis: "left" | "right" | "bottom" }) {
    let node!: NodeMirror;
    let origin = 0;
    onMount(() => onCleanup(bridge.drag(node, event => {
      if (event.started) origin = p.axis === "left" ? layout().left : p.axis === "right" ? layout().right : layout().assets[3];
      if (p.axis === "left") setLeftWidth(clamp(origin + event.dx, 180, layout().w * 0.28));
      if (p.axis === "right") setRightWidth(clamp(origin - event.dx, 232, layout().w * 0.35));
      if (p.axis === "bottom") setAssetHeight(clamp(origin - event.dy, 112, Math.max(112, layout().h * 0.42)));
    })));
    return <View ref={node} class="absolute" style={{ ...box(p.rect), bgColor: t.border }} />;
  }
  function TransformGroup(p: { field: string; label: string; unit: string }) {
    return <View class="flex-col gap-1">
      <View class="flex-row items-center" style={{ height: 24 }}><Label>{p.label}</Label><View class="flex-1" /><Label>{p.unit}</Label></View>
      <For each={[0, 1, 2]}>{axis => <View class="flex-row items-center gap-1" style={{ height: 28 }}>
        <Text class="text-xs" style={{ width: 20, textColor: t.axes[axis] }}>{["X", "Y", "Z"][axis]}</Text>
        <View class="flex-1 flex-row items-center px-2 rounded" style={{ height: 28, bgColor: t.field }}>
          <Text class="text-xs" style={{ textColor: t.text }}>{(state().transform?.[p.field]?.[axis] ?? 0).toFixed(3)}</Text>
        </View>
        <Button label="−" width={26} disabled={!editable()} action={() => send("transform", { field: p.field, axis, delta: p.field === "rotation" ? -0.01 : -0.1 })} />
        <Button label="+" width={26} disabled={!editable()} action={() => send("transform", { field: p.field, axis, delta: p.field === "rotation" ? 0.01 : 0.1 })} />
      </View>}</For>
    </View>;
  }

  return <View class="w-full h-full" style={{ bgColor: t.canvas }}>
    <View class="absolute flex-row items-center gap-2 px-3" style={{ insetL: 0, insetT: 0, width: layout().w, height: t.toolbar, bgColor: t.panel }}>
      <View class="items-center justify-center rounded" style={{ width: 24, height: 24, bgColor: t.selected }}><Text class="text-sm font-bold" style={{ textColor: t.accent }}>H</Text></View>
      <Text class="text-xs font-bold" style={{ width: 76, textColor: t.text }}>HITAGI</Text>
      <Show when={!props.game}><Button label="New" action={() => send("new")} /><Divider />
      <Button label="Undo" disabled={!state().canUndo} action={() => send("undo")} />
      <Button label="Redo" disabled={!state().canRedo} action={() => send("redo")} /></Show>
      <View class="flex-1" />
      <Show when={layout().w > 1050}><Label>{short(state().scene, 48)}{state().dirty ? "  •" : ""}</Label><View class="flex-1" /></Show>
      <Button label={state().mode === "Edit" ? "▶  Play" : "■  Stop"} primary action={() => send("play")} />
      <Show when={layout().w > 760}>
        <Button label={state().mode === "Pause" ? "Resume" : "Pause"} disabled={state().mode === "Edit"} action={() => send("pause")} />
        <Button label="Step" disabled={state().mode === "Edit"} action={() => send("step")} /><Divider />
        <Label>{state().mode.toUpperCase()} MODE</Label>
      </Show>
    </View>

    <Show when={layout().left > 0}>
      <View class="absolute flex-col" style={{ insetL: 0, insetT: t.toolbar, width: layout().left, height: layout().bodyBottom - t.toolbar, bgColor: t.panel }}>
        <View class="flex-row items-center px-3" style={{ height: t.header }}><Label accent>Hierarchy</Label><View class="flex-1" /><Label>{state().nodes.length}</Label></View>
        <View class="flex-row items-center gap-2 px-3" style={{ height: 40 }}>
          <Button label="+ Create" disabled={state().mode !== "Edit"} action={() => send("create")} />
          <Button label="Focus" disabled={!state().selected} action={() => send("focus")} />
        </View>
        <For each={visibleNodes().slice(nodeOffset(), nodeOffset() + nodeRows())}>{node =>
          <View class={state().selected === node.id ? "flex-row items-center bg-[#34304e] focus:bg-[#40385e]" : "flex-row items-center focus:bg-[#292e39]"}
            style={{ height: t.row, paddingL: 6 + Math.min(node.depth, 8) * 14 }}
            focusable onPress={() => send("select", { id: node.id })}>
            <View class="items-center justify-center" style={{ width: 20, height: t.row }}
              focusable={state().nodes[state().nodes.indexOf(node) + 1]?.depth > node.depth}
              onPress={() => setCollapsed(old => { const next = new Set(old); next.has(node.id) ? next.delete(node.id) : next.add(node.id); return next; })}>
              <Label>{state().nodes[state().nodes.indexOf(node) + 1]?.depth > node.depth ? collapsed().has(node.id) ? "›" : "⌄" : "·"}</Label>
            </View>
            <Text class="text-xs" style={{ textColor: state().selected === node.id ? t.accent : t.text }}>{short(node.name, Math.floor((layout().left - 38 - node.depth * 14) / 7))}</Text>
          </View>
        }</For>
        <ScrollHint x={layout().left - 4} y={76} height={nodeRows() * t.row} total={visibleNodes().length} visible={nodeRows()} offset={nodeOffset()} />
        <View class="absolute px-3" style={{ insetL: 0, insetB: 8 }}><Label>{visibleNodes().length > nodeRows() ? "Scroll to browse objects" : "Scene objects"}</Label></View>
      </View>
      <Splitter axis="left" rect={[layout().left, t.toolbar, t.gap, layout().bodyBottom - t.toolbar]} />
    </Show>

    <View class="absolute flex-row items-center px-3" style={{ insetL: layout().scene[0], insetT: t.toolbar, width: layout().scene[2], height: t.header, bgColor: t.raised }}>
      <Label accent>{props.game ? "Game / Snake" : "Scene"}</Label><View class="flex-1" /><Show when={layout().scene[2] > 400}><Label>{props.game ? props.gameStatus || "Ready" : "Perspective   /   World"}</Label></Show>
    </View>
    <View class="absolute" style={{ ...box(layout().scene), bgColor: "#14171e" }} />

    <Show when={layout().right > 0}>
      <Splitter axis="right" rect={[layout().w - layout().right - t.gap, t.toolbar, t.gap, layout().h - t.toolbar - t.status]} />
      <View class="absolute flex-col" style={{ insetL: layout().w - layout().right, insetT: t.toolbar, width: layout().right, height: layout().h - t.toolbar - t.status, bgColor: t.panel }}>
        <View class="flex-row items-center px-3" style={{ height: t.header }}><Label accent>{props.game ? "Game settings" : "Inspector"}</Label></View>
        <Show when={props.game} fallback={<>
        <View class="absolute overflow-hidden" style={{ insetL: 0, insetT: 36, width: layout().right, height: inspectorHeight() }}>
        <Show when={state().selected} fallback={<View class="w-full flex-col items-center gap-3" style={{ paddingT: 60 }}>
          <Text class="text-base" style={{ textColor: t.dim }}>◇</Text>
          <Text class="text-sm" style={{ textColor: t.text }}>Nothing selected</Text>
          <Label>Select an object in the scene</Label><Label>or choose one from Hierarchy.</Label>
        </View>}>
          <View class="absolute flex-col gap-2 px-3" style={{ insetL: 0, insetT: -inspectorOffset(), width: layout().right }}>
            <TextField host={textInput} identity={state().selected}
              value={state().nodes.find(n => n.id === state().selected)?.name || "Entity"}
              disabled={!editable()} visible={inspectorOffset() === 0}
              rect={[layout().w - layout().right + 12, 84 - inspectorOffset(), layout().right - 24, 28]}
              onCommit={(name, id) => send("rename", { id, name })} />
            <View class="flex-row gap-2"><Button label="Duplicate" disabled={!editable()} action={() => send("duplicate")} /><Button label="Delete" disabled={!editable()} action={() => send("delete")} /></View>
            <View style={{ height: 1, bgColor: t.border }} />
            <Show when={state().transform}>
              <TransformGroup field="position" label="Position" unit="m" />
              <TransformGroup field="rotation" label="Rotation" unit="rad" />
              <TransformGroup field="scale" label="Scale" unit="" />
            </Show>
            <View style={{ height: 16 }}><Label>ADD COMPONENT</Label></View>
            <View class="flex-row gap-2"><Button label="+ Camera" disabled={!editable()} action={() => send("component", { kind: "camera" })} /><Button label="+ Light" disabled={!editable()} action={() => send("component", { kind: "light" })} /></View>
          </View>
        </Show>
        </View>
        <Show when={state().selected}><ScrollHint x={layout().right - 4} y={36} height={inspectorHeight()} total={inspectorContentHeight()} visible={inspectorHeight()} offset={inspectorOffset()} /></Show>
        <Show when={!!props.extension && !state().preview}><View class="absolute overflow-hidden" style={{ insetL: 8, insetB: 8, width: layout().right - 16, height: 220 }}>{props.extension}</View></Show>
        <Show when={state().preview}><View class="absolute flex-row items-center gap-2 px-3" style={{ insetL: 0, insetB: 178, width: layout().right, height: 32, bgColor: t.panel }}><Label>Texture preview</Label><View class="flex-1" /><Button label="Close" action={() => send("closePreview")} /></View></Show>
        </>}>
          <View class="absolute overflow-hidden px-3" style={{ insetL: 0, insetT: 44, width: layout().right, height: Math.max(0, layout().h - 120) }}>{props.inspector?.(send)}</View>
        </Show>
      </View>
    </Show>

    <Show when={!props.game}><Splitter axis="bottom" rect={[0, layout().assets[1] - t.gap, layout().assets[2], t.gap]} />
    <View class="absolute flex-col" style={{ ...box(layout().assets), bgColor: t.panel }}>
      <View class="flex-row items-center gap-3 px-3" style={{ height: t.header }}>
        <Label accent>Assets</Label><Divider /><Label>Project / {folder() || "All assets"}</Label><View class="flex-1" /><Button label="Refresh" action={() => send("refresh")} />
      </View>
      <View class="absolute flex-col gap-1 px-2" style={{ insetL: 0, insetT: 44, width: 156 }}>
        <Button label="All assets" selected={!folder()} action={() => { setFolder(""); setAssetOffset(0); }} />
        <For each={folders().slice(folderOffset(), folderOffset() + folderRows())}>{name =>
          <Button label={short(name, 18)} selected={folder() === name} action={() => { setFolder(name); setAssetOffset(0); }} />
        }</For>
      </View>
      <View class="absolute" style={{ insetL: 159, insetT: 40, width: 1, height: layout().assets[3] - 48, bgColor: t.border }} />
      <View class="absolute flex-row gap-1" style={{ insetL: 172, insetT: 40 }}>
        <For each={layout().assets[2] > 760 ? ["All", "Scene", "Model", "Material", "Texture"] : ["All", "Scene", "Texture"]}>{kind =>
          <Button label={kind} selected={assetKind() === kind} action={() => { setAssetKind(kind); setAssetOffset(0); }} />
        }</For>
      </View>
      <View class="absolute flex-col" style={{ insetL: 172, insetT: 74, width: Math.max(1, layout().assets[2] - 184) }}>
        <For each={assets().slice(assetOffset(), assetOffset() + assetRows())}>{asset =>
          <View class="flex-row items-center gap-2 px-2 focus:bg-[#303544]" style={{ height: t.row }} focusable onPress={() => send("asset", { path: asset.path })}>
            <Text class="text-xs" style={{ width: 18, textColor: t.accent }}>◇</Text>
            <Text class="text-xs flex-1" style={{ textColor: t.text }}>{short(normalized(asset.path), Math.floor((layout().assets[2] - 330) / 7))}</Text>
            <Text class="text-xs" style={{ width: 70, textColor: t.dim }}>{asset.kind}</Text>
          </View>
        }</For>
        <Show when={assets().length === 0}><Label>{state().assetError || "No assets in this view"}</Label></Show>
      </View>
      <ScrollHint x={layout().assets[2] - 4} y={74} height={assetRows() * t.row} total={assets().length} visible={assetRows()} offset={assetOffset()} />
      <View class="absolute px-3" style={{ insetL: 160, insetB: 6 }}><Label>{assets().length} items{assets().length > assetRows() ? "  ·  Scroll to browse" : ""}</Label></View>
    </View>
    </Show><View class="absolute flex-row items-center px-3" style={{ insetL: 0, insetB: 0, width: layout().w, height: t.status, bgColor: "#14161c" }}>
      <Text class="text-xs" style={{ textColor: t.green }}>●</Text><View style={{ width: 8 }} /><Label>{short(state().status, 90)}</Label><View class="flex-1" />
      <Show when={layout().w > 900}><Label>{props.game ? "Click Game: capture input    Esc: release    F5 / F6 / F10: play / pause / step" : "Middle mouse: orbit    Scroll: zoom    Click: select"}</Label></Show>
    </View>
  </View>;
}
