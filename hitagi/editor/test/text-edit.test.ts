import { expect, test } from "bun:test";
import { TextEdit, TextInputHost, type TextEvent } from "../ui/text-edit";
const event = (event: TextEvent["event"], data: Partial<TextEvent> = {}): TextEvent => ({ kind: "text", session: 1, event, ...data });

test("composition updates replace preedit without changing the draft", () => {
  const edit = new TextEdit("old");
  edit.receive(event("start"));
  edit.receive(event("update", { text: "ni", caret: 2 }));
  expect(edit.value).toBe("old"); expect(edit.display).toBe("ni");
  edit.receive(event("update", { text: "nihao", caret: 3, attributes: [0, 0, 1, 1, 1] }));
  expect(edit.display).toBe("nihao"); expect(edit.displayCaret).toBe(3);
  edit.receive(event("commit", { text: "你好" }));
  edit.receive(event("end"));
  expect(edit.value).toBe("你好"); expect(edit.preedit).toBe("");
  edit.receive(event("key", { key: 90, control: true }));
  expect(edit.value).toBe("old");
});
test("cancelled composition preserves the selected original text", () => {
  const edit = new TextEdit("abc");
  edit.receive(event("start")); edit.receive(event("update", { text: "拼音" })); edit.receive(event("end"));
  expect(edit.value).toBe("abc"); expect(edit.display).toBe("abc");
});
test("a result followed by a new composition in the same batch preserves both", () => {
  const edit = new TextEdit("");
  edit.receive(event("start")); edit.receive(event("commit", { text: "你" }));
  edit.receive(event("update", { text: "hao", caret: 3 }));
  expect(edit.value).toBe("你"); expect(edit.display).toBe("你hao");
  edit.receive(event("commit", { text: "好" })); edit.receive(event("end"));
  expect(edit.value).toBe("你好");
});
test("editing and submission keys do not act on a composition", () => {
  const edit = new TextEdit("old"); edit.receive(event("start"));
  expect(edit.receive(event("key", { key: 13 }))).toBeUndefined();
  edit.receive(event("key", { key: 8 })); expect(edit.value).toBe("old");
});
test("supplementary characters survive caret movement and deletion", () => {
  const edit = new TextEdit(""); edit.receive(event("commit", { text: "a😀b" }));
  edit.receive(event("key", { key: 37 })); edit.receive(event("key", { key: 8 }));
  expect(edit.value).toBe("ab"); expect(edit.caret).toBe(1);
});
test("empty result never deletes a selection", () => {
  const edit = new TextEdit("abc"); edit.receive(event("commit", { text: "" }));
  expect(edit.value).toBe("abc");
});
test("blur discards preedit while accepting the committed draft", () => {
  const edit = new TextEdit("abc"); edit.receive(event("start")); edit.receive(event("update", { text: "ni" }));
  expect(edit.receive(event("blur"))).toBe("accept"); expect(edit.value).toBe("abc"); expect(edit.composing).toBe(false);
});
test("switching fields rejects delayed events for the former session", () => {
  const host = new TextInputHost(), first: TextEvent[] = [], second: TextEvent[] = [];
  const old = host.begin(e => first.push(e)), current = host.begin(e => second.push(e));
  host.receive(event("commit", { session: old, text: "late" }));
  host.receive(event("commit", { session: current, text: "new" }));
  expect(first.map(e => e.event)).toEqual(["blur"]); expect(second.map(e => e.text)).toEqual(["new"]);
  host.end(old); host.receive(event("end", { session: current })); expect(second.length).toBe(2);
});
