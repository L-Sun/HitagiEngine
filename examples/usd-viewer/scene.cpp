export module viewer:scene;

import std;
import asset;
import core;

export namespace hitagi::examples {
auto LoadScene(const std::filesystem::path& path, core::JobSystem& job_system) -> std::shared_ptr<asset::Scene>;
}  // namespace hitagi::examples
