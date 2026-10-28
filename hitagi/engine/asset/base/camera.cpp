export module asset.base:camera;
import math;

export namespace hitagi::asset {
// Pose belongs to the entity's Transform; camera local forward is -Z, up is +Y.
struct CameraComponent {
    float aspect         = 16.0f / 9.0f;
    float near_clip      = 1.0f;
    float far_clip       = 1000.0f;
    float horizontal_fov = 60.0_deg;
};
}  // namespace hitagi::asset
