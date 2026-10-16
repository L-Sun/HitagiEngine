import { createSignal, onCleanup } from "solid-js";
import { mount } from "@pocketjs/framework/solid";
import { View, Text } from "@pocketjs/framework/solid/components";
import { hostViewport, getOps } from "@pocketjs/framework/host";
import { connectOverlay } from "@pocketjs/framework/overlay-host";

function ViewerHud() {
  const [width, setWidth] = createSignal(hostViewport(getOps())?.w || 960);
  const [fps, setFps] = createSignal("--");
  const bridge = connectOverlay<Record<string, unknown>, never>((value) => {
    if (value.kind === "viewer.fps" && typeof value.fps === "number")
      setFps(value.fps.toFixed(1));
    else if (value.kind === "viewport" && typeof value.width === "number")
      setWidth(value.width);
  });
  onCleanup(() => bridge.dispose());
  return (
    <View class="absolute flex-row items-center justify-center rounded-lg bg-slate-900"
      style={{ insetL: Math.max(0, width() - 128), insetT: 12, width: 116, height: 36 }}>
      <Text class="text-sm text-white">FPS {fps()}</Text>
    </View>
  );
}

mount(() => <ViewerHud />);
