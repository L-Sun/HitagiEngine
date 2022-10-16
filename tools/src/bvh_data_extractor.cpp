#include "hitagi/math/transform.hpp"
#include "simple_bvh_parser.hpp"
#include "highfive_pmr.hpp"

#include <hitagi/core/memory_manager.hpp>
#include <hitagi/core/file_io_manager.hpp>
#include <hitagi/core/thread_manager.hpp>

#include <fmt/core.h>
#include <cxxopts.hpp>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <iterator>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <algorithm>

using namespace hitagi::core;
using namespace hitagi::math;

namespace fs = std::filesystem;
using namespace HighFive;

auto memory_manager  = std::make_shared<MemoryManager>();
auto file_io_manager = std::make_shared<FileIOManager>();
auto thread_manager  = std::make_shared<ThreadManager>();

int init() {
#ifndef _DEBUG
    spdlog::set_level(spdlog::level::info);
#endif
    hitagi::memory_manager  = memory_manager.get();
    hitagi::file_io_manager = file_io_manager.get();
    hitagi::thread_manager  = thread_manager.get();

    if (!memory_manager->Initialize()) return false;
    if (!file_io_manager->Initialize()) return false;
    if (!thread_manager->Initialize()) return false;

    return true;
}

void clean_exit(int exit_code = 0) {
    thread_manager->Finalize();
    file_io_manager->Finalize();
    memory_manager->Finalize();

    thread_manager.reset();
    file_io_manager.reset();
    memory_manager.reset();

    hitagi::memory_manager  = nullptr;
    hitagi::file_io_manager = nullptr;
    hitagi::thread_manager  = nullptr;

    exit(exit_code);
}

std::pmr::vector<fs::path>         get_all_bvh(const fs::path& dir, const std::regex& filter);
std::pmr::vector<float>            extract_frames(const Animation& anima);
std::pmr::vector<int>              extract_parents(const Animation& anima);
std::pmr::vector<std::pmr::string> extract_joints_name(const Animation& anima);
std::pmr::vector<float>            extract_bones_length(const Animation& anima);

int main(int argc, char** argv) {
    if (!init()) return 1;

    auto logger = spdlog::stdout_color_mt("BVH Extractor");

    cxxopts::Options options(
        "bvh_extractor",
        "Dump and merge all bvh data to hdf5.\n"
        "The data format is num_frames x num_joints x (3 position + 6D rotation)");
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
        ("metric_scale",
            "Apply metric scale to skeleton",
            cxxopts::value<float>()->default_value("1.0")
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
    float      metric_scale = args["metric_scale"].as<float>();

    try {
        bvh_dir = args["dir"].as<std::string>();
        filter  = std::regex{args["filter"].as<std::string>()};
        output  = args["output"].as<std::string>();

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

    auto all_bvh_files = get_all_bvh(bvh_dir, filter);
    if (all_bvh_files.empty()) {
        logger->warn("No file is processing! exit.");
        clean_exit();
    }

    // Get the first bones data
    float                              frame_rate = 0;
    std::pmr::vector<int>              joints_parent;
    std::pmr::vector<std::pmr::string> joints_name;
    std::pmr::vector<float>            bones_length;
    {
        auto anima = parse_bvh(file_io_manager->SyncOpenAndReadBinary(all_bvh_files.front()), metric_scale);
        if (anima.has_value()) {
            joints_parent = extract_parents(anima.value());
            joints_name   = extract_joints_name(anima.value());
            frame_rate    = anima->frame_rate;
            bones_length  = extract_bones_length(anima.value());
        } else {
            logger->error("Can not parse the first file: {}", all_bvh_files.front().string());
            clean_exit(1);
        }
    }

    // extract data paralle
    std::pmr::vector<std::future<std::optional<std::pmr::vector<float>>>> jobs;

    // parallel extract
    std::transform(
        all_bvh_files.begin(), all_bvh_files.end(),
        std::back_inserter(jobs), [=, &joints_parent](const fs::path& path) {
            return thread_manager->RunTask([&]() -> std::optional<std::pmr::vector<float>> {
                auto anima = parse_bvh(file_io_manager->SyncOpenAndReadBinary(path), metric_scale);
                if (!anima.has_value()) {
                    logger->warn("Can not parse and skip the file: {}", path.string());
                    return std::nullopt;
                }

                const auto _bones = extract_parents(anima.value());
                if (!std::equal(joints_parent.begin(), joints_parent.end(), _bones.begin(), _bones.end()) ||
                    std::abs(frame_rate - anima->frame_rate) > 1e-4) {
                    logger->warn("Skip the file: ", path.string());
                    logger->warn("reason: The configuration of the current BVH is diffrent from the fist file.");
                    return std::nullopt;
                }

                auto positions_rotations = extract_frames(anima.value());

                return positions_rotations;
            });
        });

    File file(output.string(), File::ReadWrite | File::Create | File::Truncate);

    std::size_t feature_dim = 3 + 6;  // pos + 6D rotation

    DataSpace          dataspace({1000, joints_parent.size(), feature_dim}, {DataSpace::UNLIMITED, joints_parent.size(), feature_dim});
    DataSetCreateProps props;
    props.add(Chunking({300, joints_parent.size(), feature_dim}));

    file.createAttribute("joints_parent", joints_parent);
    file.createAttribute("joints_name", joints_name);
    file.createAttribute("frame_rate", frame_rate);
    file.createAttribute("bones_length", bones_length);

    DataSet dataset = file.createDataSet<float>("/motion", dataspace, props);

    std::size_t                   frames = 0;
    std::pmr::vector<std::size_t> split_index;
    for (auto& job : jobs) {
        job.wait();
        auto data = job.get();
        if (data.has_value()) {
            std::size_t new_frames = data.value().size() / (joints_parent.size() * feature_dim);
            dataset.resize({frames + new_frames, joints_parent.size(), feature_dim});
            dataset.select({frames, 0, 0}, {new_frames, joints_parent.size(), feature_dim}).write_raw(data.value().data());
            frames += new_frames;
            split_index.emplace_back(frames);
        }
        logger->info("writed {}", frames);
    }
    file.createAttribute("split_index", split_index);
    logger->info("writed total {} frames", frames);

    clean_exit();
}

std::pmr::vector<fs::path> get_all_bvh(const fs::path& dir, const std::regex& filter) {
    std::pmr::vector<fs::path> result;
    fs::directory_iterator     dir_iter{dir};
    std::copy_if(
        fs::begin(dir_iter), fs::end(dir_iter),
        std::back_inserter(result),
        [filter](const fs::directory_entry& entry) {
            return entry.is_regular_file() && std::regex_match(entry.path().filename().string(), filter);
        });

    return result;
}

std::pmr::vector<int> extract_parents(const Animation& anima) {
    std::pmr::vector<int> result;
    // Bone
    for (std::size_t i = 0; i < anima.joints.size(); i++) {
        auto        joint        = anima.joints[i];
        auto        iter         = std::find(std::begin(anima.joints), std::end(anima.joints), joint->parent.lock());
        std::size_t parent_index = std::distance(std::begin(anima.joints), iter);
        result.emplace_back(parent_index == anima.joints.size() ? -1 : parent_index);
    }
    return result;
};

std::pmr::vector<float> extract_frames(const Animation& anima) {
    auto update_node = [](const std::shared_ptr<BoneNode>& node, const TRS& trs) {
        auto parent       = node->parent.lock();
        auto parent_space = parent ? parent->transform : mat4f::identity();
        node->transform   = parent_space * translate(trs.translation) * rotate(trs.rotation);
    };

    std::pmr::vector<float> result;
    result.reserve(anima.frames.size() * anima.joints.size() * (3 + 4));

    for (std::size_t frame_index = 0; frame_index < anima.frames.size(); frame_index++) {
        const auto& frame       = anima.frames[frame_index];
        size_t      joint_index = 0;
        for (const auto& joint : anima.joints) {
            update_node(joint, joint == anima.joints.front() ? TRS{} : frame[joint_index]);
            joint_index++;
        }

        for (const auto& joint : anima.joints) {
            vec3f translation;
            vec3f up, forward;
            if (joint == anima.joints.front()) {
                translation          = frame[0].translation;
                auto rotation_matrix = rotate(frame[0].rotation);
                up                   = get_up(rotation_matrix);
                forward              = get_forward(rotation_matrix);
            } else {
                translation = get_translation(joint->transform);
                up          = get_up(joint->transform);
                forward     = get_forward(joint->transform);
            }

            result.emplace_back(translation.x);
            result.emplace_back(translation.y);
            result.emplace_back(translation.z);
            result.emplace_back(up.x);
            result.emplace_back(up.y);
            result.emplace_back(up.z);
            result.emplace_back(forward.x);
            result.emplace_back(forward.y);
            result.emplace_back(forward.z);
        }
    }

    return result;
}

std::pmr::vector<std::pmr::string> extract_joints_name(const Animation& anima) {
    std::pmr::vector<std::pmr::string> result;
    std::transform(
        anima.joints.begin(), anima.joints.end(),
        std::back_inserter(result),
        [](const std::shared_ptr<BoneNode>& node) {
            return node->name;
        });
    return result;
}

std::pmr::vector<float> extract_bones_length(const Animation& anima) {
    std::pmr::vector<float> result;
    std::transform(
        anima.joints.begin(), anima.joints.end(),
        std::back_insert_iterator(result),
        [](const std::shared_ptr<BoneNode>& node) {
            return node->offset.norm();
        });

    return result;
}