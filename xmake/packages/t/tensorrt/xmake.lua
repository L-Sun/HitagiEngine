package("tensorrt")
    set_homepage("https://developer.nvidia.com/tensorrt")
    set_description("NVIDIA TensorRT C++ inference SDK")
    set_license("NVIDIA Software License")

    if is_plat("windows") and is_arch("x64") then
        add_urls("https://developer.download.nvidia.com/compute/machine-learning/tensorrt/11.4.0/zip/TensorRT-Enterprise-11.4.0.106-Windows-amd64-cuda-12.9-Release-external.zip")
        add_versions("11.4.0", "82d29828ff7611b90f524cc13d45e56c752a6f4a3d223b07275012d0a2047bdc")
    end

    add_configs("shared", {description = "TensorRT uses shared libraries.", default = true, type = "boolean", readonly = true})
    add_configs("system_cuda", {description = "Use the system CUDA SDK instead of installing CUDA with XMake.", default = false, type = "boolean"})
    set_policy("package.precompiled", false)

    on_load(function (package)
        package:add("deps", "cuda", {system = package:config("system_cuda"), configs = {shared = true}})
        if package:config("system_cuda") then
            import("detect.sdks.find_cuda")
            local cuda = assert(find_cuda(), "System CUDA SDK not found; install CUDA or configure --cuda=<SDK path>.")
            assert(cuda.version and cuda.version:match("^12%."), "This TensorRT binary requires CUDA 12.x; configure --cuda=<CUDA 12 SDK path>.")
            package:data_set("cuda_sdk", cuda)
            package:addenv("PATH", cuda.bindir)
        end
        package:add("links", "nvonnxparser_11", "nvinfer_plugin_11", "nvinfer_11")
    end)

    on_fetch(function (package, opt)
        if opt.system or not os.isfile(package:installdir("include", "NvInfer.h")) then
            return
        end
        -- The upstream CUDA package is a toolchain; expose its C++ runtime
        -- explicitly for ordinary C++ targets without enabling nvcc.
        local cuda = package:dep("cuda")
        local sdk = package:data("cuda_sdk")
        return {
            version = package:version_str(),
            includedirs = table.join(package:installdir("include"), sdk and sdk.includedirs or cuda:installdir("include")),
            linkdirs = table.join(package:installdir("lib"), sdk and sdk.linkdirs or cuda:installdir("lib", "x64")),
            links = {"nvonnxparser_11", "nvinfer_plugin_11", "nvinfer_11", "cudart"}
        }
    end)

    on_install("windows|x64", function (package)
        os.cp("include", package:installdir())
        os.cp("lib/*.lib", package:installdir("lib"))
        os.cp("bin/*", package:installdir("bin"))
        os.cp("doc", package:installdir())
        package:addenv("PATH", "bin")
    end)

    on_test(function (package)
        assert(package:check_cxxsnippets({test = [[
            #include <NvInfer.h>
            #include <NvInferPlugin.h>
            #include <NvOnnxParser.h>
            static_assert(NV_TENSORRT_MAJOR == 11 && NV_TENSORRT_MINOR == 4 &&
                          NV_TENSORRT_PATCH == 0 && NV_TENSORRT_BUILD == 106);
            class Logger final : public nvinfer1::ILogger {
            public:
                void log(Severity, const char*) noexcept override {}
            };
            void test() {
                Logger logger;
                auto* builder = nvinfer1::createInferBuilder(logger);
                auto* network = builder->createNetworkV2(0);
                auto* parser = nvonnxparser::createParser(*network, logger);
                initLibNvInferPlugins(&logger, "");
                delete parser;
                delete network;
                delete builder;
            }
        ]]}, {configs = {languages = "c++17"}}))
    end)
