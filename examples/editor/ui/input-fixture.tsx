import { createSignal } from "solid-js";
import { mount } from "@pocketjs/framework/solid";
import { View, Text } from "@pocketjs/framework/components";

function HitagiUI() {
  const [count, setCount] = createSignal(0);
  return (
    <View class="w-full h-full bg-slate-900 p-8 flex-col gap-6">
      <Text class="text-2xl font-bold text-white">Hitagi Engine</Text>
      <Text class="text-lg text-slate-300">PocketJS integration preview</Text>
      <View class="bg-slate-800 rounded-xl p-6 flex-col gap-4">
        <Text class="text-xl text-white">Native UI / Solid components</Text>
        <Text class="text-base text-slate-300">Click the button or use arrows and Enter.</Text>
        <View class="flex-row items-center gap-4">
          <View class="px-6 py-3 bg-blue-600 rounded-lg focus:bg-blue-500 active:bg-blue-700" focusable onPress={() => setCount(count() + 1)}>
            <Text class="text-lg text-white">Increment</Text>
          </View>
          <View class="px-6 py-3 bg-slate-600 rounded-lg focus:bg-slate-500 active:bg-slate-700" focusable onPress={() => setCount(0)}>
            <Text class="text-lg text-white">Reset</Text>
          </View>
          <Text class="text-xl text-white">Count: {count()}</Text>
        </View>
      </View>
      <Text class="text-sm text-slate-400">Legacy editor panels are disabled during the GUI migration.</Text>
    </View>
  );
}

mount(() => <HitagiUI />);
