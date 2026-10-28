#include "test_macros.hpp"
#ifdef _WIN32
import std;
import interop.gtest;
import interop.dx12;
import gfx;
import gfx.dx12;
using namespace hitagi::gfx;

static auto NativeList(const CommandContext& context) -> const void* {
    switch (context.GetType()) {
        case CommandType::Graphics: return static_cast<const DX12GraphicsCommandList&>(context).storage_entry->list.Get();
        case CommandType::Compute: return static_cast<const DX12ComputeCommandList&>(context).storage_entry->list.Get();
        case CommandType::Copy: return static_cast<const DX12CopyCommandList&>(context).storage_entry->list.Get();
        default: return nullptr;
    }
}

TEST(DX12CommandReuseTest, RequiresWrapperReleaseAndCompletionOnActualQueue) {
    auto device = create_device(Device::Type::DX12, "NativeCommandReuse");
    CommandQueues owners(*device), submitters(*device);
    auto bindings = BindlessUtils::Create(*device);
    for (auto type : {CommandType::Graphics, CommandType::Compute, CommandType::Copy}) {
        auto acquire = [&] { return CommandContext::Create(*device, owners, *bindings, type, "reuse"); };
        auto first = acquire();
        const auto native = NativeList(*first);
        first->Begin(); first->End();
        auto gate = Fence::Create(*device);
        auto& queue = submitters.Get(type);
        queue.Submit({{std::cref(*first)}}, {{FenceWaitInfo{.fence = *gate, .value = 1}}});
        first.reset();
        auto second = acquire();
        EXPECT_NE(NativeList(*second), native); // returned, but still on GPU
        gate->Signal(1); queue.WaitIdle();
        auto third = acquire();
        EXPECT_EQ(NativeList(*third), native);
        third->Begin(); third->End();
        queue.Submit({{std::cref(*third)}});
        queue.WaitIdle();
        auto fourth = acquire();
        EXPECT_NE(NativeList(*fourth), native); // GPU done, wrapper still alive
        third.reset();
        auto fifth = acquire();
        EXPECT_EQ(NativeList(*fifth), native);
        fifth->Begin(); fifth->End();
        queue.Submit({{std::cref(*fifth)}});
        queue.WaitIdle();
    }
}

TEST(DX12CommandReuseTest, AbandonedRecordingIsClosedAndReusable) {
    auto device = create_device(Device::Type::DX12, "AbandonedCommands");
    CommandQueues queues(*device);
    auto bindings = BindlessUtils::Create(*device);
    for (auto type : {CommandType::Graphics, CommandType::Compute, CommandType::Copy}) {
        auto first = CommandContext::Create(*device, queues, *bindings, type);
        const auto native = NativeList(*first);
        first->Begin();
        first.reset();
        auto second = CommandContext::Create(*device, queues, *bindings, type);
        EXPECT_EQ(NativeList(*second), native);
        second->Begin(); second->End();
        queues.Get(type).Submit({{std::cref(*second)}});
        queues.Get(type).WaitIdle();
    }
}
TEST(DX12CommandReuseTest, StorageDestructionWaitsForPendingCompletion) {
    auto device = create_device(Device::Type::DX12, "StorageRetirement");
    auto& backend = static_cast<DX12Device&>(*device);
    auto fence = Fence::Create(*device);
    auto storage = std::make_shared<DX12CommandStorage>();
    auto entry = storage->Acquire(*backend.GetDevice().Get(), CommandType::Copy);
    storage->Submitted(entry.get(), static_cast<DX12Fence&>(*fence).GetFence(), 1);
    storage->Return(std::move(entry), false);
    std::promise<void> started;
    auto ready = started.get_future();
    auto destroy = std::async(std::launch::async, [storage = std::move(storage), &started]() mutable {
        started.set_value();
        storage.reset();
    });
    ready.wait();
    EXPECT_EQ(destroy.wait_for(std::chrono::milliseconds(20)), std::future_status::timeout);
    fence->Signal(1);
    EXPECT_EQ(destroy.wait_for(std::chrono::seconds(5)), std::future_status::ready);
    destroy.get();
}
TEST(DX12CommandReuseTest, NewFrameCollectsOutOfOrderAndRequiresEveryQueueFence) {
    auto device = create_device(Device::Type::DX12, "PendingCommands");
    auto& backend = static_cast<DX12Device&>(*device);
    CommandQueues queues(*device);
    auto storage = static_cast<DX12CommandQueue&>(queues.Get(CommandType::Copy)).GetCommandStorage();
    auto first = storage->Acquire(*backend.GetDevice().Get(), CommandType::Copy);
    auto second = storage->Acquire(*backend.GetDevice().Get(), CommandType::Copy);
    const auto first_native = first->list.Get();
    const auto second_native = second->list.Get();
    auto slow = Fence::Create(*device), fast = Fence::Create(*device);
    storage->Submitted(first.get(), static_cast<DX12Fence&>(*slow).GetFence(), 1);
    storage->Submitted(first.get(), static_cast<DX12Fence&>(*fast).GetFence(), 1);
    storage->Submitted(first.get(), static_cast<DX12Fence&>(*fast).GetFence(), 2);
    EXPECT_EQ(first->submissions.size(), 2);
    storage->Submitted(second.get(), static_cast<DX12Fence&>(*fast).GetFence(), 1);
    storage->Return(std::move(second), false);
    storage->Return(std::move(first), false); // unfinished head must not block second
    fast->Signal(1);
    queues.NewFrame();
    auto ready = storage->Acquire(*backend.GetDevice().Get(), CommandType::Copy);
    EXPECT_EQ(ready->list.Get(), second_native);
    slow->Signal(1);
    queues.NewFrame(); // first still awaits fast == 2
    auto extra = storage->Acquire(*backend.GetDevice().Get(), CommandType::Copy);
    EXPECT_NE(extra->list.Get(), first_native);
    fast->Signal(2);
    queues.NewFrame();
    auto all_done = storage->Acquire(*backend.GetDevice().Get(), CommandType::Copy);
    EXPECT_EQ(all_done->list.Get(), first_native);
    storage->Return(std::move(ready), false);
    storage->Return(std::move(extra), false);
    storage->Return(std::move(all_done), false);
}
#endif
