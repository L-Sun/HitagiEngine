import { createEffect, createMemo, For, onCleanup, type JSX } from "solid-js";
import { View, Text } from "@pocketjs/framework/solid/components";
import { getOps } from "@pocketjs/framework/host";

export type Rect = [number, number, number, number];
export const box = (r: Rect) => ({ insetL: r[0], insetT: r[1], width: r[2], height: r[3] });
export const colors = {
  bg: "#11151c",
  panel: "#191f29",
  line: "#2c3543",
  text: "#e6edf5",
  muted: "#8c9bad",
  accent: "#7ee2c5",
};
export const short = (s: string, length: number) =>
  s.length > length ? s.slice(0, length - 3) + "..." : s;
export const effect = (command: string) => getOps().svcSend?.("viewer " + command);
export const command = (name: string, value = "") => effect(name + " " + JSON.stringify(value));

export function registerControl(
  id: () => string,
  rect: () => Rect,
  enabled: () => boolean = () => true,
) {
  createEffect(() =>
    effect(`control ${JSON.stringify(id())} ${rect().join(" ")} ${enabled() ? 1 : 0}`),
  );
  onCleanup(() => effect(`remove ${JSON.stringify(id())}`));
}

export function Label(p: { rect: Rect; children: JSX.Element; color?: string; large?: boolean }) {
  return (
    <View class="absolute overflow-hidden" style={box(p.rect)}>
      <Text
        class={p.large ? "text-lg" : "text-xs"}
        style={{ width: p.rect[2], textColor: p.color || colors.muted }}
      >
        {p.children}
      </Text>
    </View>
  );
}

export function Panel(p: { rect: Rect; fill?: string }) {
  return <View class="absolute" style={{ ...box(p.rect), bgColor: p.fill || colors.panel }} />;
}

export function Paragraph(p: { rect: Rect; text: string; color?: string }) {
  const lines = createMemo(() => {
    const result: string[] = [];
    let line = "";
    for (const character of p.text) {
      if (getOps().measureText(line + character, 0) > p.rect[2] && line) {
        const space = line.lastIndexOf(" ");
        result.push(space > 0 ? line.slice(0, space) : line);
        line = space > 0 ? line.slice(space + 1) : "";
      }
      line += character;
    }
    result.push(line);
    return result;
  });
  return (
    <View class="absolute overflow-hidden" style={box(p.rect)}>
      <For each={lines()}>
        {(line, i) => (
          <Text
            class="absolute text-xs"
            style={{ insetT: i() * 18, textColor: p.color || colors.muted }}
          >
            {line}
          </Text>
        )}
      </For>
    </View>
  );
}

export function Button(p: {
  id: string;
  label: string;
  rect: Rect;
  action: () => void;
  selected?: boolean;
  disabled?: boolean;
  primary?: boolean;
}) {
  registerControl(
    () => p.id,
    () => p.rect,
    () => !p.disabled,
  );
  return (
    <View
      class={
        p.primary
          ? "absolute flex-row items-center justify-center rounded bg-[#7ee2c5] focus:bg-[#a0f2db] active:bg-[#55bda0]"
          : p.selected
            ? "absolute flex-row items-center justify-center rounded bg-[#304b4c] focus:bg-[#3c6260]"
            : "absolute flex-row items-center justify-center rounded bg-[#252e3b] focus:bg-[#344359] active:bg-[#40536b]"
      }
      style={{ ...box(p.rect), opacity: p.disabled ? 0.4 : 1 }}
      focusable={!p.disabled}
      onPress={() => {
        if (!p.disabled) p.action();
      }}
    >
      <Text
        class="text-xs"
        style={{ textColor: p.primary ? "#10251f" : p.selected ? colors.accent : colors.text }}
      >
        {p.label}
      </Text>
    </View>
  );
}
