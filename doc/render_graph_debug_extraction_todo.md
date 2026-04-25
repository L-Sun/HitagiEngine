# Render Graph Debug Extraction Todo

## Goal

Make render graph debug readback a first-class render graph operation. A pass that copies a texture or buffer for visualization must be represented in the graph, must participate in resource state transitions, and must survive pass culling even when it does not feed the swap chain.

## Design

1. Add explicit pass-culling control.
   - Add `AllowPassCulling(bool allow)` to render, compute, and copy pass builders.
   - The default remains cullable so existing render graph behavior does not change.
   - `AllowPassCulling(false)` marks a pass as a side-effect root. Compilation starts dependency DFS from every side-effect root in addition to the present pass.

2. Add render graph extraction APIs.
   - Add `RenderGraph::QueueTextureExtraction(...)` for texture-to-texture debug capture.
   - Add `RenderGraph::QueueBufferExtraction(...)` for texture-to-buffer readback.
   - These APIs build copy passes through the existing `CopyPassBuilder` and mark the copy pass as non-cullable.
   - Extraction APIs import destination resources, so the caller controls lifetime and can read them after GPU completion.

3. Preserve current present semantics.
   - `PresentPass` remains the normal frame root.
   - If the window is minimized, present work is skipped, but side-effect roots may still execute.
   - If the graph has neither a present pass nor side-effect roots, compile succeeds with an empty execution plan.

4. Keep debug capture inside gfx/render graph.
   - Screenshot and validation code should request extraction from RG instead of using OS/window capture.
   - The copied resource is read back through gfx resource mapping/readback paths after the device work is complete.

## Implementation Steps

1. Extend `PassNode` with a culling flag.
   - Default to cullable.
   - Keep the field private/protected and mutate it only through pass builders.

2. Extend pass builders.
   - Add typed `AllowPassCulling(bool allow) noexcept` methods returning the concrete builder type for fluent chaining.
   - Implement shared mutation in `PassBuilder` to match existing builder style.

3. Refactor `RenderGraph::Compile()`.
   - Gather non-cullable pass nodes before early-out checks.
   - Seed DFS from the present pass only when present is valid.
   - Seed DFS from all non-cullable pass nodes.
   - Keep output resource nodes of all essential passes, as the current graph already does.
   - Add a short comment near the root collection logic because this is the critical debug/extraction behavior.

4. Add extraction APIs to `RenderGraph`.
   - Validate source handle and destination resource.
   - Import destination resource.
   - Create the proper copy pass and mark it non-cullable.
   - Return the resulting `CopyPassHandle` so tests and callers can assert validity.

5. Update renderer capture helpers.
   - Make `ForwardRenderer::CopyToTexture()` use `QueueTextureExtraction()`.
   - Make `ForwardRenderer::CopyToBuffer()` use `QueueBufferExtraction()`.

6. Add tests.
   - Verify an unconnected cullable pass does not execute without present.
   - Verify `AllowPassCulling(false)` executes without present.
   - Verify `QueueBufferExtraction()` keeps an upstream producer pass alive without present.
   - Verify a cullable branch disconnected from present remains culled when a present pass exists.

7. Run verification.
   - Build the gfx tests.
   - Run the render graph test filter.
   - Build and run snake validation, then inspect generated PNGs for non-empty pixels.
