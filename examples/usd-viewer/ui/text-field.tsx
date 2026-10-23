import { createEffect, createMemo, createSignal, For, onCleanup, Show } from "solid-js";
import { View, Text } from "@pocketjs/framework/solid/components";
import { getOps } from "@pocketjs/framework/host";
import { TextEdit, TextInputHost, type TextEvent } from "./text-edit";

export function TextField(props: {
  host: TextInputHost;
  value: string;
  identity: string;
  disabled?: boolean;
  placeholder?: string;
  // Logical field rectangle supplied by the owning layout (also used for IME).
  rect: [number, number, number, number];
  visible?: boolean;
  onCommit(value: string, identity: string): void;
}) {
  const [revision, update] = createSignal(0);
  const [active, setActive] = createSignal(false);
  let edit = new TextEdit(props.value),
    session = 0,
    identity = props.identity;
  const measure = (value: string) => getOps().measureText(value, 0);
  const finish = (accept: boolean) => {
    if (!session) return;
    const value = edit.value,
      target = identity;
    props.host.end(session);
    session = 0;
    setActive(false);
    edit.endComposition();
    getOps().svcSend?.("hitagi.text 0 0 0 0 0 0 0");
    if (accept && value !== props.value) props.onCommit(value, target);
    update((v) => v + 1);
  };
  const receive = (event: TextEvent) => {
    const result = edit.receive(event);
    update((v) => v + 1);
    if (result) finish(result === "accept");
  };
  createEffect(() => {
    const value = props.value,
      nextIdentity = props.identity;
    if (identity !== nextIdentity || props.disabled || props.visible === false) finish(false);
    if (!active()) {
      identity = nextIdentity;
      edit = new TextEdit(value);
      update((v) => v + 1);
    }
  });
  onCleanup(() => finish(false));
  const display = createMemo(() => {
    revision();
    return active() ? edit.display : props.value;
  });
  const caret = createMemo(() => {
    revision();
    return measure(display().slice(0, edit.displayCaret));
  });
  const scroll = createMemo(() =>
    active() ? Math.max(0, caret() - Math.max(0, props.rect[2] - 18)) : 0,
  );
  createEffect(() => {
    revision();
    if (!active()) return;
    const [x, y, w, h] = props.rect;
    getOps().svcSend?.(
      `hitagi.text ${session} ${x} ${y} ${w} ${h} ${x + 6 + caret() - scroll()} ${y + h}`,
    );
  });
  const compositionRuns = createMemo(() => {
    revision();
    if (!edit.composing) return [];
    const runs: { x: number; width: number; target: boolean }[] = [];
    let offset = 0;
    for (const scalar of edit.preedit) {
      const start = edit.from + offset;
      runs.push({
        x: measure(display().slice(0, start)),
        width: measure(scalar),
        target: edit.attributes[offset] === 1 || edit.attributes[offset] === 3,
      });
      offset += scalar.length;
    }
    return runs;
  });
  const selection = createMemo(() => {
    revision();
    return {
      x: measure(edit.value.slice(0, edit.from)),
      width: edit.composing ? 0 : measure(edit.value.slice(edit.from, edit.to)),
    };
  });
  return (
    <View
      class="relative overflow-hidden rounded"
      style={{ height: 28, width: props.rect[2], bgColor: active() ? "#25213a" : "#171a21" }}
      focusable={!props.disabled}
      onPress={() => {
        if (props.disabled || active()) return;
        edit = new TextEdit(props.value);
        identity = props.identity;
        session = props.host.begin(receive);
        setActive(true);
        update((v) => v + 1);
      }}
    >
      <Show when={active() && selection().width > 0}>
        <View
          class="absolute"
          style={{
            insetL: 6 + selection().x - scroll(),
            insetT: 4,
            height: 20,
            width: selection().width,
            bgColor: "#49416c",
          }}
        />
      </Show>
      <Text
        class="absolute text-xs"
        style={{
          insetL: 6 - scroll(),
          insetT: 6,
          textColor: props.disabled || !display() ? "#8c9bad" : "#dce1eb",
        }}
      >
        {display() || (!active() ? props.placeholder : "")}
      </Text>
      <For each={compositionRuns()}>
        {(run) => (
          <View
            class="absolute"
            style={{
              insetL: 6 + run.x - scroll(),
              insetT: 24,
              width: run.width,
              height: run.target ? 2 : 1,
              bgColor: run.target ? "#b2a6ff" : "#939cab",
            }}
          />
        )}
      </For>
      <Show when={active()}>
        <View
          class="absolute"
          style={{
            insetL: 6 + caret() - scroll(),
            insetT: 5,
            width: 1,
            height: 18,
            bgColor: "#b2a6ff",
          }}
        />
      </Show>
    </View>
  );
}
