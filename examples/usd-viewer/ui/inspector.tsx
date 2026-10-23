import { For, Show } from "solid-js";
import { Button, Label, Panel, Paragraph, colors, command, short } from "./components";
import type { ViewerState } from "./types";

export function Inspector(p: { x: number; width: number; height: number; state: ViewerState }) {
  return (
    <>
      <Panel rect={[p.x, 94, p.width, p.height - 126]} />
      <Label rect={[p.x + 18, 112, p.width - 36, 22]} color={colors.text}>
        INSPECTOR
      </Label>
      <Show
        when={p.state.inspector}
        fallback={
          <>
            <Label rect={[p.x + 18, 158, p.width - 36, 24]} color={colors.text}>
              Nothing selected
            </Label>
            <Paragraph
              rect={[p.x + 18, 188, p.width - 36, 80]}
              text="Choose an object in the scene tree to inspect its geometry and material."
            />
          </>
        }
      >
        <Label rect={[p.x + 18, 152, p.width - 36, 28]} large color={colors.text}>
          {short(p.state.inspector!.name, 22)}
        </Label>
        <Label rect={[p.x + 18, 186, p.width - 36, 22]}>
          {p.state.inspector!.type} / {p.state.inspector!.triangles} triangles
        </Label>
        <Button
          id="selection.frame"
          label="Frame"
          rect={[p.x + 18, 218, 94, 30]}
          action={() => command("fit")}
        />
        <Button
          id="selection.visibility"
          label={p.state.inspector!.visible ? "Hide" : "Show"}
          rect={[p.x + 120, 218, p.width - 138, 30]}
          action={() => command("visibility")}
        />
        <Label rect={[p.x + 18, 274, p.width - 36, 20]}>LOCAL TRANSFORM</Label>
        <For each={["position", "scale"] as const}>
          {(field, group) => (
            <>
              <Label rect={[p.x + 18, 306 + group() * 62, p.width - 36, 20]} color={colors.text}>
                {field === "position" ? "Position" : "Scale"}
              </Label>
              <For each={[0, 1, 2]}>
                {(axis) => (
                  <Label
                    rect={[
                      p.x + 18 + (axis * (p.width - 36)) / 3,
                      332 + group() * 62,
                      (p.width - 36) / 3,
                      20,
                    ]}
                  >
                    {["X", "Y", "Z"][axis]} {p.state.inspector![field][axis].toFixed(2)}
                  </Label>
                )}
              </For>
            </>
          )}
        </For>
        <Label rect={[p.x + 18, 442, p.width - 36, 20]}>MATERIAL</Label>
        <Label rect={[p.x + 18, 470, p.width - 36, 48]} color={colors.text}>
          {short(p.state.inspector!.materials || "No material", 54)}
        </Label>
      </Show>
    </>
  );
}
