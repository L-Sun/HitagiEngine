import { createEffect, createMemo, createSignal, For, Show, onCleanup } from "solid-js";
import { mount } from "@pocketjs/framework/solid";
import { View } from "@pocketjs/framework/solid/components";
import { hostViewport, getOps } from "@pocketjs/framework/host";
import { connectOverlay } from "@pocketjs/framework/overlay-host";
import {
  Button,
  Label,
  Panel,
  Paragraph,
  colors,
  command,
  effect,
  short,
  registerControl,
  type Rect,
} from "./components";
import { TextInputHost } from "./text-edit";
import { TextField } from "./text-field";
import { Inspector } from "./inspector";
import type { ViewerState } from "./types";

function Viewer() {
  const initial = hostViewport(getOps());
  const [size, setSize] = createSignal({ w: initial?.w || 960, h: initial?.h || 600 });
  const [snapshot, setState] = createSignal<ViewerState>({
    kind: "viewer",
    path: "",
    file: "",
    renderer: "pbr",
    backend: "",
    fps: 0,
    debug: 0,
    light: 1,
    selected: "",
    nodes: [],
    inspector: null,
    meshes: 0,
    visible: 0,
    triangles: 0,
    error: "",
    status: "Starting",
    animations: 0,
    physicsBodies: 0,
    physicsBackend: "",
    physics: false,
    playing: true,
    animationTime: 0,
    animationDuration: 0,
  });
  // Track individual fields: advancing the timeline must not invalidate every
  // layout/control expression that reads an unchanged part of the snapshot.
  const viewerState = {} as ViewerState;
  for (const key of Object.keys(snapshot()) as (keyof ViewerState)[]) {
    const value = createMemo(() => snapshot()[key]);
    Object.defineProperty(viewerState, key, { get: value });
  }
  const state = () => viewerState;
  const [opening, setOpening] = createSignal(false);
  const [draft, setDraft] = createSignal("");
  const [search, setSearch] = createSignal("");
  const [offset, setOffset] = createSignal(0);
  const [collapsed, setCollapsed] = createSignal(new Set<string>());
  const text = new TextInputHost();
  let lastPath = "";
  let lastRoot = "";
  const bridge = connectOverlay<Record<string, unknown>, never>((value) => {
    if (text.receive(value)) return;
    if (value.kind === "viewer") {
      const next = value as unknown as ViewerState;
      if (next.path !== lastPath || (next.nodes[0]?.id || "") !== lastRoot) {
        lastRoot = next.nodes[0]?.id || "";
        lastPath = next.path;
        setOpening(false);
        setDraft(next.path);
        setSearch("");
        setOffset(0);
        setCollapsed(
          new Set(
            next.nodes.filter((node) => !node.mesh && node.depth >= 2).map((node) => node.id),
          ),
        );
      }
      if (next.error && next.error !== state().error) setOpening(true);
      // Keep scene-row identity across frame snapshots. Replacing a pressed row
      // between pointer-down and pointer-up would cancel its activation.
      const previous = new Map(state().nodes.map((node) => [node.id, node]));
      const oldNodes = state().nodes;
      next.nodes = next.nodes.map((node) => {
        const old = previous.get(node.id);
        return old &&
          old.name === node.name &&
          old.parent === node.parent &&
          old.depth === node.depth &&
          old.mesh === node.mesh &&
          old.visible === node.visible
          ? old
          : node;
      });
      if (next.nodes.length === oldNodes.length && next.nodes.every((node, index) => node === oldNodes[index]))
        next.nodes = oldNodes;
      setState(next);
    } else if (value.kind === "viewport")
      setSize({ w: Number(value.width), h: Number(value.height) });
    else if (value.kind === "wheel" && Number(value.x) < 236 && Number(value.y) > 180)
      setOffset((v) =>
        Math.max(
          0,
          Math.min(v - Math.sign(Number(value.delta)) * 3, Math.max(0, nodes().length - rows())),
        ),
      );
  });
  onCleanup(() => bridge.dispose());
  const right = () => (size().w >= 900 ? 248 : 0);
  const left = () => Math.min(236, Math.max(180, size().w * 0.25));
  const viewport = (): Rect => [
    left() + 12,
    112,
    Math.max(1, size().w - left() - right() - 24),
    Math.max(1, size().h - 180 - (state().animations ? 40 : 0)),
  ];
  registerControl(
    () => "viewport",
    viewport,
    () => false,
  );
  createEffect(() => effect("layout " + viewport().join(" ")));
  const rows = () => Math.max(1, Math.floor((size().h - 276) / 34));
  const nodes = createMemo(() => {
    const term = search().toLowerCase();
    const hidden = new Set<string>();
    return state().nodes.filter((node) => {
      if (term) return node.name.toLowerCase().includes(term);
      const ancestorCollapsed = hidden.has(node.parent) || collapsed().has(node.parent);
      if (ancestorCollapsed) hidden.add(node.id);
      return !ancestorCollapsed;
    });
  });
  const toggle = (id: string) =>
    setCollapsed((old) => {
      const next = new Set(old);
      if (next.has(id)) next.delete(id);
      else next.add(id);
      return next;
    });
  function Field(p: {
    id: string;
    rect: Rect;
    value: string;
    placeholder?: string;
    commit: (value: string) => void;
  }) {
    registerControl(
      () => p.id,
      () => p.rect,
    );
    return (
      <View class="absolute" style={{ insetL: p.rect[0], insetT: p.rect[1] }}>
        <TextField
          identity={p.id}
          host={text}
          value={p.value}
          placeholder={p.placeholder}
          rect={p.rect}
          onCommit={p.commit}
        />
      </View>
    );
  }
  return (
    <View class="relative" style={{ width: size().w, height: size().h, bgColor: colors.bg }}>
      <Panel rect={[0, 0, size().w, 52]} />
      <Label rect={[20, 14, 180, 28]} large color={colors.text}>
        HITAGI / USD
      </Label>
      <Label rect={[218, 20, Math.max(0, size().w - 475), 20]}>
        {short(state().file || "No scene", 46)}
      </Label>
      <Button
        id="file.open"
        label={opening() ? "Scene tree" : "Open USD"}
        rect={[size().w - 224, 12, 104, 30]}
        primary
        action={() => setOpening((v) => !v)}
      />
      <Button
        id="file.reload"
        label="Reload"
        rect={[size().w - 110, 12, 90, 30]}
        disabled={!state().path}
        action={() => command("reload")}
      />
      <Label rect={[20, 68, 80, 20]}>RENDERER</Label>
      <Button
        id="renderer.pbr"
        label="PBR"
        rect={[102, 60, 70, 28]}
        selected={state().renderer === "pbr"}
        action={() => command("renderer", "pbr")}
      />
      <Button
        id="renderer.toon"
        label="Toon"
        rect={[178, 60, 70, 28]}
        selected={state().renderer === "toon"}
        action={() => command("renderer", "toon")}
      />
      <Button
        id="renderer.deferred"
        label="Deferred"
        rect={[254, 60, 92, 28]}
        selected={state().renderer === "deferred"}
        action={() => command("renderer", "deferred")}
      />
      <For
        each={
          [
            ["final", "Lit", 0],
            ["color", "Color", 1],
            ["normal", "Normals", 2],
            ["roughness", "Roughness", 4],
          ] as const
        }
      >
        {(mode, i) => (
          <Button
            id={"debug." + mode[0]}
            label={mode[1]}
            rect={[366 + i() * 79, 60, 73, 28]}
            selected={state().debug === mode[2]}
            disabled={state().renderer !== "deferred"}
            action={() => command("debug", mode[0])}
          />
        )}
      </For>
      <Label rect={[size().w - 124, 68, 104, 20]} color={colors.accent}>
        {state().backend} / LIVE
      </Label>
      <Panel rect={[0, 94, left(), size().h - 126]} />
      <Show
        when={opening()}
        fallback={
          <>
            <Label rect={[16, 112, left() - 32, 22]} color={colors.text}>
              SCENE EXPLORER
            </Label>
            <Field
              id="tree.search"
              rect={[16, 146, left() - 32, 28]}
              value={search()}
              placeholder="Search objects... [Enter]"
              commit={(value) => {
                setSearch(value);
                setOffset(0);
              }}
            />
            <Show when={nodes().length === 0}>
              <Paragraph
                rect={[16, 194, left() - 32, 72]}
                text="No matching objects. Clear the search to see the scene."
              />
            </Show>
            <For each={nodes().slice(offset(), offset() + rows())}>
              {(node, i) => (
                <>
                  <Button
                    id={"collapse:" + node.id}
                    label={node.mesh ? "M" : collapsed().has(node.id) ? "+" : "-"}
                    rect={[12 + Math.min(node.depth, 3) * 10, 190 + i() * 34, 24, 28]}
                    disabled={node.mesh}
                    action={() => toggle(node.id)}
                  />
                  <Button
                    id={"node:" + node.id}
                    label={
                      (node.visible ? "" : "[off] ") +
                      short(node.name, 19 - Math.min(node.depth, 3))
                    }
                    rect={[
                      40 + Math.min(node.depth, 3) * 10,
                      190 + i() * 34,
                      left() - 52 - Math.min(node.depth, 3) * 10,
                      28,
                    ]}
                    selected={state().selected === node.id}
                    action={() => command("select", node.id)}
                  />
                </>
              )}
            </For>
            <Button
              id="tree.show-all"
              label="Show all objects"
              rect={[16, size().h - 74, left() - 32, 28]}
              action={() => command("show-all")}
            />
          </>
        }
      >
        <Label rect={[16, 112, left() - 32, 22]} color={colors.text}>
          OPEN A SCENE
        </Label>
        <Label rect={[16, 146, left() - 32, 36]}>USD, USDA or USDC path</Label>
        <Field id="file.path" rect={[16, 184, left() - 32, 28]} value={draft()} commit={setDraft} />
        <Button
          id="file.confirm"
          label="Open scene"
          rect={[16, 224, left() - 32, 32]}
          primary
          action={() => {
            command("open", draft());
          }}
        />
        <Label rect={[16, 286, left() - 32, 22]}>SAMPLE SCENES</Label>
        <Button
          id="sample.studio"
          label="Material studio"
          rect={[16, 320, left() - 32, 32]}
          action={() => {
            command("open", "examples/usd-viewer/assets/studio.usda");
            setOpening(false);
          }}
        />
        <Button
          id="sample.texture"
          label="Texture test"
          rect={[16, 362, left() - 32, 32]}
          action={() => {
            command("open", "assets/test/pbr_texture_golden.usda");
            setOpening(false);
          }}
        />
        <Button
          id="sample.skinned"
          label="Skeletal animation"
          rect={[16, 404, left() - 32, 32]}
          action={() => {
            command("open", "examples/usd-viewer/assets/skinned.usda");
            setOpening(false);
          }}
        />
        <Show when={state().error}>
          <Paragraph
            rect={[16, 448, left() - 32, Math.max(32, size().h - 502)]}
            color="#f3a195"
            text={state().error}
          />
        </Show>
      </Show>
      <Show when={right() > 0}>
        <Inspector x={size().w - right()} width={right()} height={size().h} state={state()} />
      </Show>
      <Show when={!state().path}>
        <Label rect={[left() + 40, 180, viewport()[2] - 60, 60]} large color={colors.text}>
          Open a USD scene to begin
        </Label>
      </Show>
      <Show when={state().animations > 0}>
        <Button
          id="animation.toggle"
          label={state().playing ? "Pause" : "Play"}
          rect={[left() + 12, size().h - 98, 68, 28]}
          action={() => command("animation", "toggle")}
        />
        <Button
          id="animation.restart"
          label="Restart"
          rect={[left() + 88, size().h - 98, 74, 28]}
          action={() => command("animation", "restart")}
        />
        <Show when={state().physicsBodies > 0}>
          <Button
            id="physics.toggle"
            label={state().physics ? "Physics: On" : "Physics: Off"}
            rect={[left() + 174, size().h - 98, 106, 28]}
            action={() => command("physics")}
          />
        </Show>
        <Show when={state().physicsBackend}>
          <Label rect={[left() + 290, size().h - 92, 72, 20]}>{state().physicsBackend}</Label>
        </Show>
        <Label rect={[left() + (state().physicsBodies ? 372 : 174), size().h - 92, Math.max(1, viewport()[2] - 290), 20]}>
          {state().animationTime.toFixed(2)} / {state().animationDuration.toFixed(2)} s
        </Label>
      </Show>
      <Button
        id="camera.fit"
        label="Frame [F]"
        rect={[left() + 12, size().h - 58, 88, 24]}
        action={() => command("fit")}
      />
      <Button
        id="camera.home"
        label="Reset"
        rect={[left() + 106, size().h - 58, 58, 24]}
        action={() => command("home")}
      />
      <For each={["front", "right", "top"]}>
        {(view, i) => (
          <Button
            id={"camera." + view}
            label={view}
            rect={[left() + 170 + i() * 56, size().h - 58, 50, 24]}
            action={() => command("view", view)}
          />
        )}
      </For>
      <Show when={viewport()[2] > 460}>
        <Button
          id="light.down"
          label="-"
          rect={[size().w - right() - 112, size().h - 58, 24, 24]}
          action={() => command("light", "down")}
        />
        <Label rect={[size().w - right() - 82, size().h - 54, 42, 20]}>
          {state().light.toFixed(2)}
        </Label>
        <Button
          id="light.up"
          label="+"
          rect={[size().w - right() - 36, size().h - 58, 24, 24]}
          action={() => command("light", "up")}
        />
      </Show>
      <Panel rect={[0, size().h - 30, size().w, 30]} fill="#202936" />
      <Label
        rect={[16, size().h - 23, Math.max(80, size().w - (size().w >= 1200 ? 780 : 490)), 20]}
        color={state().error ? "#f3a195" : colors.accent}
      >
        {state().error ? "Could not open scene" : state().status}
      </Label>
      <Label rect={[size().w - 470, size().h - 23, 240, 20]}>
        {state().visible}/{state().meshes} meshes / {state().triangles} triangles
      </Label>
      <Show when={size().w >= 1200}>
        <Label rect={[size().w - 760, size().h - 23, 278, 20]}>
          LMB orbit / MMB pan / Scroll zoom
        </Label>
      </Show>
      <Label rect={[size().w - 210, size().h - 23, 198, 20]} color={colors.accent}>
        {state().fps > 0
          ? `${state().fps.toFixed(1)} FPS / ${(1000 / state().fps).toFixed(1)} ms`
          : "-- FPS / -- ms"}
      </Label>
    </View>
  );
}

mount(() => <Viewer />);
