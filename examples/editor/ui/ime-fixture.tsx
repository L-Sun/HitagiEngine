import { createSignal, onCleanup } from "solid-js";
import { mount } from "@pocketjs/framework/solid";
import { View } from "@pocketjs/framework/solid/components";
import { connectOverlay } from "@pocketjs/framework/overlay-host";
import { TextField } from "../../../hitagi/gui/ui/text-field";
import { TextInputHost } from "../../../hitagi/gui/ui/text-edit";

function Fixture() {
  const host = new TextInputHost();
  const bridge = connectOverlay<unknown, { type: string; value: string; id: string }>(value => host.receive(value));
  onCleanup(() => bridge.dispose());
  const [first, setFirst] = createSignal("First"), [second, setSecond] = createSignal("Second");
  return <View class="w-full h-full" style={{ bgColor: "#17191f" }}>
    <View class="absolute" style={{ insetL: 20, insetT: 20 }}>
      <TextField host={host} identity="first" value={first()} rect={[20, 20, 240, 28]}
        onCommit={(value, id) => { setFirst(value); bridge.send({ type: "text.test", value, id }); }} />
    </View>
    <View class="absolute" style={{ insetL: 20, insetT: 60 }}>
      <TextField host={host} identity="second" value={second()} rect={[20, 60, 240, 28]}
        onCommit={(value, id) => { setSecond(value); bridge.send({ type: "text.test", value, id }); }} />
    </View>
  </View>;
}
mount(() => <Fixture />);
