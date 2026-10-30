export module motionbricks.recording;
import std;
import asset;
import viewer;

export namespace hitagi::examples::motionbricks {
// GPU readback stays on the render thread. PNG compression and MP4 encoding
// run off-thread; at most one image is queued, so a slow disk cannot grow RAM.
class Recording {
public:
    void Toggle() {
        Poll();
        if (active) { Stop(); return; }
        if (encoding.valid()) return;
        const auto stamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        directory = std::filesystem::path("temp/motionbricks/recordings") / std::to_string(stamp);
        std::filesystem::create_directories(directory);
        manifest.open(directory / "frames.ffconcat");
        manifest << "ffconcat version 1.0\n" << std::setprecision(9);
        trace.open(directory / "state.csv");
        trace << "frame,time_s,dt_s,input_x,input_z,jog,reset,inferences,root_x,root_y,root_z,camera_x,camera_y,camera_z\n" << std::setprecision(9);
        joints.open(directory / "joints.csv");
        model.open(directory / "model.jsonl");
        model << std::setprecision(9);
        joints << "frame,joint,x,y,z,qx,qy,qz,qw\n" << std::setprecision(9);
        if (!manifest || !trace || !joints || !model) throw std::runtime_error("Cannot create recording files");
        started = std::chrono::steady_clock::now();
        count = 0; last_time = -1; active = true;
        status = "REC | F9 stop";
    }
    void Poll() {
        if (encoding.valid() && encoding.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            const int result = encoding.get();
            status = result == 0 ? "Saved recording | F9 record" : "MP4 failed; PNGs saved | F9 record";
            std::println("Recording {}: {}", result == 0 ? "saved" : "encoding failed", directory.string());
        }
    }
    template<class WriteState>
    void Capture(USDViewer& viewer, WriteState write_state) {
        Poll();
        if (!active) return;
        const double time = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
        if (last_time >= 0 && time - last_time < 1.0 / 30) return;
        if (image.valid()) {
            if (image.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
            image.get();
        }
        auto pixels = viewer.Capture();
        if (!count) { width = pixels.width + pixels.width % 2; height = pixels.height + pixels.height % 2; }
        const auto name = std::format("frame-{:06}.png", count);
        if (count) manifest << "duration " << time - last_time << '\n';
        manifest << "file '" << name << "'\noption framerate 1000\n";
        write_state(trace, joints, model, count, time);
        trace.flush(); joints.flush(); model.flush(); manifest.flush();
        image = std::async(std::launch::async, [pixels = std::move(pixels), path = directory / name]() mutable {
            const auto png = asset::PngEncoder{}.Encode(pixels);
            std::ofstream file(path, std::ios::binary);
            file.write(reinterpret_cast<const char*>(png.GetData()), png.GetDataSize());
            if (!file) throw std::runtime_error("Recording image write failed");
        });
        last_time = time; ++count;
        status = std::format("REC {:.1f}s | F9 stop", time);
    }
    void Stop() {
        if (!active) return;
        active = false;
        if (image.valid()) image.get();
        if (count) manifest << "duration 0.033333333\nfile '" << std::format("frame-{:06}.png", count - 1) << "'\noption framerate 1000\n";
        manifest.close(); trace.close(); joints.close(); model.close();
        if (!count) { status = "No frames | F9 record"; return; }
        status = "Saving MP4...";
        // Only internally generated numeric paths enter this command.
        const auto folder = directory.generic_string();
        encoding = std::async(std::launch::async, [folder, width = width, height = height] {
            const auto command = std::format("ffmpeg -hide_banner -loglevel error -y -safe 0 -f concat -i \"{0}/frames.ffconcat\" -vf \"scale={1}:{2}:force_original_aspect_ratio=decrease,pad={1}:{2}:(ow-iw)/2:(oh-ih)/2,setsar=1\" -fps_mode vfr -c:v libx264 -preset fast -crf 18 -pix_fmt yuv420p -movflags +faststart \"{0}/video.mp4\" > \"{0}/encode.log\" 2>&1", folder, width, height);
            return std::system(command.c_str());
        });
    }
    void Finish() { Stop(); if (encoding.valid()) { encoding.wait(); Poll(); } }
    auto Status() const -> const std::string& { return status; }
    auto Directory() const -> const std::filesystem::path& { return directory; }
private:
    bool active = false;
    unsigned count = 0;
    unsigned width = 0, height = 0;
    double last_time = -1;
    std::chrono::steady_clock::time_point started;
    std::filesystem::path directory;
    std::ofstream manifest, trace, joints, model;
    std::future<void> image;
    std::future<int> encoding;
    std::string status = "F9 record";
};
}
