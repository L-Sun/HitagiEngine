---
description: Using git to commit current changes
match:
  - commit
  - git commit
---

Using git to commit current changes with the Commit Rule following these steps, and as fast as possible without any explanations.

# Steps

1. According to the staged changes existing or not,
   - If there are staged changes, read the staged changes using `git --no-pager diff --cached`
   - If there are not staged changes, read the changes in the current branch using `git --no-pager diff`
2. According to the changes content, the changes may be committed with one or more commits.
3. For each commit, generate a commit message using the Commit Rule.
4. Commit the changes with the generated commit message.

# Commit Rule

Follow the commit message format: `<gitmoji> <scope?> <message>`

## Format Structure

- **gitmoji**: A required gitmoji indicating the type of change
- **scope**: Optional module/area name (e.g., `gfx:`, `asset:`, `render_graph:`)
- **message**: Descriptive commit message in present tense

## Gitmoji Reference

| Gitmoji | Code            | Description                        |
| ------- | --------------- | ---------------------------------- |
| ✨      | `:sparkles:`    | New feature                        |
| 🐛      | `:bug:`         | Bug fix                            |
| ♻️      | `:recycle:`     | Refactor / restructure code        |
| ⚡      | `:zap:`         | Performance improvement            |
| ⬆️      | `:arrow_up:`    | Upgrade dependencies               |
| 💚      | `:green_heart:` | Fix CI/build                       |
| ✏️      | `:pencil2:`     | Fix typo                           |
| 🔧      | `:wrench:`      | Configuration changes              |
| 📝      | `:memo:`        | Documentation                      |
| 🎨      | `:art:`         | Improve structure / format of code |
| 🔥      | `:fire:`        | Remove code or files               |
| 🏗️      | `:building_construction:` | Architectural changes    |

## Examples

- `✨ simple material viewer`
- `🐛 fix uninitialized RT/DS on D3D12MA heaps with CREATE_NOT_ZEROED`
- `♻️ modernize gfx: Vulkan submit2, std::ranges, scoped_lock; fix DX12 barriers`
- `⬆️ update sdl2 to sdl3`
- `♻️ refactor ecs interface`
- `⚡ use GPU Upload Heap (DX12) and VK_EXT_host_image_copy (Vulkan) for resource init`

## Rules

1. Always start with an appropriate gitmoji
2. Optionally include a scope prefix (e.g., `gfx:`, `asset:`) when the change affects a specific module
3. Keep the message concise but descriptive
4. Use present tense ("Add feature" not "Added feature")
5. Lowercase the message (except proper nouns like API names, DX12, Vulkan, etc.)
