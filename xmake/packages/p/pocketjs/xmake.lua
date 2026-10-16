package("pocketjs")
    set_homepage("https://github.com/pocket-nexus/pocketjs")
    set_description("PocketJS native UI runtime, TSX compiler and application build rule")
    set_license("MIT")
    add_urls("https://github.com/pocket-nexus/pocketjs/archive/55e39a06868c9776f0320909ba37b187f0606d0f.tar.gz")
    add_versions("0.14.0", "3e64f85c5a55e651dcf5f9831aa38c69741d1834b5c4c427760aa9e46a6a89fb")
    add_configs("shared", {description = "Build shared libraries", default = false, type = "boolean", readonly = true})
    add_links("pocket_apple")
    add_syslinks("advapi32", "bcrypt", "ntdll", "userenv", "ws2_32")

    on_install("windows|x64", function (package)
        import("lib.detect.find_tool")
        local cargo = assert(find_tool("cargo"), "PocketJS requires Cargo/Rust on PATH")
        local bun = assert(find_tool("bun"), "PocketJS UI compilation requires Bun on PATH")
        os.cp(path.join(package:scriptdir(), "hitagi_desktop.rs"), "engine/ios/src/hitagi_desktop.rs")
        os.cp(path.join(package:scriptdir(), "hitagi_gpu.rs"), "engine/ios/src/hitagi_gpu.rs")
        io.replace("engine/ios/src/lib.rs", "pub mod core_host;", "pub mod core_host;\nmod hitagi_desktop;\nmod hitagi_gpu;", {plain = true})
        local profile = package:is_debug() and "dev" or "release"
        os.vrunv(cargo.program, {"build", "--manifest-path", "engine/Cargo.toml",
            "-p", "pocket-apple", "--profile", profile, "--locked", "-j", "1",
            "--target", "x86_64-pc-windows-msvc"})
        local output = package:is_debug() and "debug" or "release"
        os.cp(path.join("engine/target/x86_64-pc-windows-msvc", output, "pocket_apple.lib"), package:installdir("lib"))
        os.cp("engine/ios/include/*.h", package:installdir("include"))
        os.cp("contracts/generated/*.h", package:installdir("include"))
        os.cp(path.join(package:scriptdir(), "pocket_hitagi.h"), package:installdir("include"))

        -- Install the compiler alongside the runtime, but keep application builds
        -- and their mutable caches in the consuming project's @pocketjs/ui rule.
        io.replace("tools/build.ts",
            'const generatedPath = join(ROOT, "framework/src/styles.generated.ts");',
            'const generatedPath = join(process.env.POCKETJS_WORK_DIR!, "styles.generated.ts");', {plain = true})
        io.replace("framework/compiler/jsx-plugin.ts",
            'const CACHE_DIR = fileURLToPath(new URL("../../.cache/transforms/", import.meta.url));',
            'const CACHE_DIR = process.env.POCKETJS_WORK_DIR! + "/transforms/";', {plain = true})
        -- Bun's Windows resolver caches missing imports during the first pass.
        io.writefile("framework/src/styles.generated.ts", "export const STYLE_IDS: Record<string, number> = {};\n")
        local sdk = package:installdir("share/pocketjs")
        os.mkdir(sdk)
        for _, entry in ipairs({"framework", "contracts", "tools", "microts/compiler", "assets/fonts",
                                "assets/images", "package.json", "bun.lock", "tsconfig.json", "LICENSE"}) do
            os.cp(entry, path.join(sdk, entry))
        end
        os.vrunv(bun.program, {"install", "--frozen-lockfile"}, {curdir = sdk})
    end)

    on_test(function (package)
        assert(package:check_cxxsnippets({test = [[
            void test() {
                auto runtime = pocket_hitagi_create_gpu(1, 960, 540);
                pocket_hitagi_resize(runtime, 1200, 800);
                PocketHitagiDrawList draw{};
                pocket_hitagi_draw(runtime, &draw);
                pocket_apple_destroy(runtime);
            }
        ]]}, {includes = "pocket_hitagi.h"}))
        assert(os.isfile(package:installdir("share/pocketjs/tools/build.ts")))
        assert(os.isfile(package:installdir("share/pocketjs/node_modules/solid-js/package.json")))
    end)
