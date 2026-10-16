// Platform-neutral single-line edit state. All offsets are UTF-16 code units.
export type TextEvent = {
  kind: "text"; session: number; event: "start" | "update" | "commit" | "end" | "key" | "blur";
  text?: string; caret?: number; attributes?: number[]; clauses?: number[];
  key?: number; shift?: boolean; control?: boolean;
};

export class TextEdit {
  value = "";
  anchor = 0;
  caret = 0;
  composing = false;
  preedit = "";
  preeditCaret = 0;
  attributes: number[] = [];
  clauses: number[] = [];
  private undo: { value: string; caret: number; anchor: number }[] = [];
  private redo: { value: string; caret: number; anchor: number }[] = [];

  constructor(value: string) { this.value = value; this.caret = value.length; }
  get from() { return Math.min(this.anchor, this.caret); }
  get to() { return Math.max(this.anchor, this.caret); }
  get display() { return this.composing ? this.value.slice(0, this.from) + this.preedit + this.value.slice(this.to) : this.value; }
  get displayCaret() { return this.composing ? this.from + this.preeditCaret : this.caret; }
  private snapshot() { return { value: this.value, caret: this.caret, anchor: this.anchor }; }
  private remember() { this.undo.push(this.snapshot()); if (this.undo.length > 100) this.undo.shift(); this.redo = []; }
  endComposition() { this.composing = false; this.preedit = ""; this.preeditCaret = 0; this.attributes = []; this.clauses = []; }
  private boundary(index: number) {
    index = Math.max(0, Math.min(this.value.length, index));
    if (index && index < this.value.length && /[\uDC00-\uDFFF]/.test(this.value[index]) && /[\uD800-\uDBFF]/.test(this.value[index - 1])) index--;
    return index;
  }
  receive(event: TextEvent): "accept" | "cancel" | undefined {
    if (event.event === "start") { this.endComposition(); this.composing = true; }
    else if (event.event === "update") {
      this.composing = true;
      this.preedit = event.text ?? "";
      this.preeditCaret = Math.max(0, Math.min(this.preedit.length, event.caret ?? 0));
      this.attributes = event.attributes ?? [];
      this.clauses = event.clauses ?? [];
    } else if (event.event === "commit") {
      const text = (event.text ?? "").replace(/[\x00-\x1f\x7f]/g, "");
      if (text) {
        this.remember();
        const from = this.from, to = this.to;
        this.value = this.value.slice(0, from) + text + this.value.slice(to);
        this.anchor = this.caret = from + text.length;
      }
      this.endComposition();
    } else if (event.event === "end") this.endComposition();
    else if (event.event === "blur") { this.endComposition(); return "accept"; }
    else if (event.event === "key" && !this.composing) {
      const key = event.key;
      if (key === 13 || key === 9) return "accept";
      if (key === 27) return "cancel";
      if (event.control && key === 65) { this.anchor = 0; this.caret = this.value.length; }
      else if (event.control && (key === 90 || key === 89)) {
        const redo = key === 89 || event.shift;
        const source = redo ? this.redo : this.undo, destination = redo ? this.undo : this.redo;
        const state = source.pop();
        if (state) { destination.push(this.snapshot()); Object.assign(this, state); }
      } else if (key === 37 || key === 39 || key === 36 || key === 35) {
        let next = this.caret;
        if (!event.shift && this.from !== this.to && (key === 37 || key === 39)) next = key === 37 ? this.from : this.to;
        else if (key === 36) next = 0;
        else if (key === 35) next = this.value.length;
        else if (key === 37) next = this.boundary(this.caret - 1);
        else next = this.caret + (this.value.codePointAt(this.caret)! > 0xffff ? 2 : 1);
        this.caret = this.boundary(next);
        if (!event.shift) this.anchor = this.caret;
      } else if (key === 8 || key === 46) {
        if (this.from === this.to && (key === 8 ? this.caret === 0 : this.caret === this.value.length)) return;
        this.remember();
        if (this.from === this.to) {
          if (key === 8) this.anchor = this.boundary(this.caret - 1);
          else this.anchor = this.caret + (this.value.codePointAt(this.caret)! > 0xffff ? 2 : 1);
        }
        const from = this.from;
        this.value = this.value.slice(0, from) + this.value.slice(this.to);
        this.anchor = this.caret = from;
      }
    }
  }
}

// One owner per UI realm. Session ids reject queued events from a prior field.
export class TextInputHost {
  private serial = 0;
  private owner?: { session: number; receive(event: TextEvent): void };
  begin(receive: (event: TextEvent) => void): number {
    this.owner?.receive({ kind: "text", session: this.owner.session, event: "blur" });
    const session = ++this.serial;
    this.owner = { session, receive };
    return session;
  }
  end(session: number) { if (this.owner?.session === session) this.owner = undefined; }
  receive(value: unknown): boolean {
    const event = value as TextEvent;
    if (!event || event.kind !== "text") return false;
    if (event.session === this.owner?.session) this.owner.receive(event);
    return true;
  }
}
