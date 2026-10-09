export module snake_demo;
import interop.imgui;

import engine;
import std;

export namespace hitagi::snake {

enum struct SnakeDirection : std::uint8_t {
    Up,
    Down,
    Left,
    Right,
};

enum struct SnakeRunState : std::uint8_t {
    Running,
    Paused,
    GameOver,
};

struct SnakeBoardSize {
    int width  = 24;
    int height = 18;
};

class SnakeGameState {
public:
    explicit SnakeGameState(SnakeBoardSize board_size = {});

    void Reset();
    void SetDirection(SnakeDirection direction) noexcept;
    void SetPaused(bool paused) noexcept;
    void TogglePaused() noexcept;
    void Step();

    [[nodiscard]] auto GetBoardSize() const noexcept -> SnakeBoardSize { return m_BoardSize; }
    [[nodiscard]] auto GetSnake() const noexcept -> const std::deque<math::vec2i>& { return m_Snake; }
    [[nodiscard]] auto GetFood() const noexcept -> math::vec2i { return m_Food; }
    [[nodiscard]] auto GetScore() const noexcept -> int { return m_Score; }
    [[nodiscard]] auto GetRunState() const noexcept -> SnakeRunState { return m_RunState; }
    [[nodiscard]] auto GetDirection() const noexcept -> SnakeDirection { return m_Direction; }

private:
    [[nodiscard]] bool Contains(math::vec2i cell) const noexcept;
    void SpawnFood();

    SnakeBoardSize          m_BoardSize;
    std::deque<math::vec2i> m_Snake;
    math::vec2i             m_Food{};
    SnakeDirection          m_Direction     = SnakeDirection::Right;
    SnakeDirection          m_NextDirection = SnakeDirection::Right;
    SnakeRunState           m_RunState      = SnakeRunState::Running;
    int                     m_Score         = 0;
    std::mt19937            m_Random{0x48697461u};
};

class SnakeGameModule final : public core::RuntimeModule {
public:
    explicit SnakeGameModule(Engine& engine);
    void Tick() final;

private:
    void HandleInput();
    void DrawGame();
    void RenderGui();

    Engine&        m_Engine;
    SnakeGameState m_State;
    double         m_Accumulator = 0.0;
    double         m_StepSeconds = 0.12;
};

class SnakeEditorPanel final : public core::RuntimeModule {
public:
    explicit SnakeEditorPanel(Engine& engine);
    void Tick() final;

private:
    Engine&        m_Engine;
    SnakeGameState m_State;
};

}  // namespace hitagi::snake

namespace hitagi::snake {
namespace {

auto DirectionVector(SnakeDirection direction) noexcept -> math::vec2i {
    switch (direction) {
        case SnakeDirection::Up:
            return {0, -1};
        case SnakeDirection::Down:
            return {0, 1};
        case SnakeDirection::Left:
            return {-1, 0};
        case SnakeDirection::Right:
            return {1, 0};
    }
    return {1, 0};
}

auto Opposite(SnakeDirection lhs, SnakeDirection rhs) noexcept -> bool {
    const auto a = DirectionVector(lhs);
    const auto b = DirectionVector(rhs);
    return a.x + b.x == 0 && a.y + b.y == 0;
}

auto RunStateName(SnakeRunState state) noexcept -> const char* {
    switch (state) {
        case SnakeRunState::Running:
            return "Running";
        case SnakeRunState::Paused:
            return "Paused";
        case SnakeRunState::GameOver:
            return "Game Over";
    }
    return "Unknown";
}

void DrawSnakeBoard(const SnakeGameState& state, float max_size) {
    const auto board = state.GetBoardSize();
    const auto cell  = std::floor(std::max(8.0f, std::min(max_size / static_cast<float>(board.width), max_size / static_cast<float>(board.height))));
    const auto size  = ImVec2{cell * static_cast<float>(board.width), cell * static_cast<float>(board.height)};

    ImGui::InvisibleButton("##SnakeBoard", size);
    const auto origin = ImGui::GetItemRectMin();
    auto*      draw   = ImGui::GetWindowDrawList();

    const auto bg     = interop::imgui_color(19, 23, 29, 255);
    const auto grid   = interop::imgui_color(45, 52, 62, 255);
    const auto snake  = interop::imgui_color(78, 190, 132, 255);
    const auto head   = interop::imgui_color(122, 230, 166, 255);
    const auto food   = interop::imgui_color(235, 83, 83, 255);

    draw->AddRectFilled(origin, {origin.x + size.x, origin.y + size.y}, bg, 3.0f);
    for (int x = 0; x <= board.width; ++x) {
        const auto px = origin.x + static_cast<float>(x) * cell;
        draw->AddLine({px, origin.y}, {px, origin.y + size.y}, grid);
    }
    for (int y = 0; y <= board.height; ++y) {
        const auto py = origin.y + static_cast<float>(y) * cell;
        draw->AddLine({origin.x, py}, {origin.x + size.x, py}, grid);
    }

    const auto draw_cell = [&](math::vec2i cell_pos, ImU32 color) {
        const auto min = ImVec2{
            origin.x + static_cast<float>(cell_pos.x) * cell + 1.0f,
            origin.y + static_cast<float>(cell_pos.y) * cell + 1.0f,
        };
        const auto max = ImVec2{min.x + cell - 2.0f, min.y + cell - 2.0f};
        draw->AddRectFilled(min, max, color, 3.0f);
    };

    draw_cell(state.GetFood(), food);
    const auto& body = state.GetSnake();
    for (std::size_t i = 0; i < body.size(); ++i) {
        draw_cell(body[i], i == 0 ? head : snake);
    }
}

void DrawSnakeHud(SnakeGameState& state, bool editable) {
    ImGui::Text("Score: %d", state.GetScore());
    ImGui::SameLine();
    ImGui::Text("State: %s", RunStateName(state.GetRunState()));

    if (ImGui::Button("Reset")) state.Reset();
    ImGui::SameLine();
    if (ImGui::Button(state.GetRunState() == SnakeRunState::Paused ? "Resume" : "Pause")) state.TogglePaused();

    if (editable) {
        ImGui::SameLine();
        if (ImGui::Button("Step")) state.Step();
    }
}

}  // namespace

SnakeGameState::SnakeGameState(SnakeBoardSize board_size)
    : m_BoardSize(board_size) {
    Reset();
}

void SnakeGameState::Reset() {
    m_Snake.clear();
    const auto center = math::vec2i{m_BoardSize.width / 2, m_BoardSize.height / 2};
    m_Snake.emplace_back(center);
    m_Snake.emplace_back(center - math::vec2i{1, 0});
    m_Snake.emplace_back(center - math::vec2i{2, 0});
    m_Direction     = SnakeDirection::Right;
    m_NextDirection = SnakeDirection::Right;
    m_RunState      = SnakeRunState::Running;
    m_Score         = 0;
    SpawnFood();
}

void SnakeGameState::SetDirection(SnakeDirection direction) noexcept {
    if (!Opposite(m_Direction, direction)) m_NextDirection = direction;
}

void SnakeGameState::SetPaused(bool paused) noexcept {
    if (m_RunState == SnakeRunState::GameOver) return;
    m_RunState = paused ? SnakeRunState::Paused : SnakeRunState::Running;
}

void SnakeGameState::TogglePaused() noexcept {
    SetPaused(m_RunState == SnakeRunState::Running);
}

void SnakeGameState::Step() {
    if (m_RunState != SnakeRunState::Running) return;

    m_Direction = m_NextDirection;
    const auto head = m_Snake.front() + DirectionVector(m_Direction);
    if (head.x < 0 || head.y < 0 || head.x >= m_BoardSize.width || head.y >= m_BoardSize.height || Contains(head)) {
        m_RunState = SnakeRunState::GameOver;
        return;
    }

    m_Snake.emplace_front(head);
    if (head == m_Food) {
        ++m_Score;
        SpawnFood();
    } else {
        m_Snake.pop_back();
    }
}

bool SnakeGameState::Contains(math::vec2i cell) const noexcept {
    return std::ranges::find(m_Snake, cell) != m_Snake.end();
}

void SnakeGameState::SpawnFood() {
    std::uniform_int_distribution<int> x_dist(0, m_BoardSize.width - 1);
    std::uniform_int_distribution<int> y_dist(0, m_BoardSize.height - 1);
    do {
        m_Food = {x_dist(m_Random), y_dist(m_Random)};
    } while (Contains(m_Food));
}

SnakeGameModule::SnakeGameModule(Engine& engine)
    : core::RuntimeModule("SnakeGame"),
      m_Engine(engine) {}

void SnakeGameModule::Tick() {
    HandleInput();

    m_Accumulator += m_Engine.GetDeltaTime().count();
    while (m_Accumulator >= m_StepSeconds) {
        m_State.Step();
        m_Accumulator -= m_StepSeconds;
    }

    DrawGame();
    RenderGui();
}

void SnakeGameModule::HandleInput() {
    auto& input = m_Engine.App().GetInputManager();
    if (input.GetBoolNew(hid::VirtualKeyCode::KEY_UP) || input.GetBoolNew(hid::VirtualKeyCode::KEY_W)) m_State.SetDirection(SnakeDirection::Up);
    if (input.GetBoolNew(hid::VirtualKeyCode::KEY_DOWN) || input.GetBoolNew(hid::VirtualKeyCode::KEY_S)) m_State.SetDirection(SnakeDirection::Down);
    if (input.GetBoolNew(hid::VirtualKeyCode::KEY_LEFT) || input.GetBoolNew(hid::VirtualKeyCode::KEY_A)) m_State.SetDirection(SnakeDirection::Left);
    if (input.GetBoolNew(hid::VirtualKeyCode::KEY_RIGHT) || input.GetBoolNew(hid::VirtualKeyCode::KEY_D)) m_State.SetDirection(SnakeDirection::Right);
    if (input.GetBoolNew(hid::VirtualKeyCode::KEY_SPACE)) m_State.TogglePaused();
    if (input.GetBoolNew(hid::VirtualKeyCode::KEY_R)) m_State.Reset();
}

void SnakeGameModule::DrawGame() {
    m_Engine.GuiManager().DrawGui([this] {
        ImGui::SetNextWindowPos({24.0f, 24.0f}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize({640.0f, 620.0f}, ImGuiCond_FirstUseEver);
        ImGui::Begin("Snake");
        DrawSnakeHud(m_State, false);
        ImGui::Separator();
        DrawSnakeBoard(m_State, std::min(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y));
        ImGui::End();
    });
}

void SnakeGameModule::RenderGui() {
    auto& render_runtime = m_Engine.RenderRuntime();
    auto& render_graph   = render_runtime.GetRenderGraph();
    const auto target = render_graph.Create(
        gfx::TextureDesc{
            .name        = "Snake Output",
            .width       = render_runtime.GetSwapChain().GetWidth(),
            .height      = render_runtime.GetSwapChain().GetHeight(),
            .format      = gfx::Format::R8G8B8A8_UNORM,
            .clear_value = math::Color{0.04f, 0.05f, 0.06f, 1.0f},
            .usages      = gfx::TextureUsageFlags::RenderTarget | gfx::TextureUsageFlags::CopySrc,
        });
    render_runtime.RenderGui(target, m_Engine.GuiManager().GetDrawData(), true);
    render_runtime.ToSwapChain(target);
}

SnakeEditorPanel::SnakeEditorPanel(Engine& engine)
    : core::RuntimeModule("SnakeEditorPanel"),
      m_Engine(engine) {}

void SnakeEditorPanel::Tick() {
    m_Engine.GuiManager().DrawGui([this] {
        ImGui::SetNextWindowSize({360.0f, 420.0f}, ImGuiCond_FirstUseEver);
        if (const auto* viewport_window = ImGui::FindWindowByName("Viewport");
            viewport_window != nullptr && viewport_window->DockId != 0) {
            ImGui::SetNextWindowDockID(viewport_window->DockId, ImGuiCond_Appearing);
        }
        ImGui::Begin("Snake Game");
        DrawSnakeHud(m_State, true);
        ImGui::Separator();
        DrawSnakeBoard(m_State, std::min(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y));
        ImGui::End();
    });
}

}  // namespace hitagi::snake
