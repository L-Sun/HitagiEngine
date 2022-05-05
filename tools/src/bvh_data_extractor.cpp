#include "simple_bvh_parser.hpp"
#include <filesystem>
#include <hitagi/core/memory_manager.hpp>
#include <hitagi/core/file_io_manager.hpp>
#include <hitagi/core/thread_manager.hpp>

#include <fmt/core.h>
#include <cxxopts.hpp>

#include <fstream>
#include <algorithm>
#include <iterator>
#include <memory_resource>
#include <ostream>
#include <regex>
#include <sstream>
#include <vector>

using namespace hitagi;
using namespace hitagi::core;
using namespace hitagi::math;

namespace fs = std::filesystem;

int init() {
    int result;
    if ((result = g_MemoryManager->Initialize()) != 0) return result;
    if ((result = g_FileIoManager->Initialize()) != 0) return result;
    if ((result = g_ThreadManager->Initialize()) != 0) return result;
    return result;
}

void clean() {
    g_ThreadManager->Finalize();
    g_FileIoManager->Finalize();
    g_MemoryManager->Finalize();
}

void update_node(const std::shared_ptr<BoneNode>& node, const TRS& trs) {
    auto parent       = node->parent.lock();
    auto parent_space = parent ? parent->transform : mat4f(1.0f);
    node->transform   = parent_space * translate(rotate(mat4f(1.0f), trs.rotation), trs.translation);
};

std::stringstream frame_extractor(Animation& anima) {
    std::stringstream ss;

    for (const auto& frame : anima.frames) {
        size_t joint_index = 0;
        for (const auto& joint : anima.joints) {
            update_node(joint, joint == anima.joints.front() ? TRS{} : frame[joint_index]);
            joint_index++;
        }
        for (const auto& joint : anima.joints) {
            auto [translation, _r, _s] = decompose(joint->transform);
            auto rotation              = euler_to_quaternion(_r);
            ss << translation.x << ","
               << translation.y << ","
               << translation.z << ","
               << rotation.x << ","
               << rotation.y << ","
               << rotation.z << ","
               << rotation.w;
            if (joint != anima.joints.back()) {
                ss << ",";
            }
        }
        ss << std::endl;
    }
    return ss;
}

std::stringstream bone_extractor(Animation& anima) {
    std::stringstream ss;
    // Bone
    for (size_t i = 0; i < anima.joints.size(); i++) {
        auto   joint        = anima.joints[i];
        auto   iter         = std::find(std::begin(anima.joints), std::end(anima.joints), joint->parent.lock());
        size_t parent_index = std::distance(std::begin(anima.joints), iter);
        ss << i << "," << parent_index << std::endl;
    }
    return ss;
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

    cxxopts::Options options("Bvh Data Extractor");
    // clang-format off
    options.add_options()
        ("d,dir",
            "The BVH files directory path",
            cxxopts::value<std::string>()
        )
        (
            "filter",
            "Use a regexpr to get bvh file from directory",
            cxxopts::value<std::string>()->default_value("*.bvh"),
            "Example: -f 01_*.bvh\n"
        )
        ("s,space", 
            "The space for joint data is located in",
            cxxopts::value<std::string>()->default_value("root"),
            "accept value: root(default), world, relative"
        )
        ("m,merge", 
            "Merge all data to one file",
            cxxopts::value<bool>()->default_value("false"))
        ("o,output",
            "The path of output file",
            cxxopts::value<std::string>()
        )
    ;
    // clang-format on
    auto args = options.parse(argc, argv);

    auto collect_files = [&]() -> std::vector<fs::path> {
        try {
            const fs::path   bvh_dir{args["dir"].as<std::string>()};
            const std::regex reg{args["filter"].as<std::string>()};

            if (!fs::exists(bvh_dir) || !fs::is_directory(bvh_dir)) {
                throw cxxopts::option_syntax_exception("the dir argument is not a directory");
            }

            std::vector<fs::path>  paths;
            fs::directory_iterator dir_iter{bvh_dir};
            std::copy_if(
                fs::begin(dir_iter), fs::end(dir_iter),
                std::back_inserter(paths),
                [reg](const fs::directory_entry& entry) {
                    return entry.is_regular_file() && std::regex_match(entry.path().filename().string(), reg);
                });

            return paths;

        } catch (const fs::filesystem_error& err) {
            fmt::print("{}\n", err.what());
        } catch (const cxxopts::OptionException& ex) {
            fmt::print("{}\n", ex.what());
        } catch (const std::exception& ex) {
            fmt::print("{}\n", ex.what());
        }
        exit(1);
    };

    auto all_bvh_files = collect_files();

    std::vector<Animation> animations;
    for (auto&& path : all_bvh_files) {
        auto anima = parse_bvh(g_FileIoManager->SyncOpenAndReadBinary(path));
        if (anima.has_value())
            animations.emplace_back(std::move(anima.value()));
        else {
            fmt::print("error: failed to parse bvh file: {}\n", path.string());
            exit(1);
        }
    }

    std::vector<std::stringstream> frame_data(animations.size());
    std::transform(animations.begin(), animations.end(), frame_data.begin(), frame_extractor);

    std::vector<std::stringstream> bone_data(animations.size());
    std::transform(animations.begin(), animations.end(), bone_data.begin(), bone_extractor);

    bool                  merge = args["merge"].as<bool>();
    std::filesystem::path output;
    try {
        output = {args["output"].as<std::string>()};
        if (!fs::is_directory(output)) {
            throw cxxopts::option_syntax_exception(fmt::format("error: {} is not a directory", output.string()));
        }
    } catch (const cxxopts::OptionException& ex) {
        fmt::print("{}\n", ex.what());
        exit(1);
    }

    if (merge) {
        fmt::print("Warning: you are merging the data to one file with a same bone, please note that there may be some diffrent skeleton in your given bvh files\n");
        std::stringstream ss;
        for (size_t i = 0; i < all_bvh_files.size(); i++) {
            ss << frame_data.at(i).rdbuf();
        }
        ss << std::flush;
        save_file(output / all_bvh_files.front().stem() / "_skeleton.csv", bone_data.front());
        save_file(output / all_bvh_files.front().stem() / "_frames.csv", ss);
    } else {
        for (size_t i = 0; i < all_bvh_files.size(); i++) {
            save_file(output / all_bvh_files.at(i).stem() / "_skeleton.csv", bone_data.at(i));
            save_file(output / all_bvh_files.at(i).stem() / "_frames.csv", frame_data.at(i));
        }
    }

    clean();
    return 0;
}

int x(int argc, char** argv) {
    g_FileIoManager->Initialize();

    g_FileIoManager->Finalize();
    return 0;
}