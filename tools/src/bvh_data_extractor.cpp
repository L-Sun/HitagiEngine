#include "simple_bvh_parser.hpp"

#include <hitagi/core/memory_manager.hpp>
#include <hitagi/core/file_io_manager.hpp>
#include <hitagi/core/thread_manager.hpp>

#include <fmt/core.h>
#include <cxxopts.hpp>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <highfive/H5File.hpp>

#include <stdexcept>
#include <algorithm>

using namespace hitagi;
using namespace hitagi::core;
using namespace hitagi::math;

namespace fs = std::filesystem;
using namespace HighFive;

enum struct Space {
    World,
    Root,
    Local
};

int init() {
#ifndef _DEBUG
    spdlog::set_level(spdlog::level::warn);
#endif
    int result;
    if ((result = g_MemoryManager->Initialize()) != 0) return result;
    if ((result = g_FileIoManager->Initialize()) != 0) return result;
    if ((result = g_ThreadManager->Initialize()) != 0) return result;
    return result;
}

void clean_exit(int exit_code = 0) {
    g_ThreadManager->Finalize();
    g_FileIoManager->Finalize();
    g_MemoryManager->Finalize();
    exit(exit_code);
}

void update_node(const std::shared_ptr<BoneNode>& node, const TRS& trs) {
    auto parent       = node->parent.lock();
    auto parent_space = parent ? parent->transform : mat4f(1.0f);
    node->transform   = parent_space * translate(rotate(mat4f(1.0f), trs.rotation), trs.translation);
};

std::pmr::vector<float> extract_frames(const Animation& anima, Space space_mode, bool standard_quaternion) {
    std::pmr::vector<float> result;
    result.reserve(anima.frames.size() * anima.joints.size() * (3 + 4));

    for (const auto& frame : anima.frames) {
        size_t joint_index = 0;
        for (const auto& joint : anima.joints) {
            switch (space_mode) {
                case Space::World:
                    update_node(joint, frame[joint_index]);
                    break;
                case Space::Root:
                    update_node(joint, joint == anima.joints.front() ? TRS{} : frame[joint_index]);
                    break;
                case Space::Local:
                    update_node(joint, TRS{});
                    break;
            }

            joint_index++;
        }
        for (const auto& joint : anima.joints) {
            auto [translation, rotation, _s] = decompose(joint->transform);
            result.emplace_back(translation.x);
            result.emplace_back(translation.y);
            result.emplace_back(translation.z);
            if (standard_quaternion) {
                result.emplace_back(rotation.w);
                result.emplace_back(rotation.x);
                result.emplace_back(rotation.y);
                result.emplace_back(rotation.z);
            } else {
                result.emplace_back(rotation.x);
                result.emplace_back(rotation.y);
                result.emplace_back(rotation.z);
                result.emplace_back(rotation.w);
            }
        }
    }

    return result;
}

std::pmr::vector<std::size_t> extract_bones(const Animation& anima) {
    std::pmr::vector<std::size_t> result;
    // Bone
    for (std::size_t i = 0; i < anima.joints.size(); i++) {
        auto        joint        = anima.joints[i];
        auto        iter         = std::find(std::begin(anima.joints), std::end(anima.joints), joint->parent.lock());
        std::size_t parent_index = std::distance(std::begin(anima.joints), iter);
        result.emplace_back(parent_index);
    }
    return result;
};

void save_file(const std::filesystem::path& output, std::stringstream& ss) {
    std::ofstream file{output};
    if (!file.is_open()) {
        fmt::print("failed to create file {}\n", output.string());
        return;
    }
    file << ss.rdbuf();

    file.flush();
    file.close();
}

int main(int argc, char** argv) {
    if (int success = init(); success != 0) return success;

    auto logger = spdlog::stdout_color_mt("BVH Extractor");

    cxxopts::Options options(
        "bvh_extractor",
        "Dump and merge all bvh data to hdf5.\n"
        "The data format is num_frames x num_joints x (3 position + 4 quaternion)");
    // clang-format off
    options.add_options()
        ("d,dir",
            "The directory of BVH files.\n"
            "Please note that all files must conatin the same skeleton",
            cxxopts::value<std::string>()
        )
        ("f,filter",
            "Apply a filter to the BVH files list.",
            cxxopts::value<std::string>()->default_value("*")
        )
        ("s,space", 
            "The space for joint data is located in",
            cxxopts::value<std::string>()->default_value("root"),
            "accept value: root(default), world, parent"
        )
        ("standard_quat",
            "Use (w,x,y,z) to presentate quaternion, since the format in Hitagi Engine is (x,y,z,w)."
        )
        ("o,output",
            "The path of output files",
            cxxopts::value<std::string>()
        )
        ("h,help", "Print usage")
    ;
    // clang-format on

    auto args = options.parse(argc, argv);

    if (args["help"].count() == 1) {
        fmt::print("{}", options.help());
        clean_exit();
    }

    fs::path   bvh_dir, output;
    std::regex filter;
    Space      space_mode;
    bool       standard_quat = false;

    try {
        bvh_dir       = args["dir"].as<std::string>();
        filter        = std::regex{args["filter"].as<std::string>()};
        output        = args["output"].as<std::string>();
        standard_quat = args["standard_quat"].count() != 0;

        if (auto space = args["space"].as<std::string>();
            space == "root") {
            space_mode = Space::Root;
        } else if (space == "world") {
            space_mode = Space::World;
        } else if (space == "parent") {
            space_mode = Space::Local;
        } else {
            throw std::invalid_argument(fmt::format("unkown space: {}!", space));
        }

        if (!fs::exists(bvh_dir) || !fs::is_directory(bvh_dir)) {
            throw cxxopts::option_syntax_exception(fmt::format("\"{}\" is not a directory", bvh_dir.string()));
        }

    } catch (const fs::filesystem_error& err) {
        logger->error("{}\n", err.what());
        clean_exit(1);
    } catch (const cxxopts::OptionException& ex) {
        logger->error("{}\n", ex.what());
        clean_exit(1);
    } catch (const std::exception& ex) {
        logger->error("{}\n", ex.what());
        clean_exit(1);
    }

    std::pmr::vector<fs::path> all_bvh_files;
    {
        fs::directory_iterator dir_iter{bvh_dir};
        std::copy_if(
            fs::begin(dir_iter), fs::end(dir_iter),
            std::back_inserter(all_bvh_files),
            [filter](const fs::directory_entry& entry) {
                return entry.is_regular_file() && std::regex_match(entry.path().filename().string(), filter);
            });
    }

    if (all_bvh_files.empty()) {
        logger->warn("No file is processing! exit.");
        clean_exit();
    }

    // Get the first bones data
    float                         frame_rate = 0;
    std::pmr::vector<std::size_t> bones;
    {
        auto anima = parse_bvh(g_FileIoManager->SyncOpenAndReadBinary(all_bvh_files.front()));
        if (anima.has_value()) {
            bones      = extract_bones(anima.value());
            frame_rate = anima->frame_rate;
        } else {
            logger->error("Can not parse the first file: {}", all_bvh_files.front().string());
            clean_exit(1);
        }
    }

    // extract data paralle
    std::pmr::vector<std::future<std::optional<std::pmr::vector<float>>>> jobs;

    std::transform(
        all_bvh_files.begin(), all_bvh_files.end(),
        std::back_inserter(jobs), [=, &bones](const fs::path& path) {
            return g_ThreadManager->RunTask([&]() -> std::optional<std::pmr::vector<float>> {
                auto anima = parse_bvh(g_FileIoManager->SyncOpenAndReadBinary(path));
                if (!anima.has_value()) {
                    logger->warn("Can not parse and skip the file: {}", path.string());
                    return std::nullopt;
                }

                const auto _bones = extract_bones(anima.value());
                if (!std::equal(bones.begin(), bones.end(), _bones.begin(), _bones.end()) ||
                    std::abs(frame_rate - anima->frame_rate) > 1e-4) {
                    logger->warn("Skip the file: ", path.string());
                    logger->warn("reason: The configuration of the current BVH is diffrent from the fist file.");
                    return std::nullopt;
                }

                return extract_frames(anima.value(), space_mode, standard_quat);
            });
        });

    File file(output.string(), File::ReadWrite | File::Create | File::Truncate);

    DataSpace          dataspace({1000, bones.size(), (3 + 4)}, {DataSpace::UNLIMITED, bones.size(), (3 + 4)});
    DataSetCreateProps props;
    props.add(Chunking(std::vector<std::size_t>{300, bones.size(), (3 + 4)}));

    DataSet dataset = file.createDataSet<float>("/motion", dataspace, props);
    dataset.createAttribute("frame_rate", frame_rate);

    std::size_t frames = 0;
    for (auto& job : jobs) {
        job.wait();
        auto data = job.get();
        if (data.has_value()) {
            std::size_t new_frames = data.value().size() / (bones.size() * (3 + 4));
            dataset.resize({frames + new_frames, bones.size(), (3 + 4)});
            dataset.select({frames, 0, 0}, {new_frames, bones.size(), (3 + 4)}).write_raw(data.value().data());
            frames += new_frames;
        }
    }

    clean_exit();
}
