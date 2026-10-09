-- Run from the repository root: xmake lua xmake/scripts/check_interop.lua
-- This is a source-boundary audit, not a C++ syntax or platform check.
function main()
    local standard = {}
    for name in ([=[
        algorithm any array atomic barrier bit bitset cassert cctype cerrno cfenv
        cfloat charconv chrono cinttypes ciso646 climits clocale cmath codecvt
        compare complex concepts condition_variable coroutine csetjmp csignal
        cstdalign cstdarg cstdbool cstddef cstdint cstdio cstdlib cstring ctime
        cuchar cwchar cwctype deque exception execution expected filesystem format
        forward_list fstream functional future generator initializer_list iomanip
        ios iosfwd iostream istream iterator latch limits list locale map mdspan
        memory memory_resource mutex new numbers numeric optional ostream print
        queue random ranges ratio regex scoped_allocator semaphore set shared_mutex
        source_location span spanstream sstream stack stacktrace stdexcept
        stdfloat stop_token streambuf string string_view strstream syncstream
        system_error thread tuple type_traits typeindex typeinfo unordered_map
        unordered_set utility valarray variant vector version
        assert.h ctype.h errno.h fenv.h float.h inttypes.h limits.h locale.h math.h
        setjmp.h signal.h stdarg.h stdbool.h stddef.h stdint.h stdio.h stdlib.h
        string.h time.h uchar.h wchar.h wctype.h
    ]=]):gmatch("%S+") do
        standard[name] = true
    end
    local local_headers = {
        ["test_macros.hpp"] = true,
        ["ispc_math.hpp"] = true,
        ["vector.ispc.h"] = true, -- generated ISPC ABI, not a third-party API
        ["bindless.hlsl"] = true -- embedded shader source, not a C++ include
    }
    local checked, bridges, failures = 0, 0, {}
    for _, root in ipairs({"hitagi", "examples"}) do
        for _, extension in ipairs({"cpp", "cppm", "hpp", "h", "cc", "c", "inl"}) do
            for _, file in ipairs(os.files(root .. "/**." .. extension)) do
                local normalized = file:gsub("\\", "/")
                local thin_header = normalized:find("^hitagi/interop/") and normalized:match("_macros%.hpp$")
                local boundary = normalized:find("^hitagi/interop/") and not normalized:find("^hitagi/interop/test/") and not thin_header
                if boundary then
                    bridges = bridges + 1
                else
                    checked = checked + 1
                    local line_number = 0
                    for line in (io.readfile(file) .. "\n"):gmatch("(.-)\n") do
                        line_number = line_number + 1
                        local header = line:match('^%s*#%s*include%s*[<"]([^>"]+)[>"]')
                        if header and not standard[header] and not local_headers[header]
                            and not header:match("^interop/[%w_]+_macros%.hpp$")
                            and not (thin_header and header:match("^[%w_]+_macros%.hpp$")) then
                            table.insert(failures, string.format("%s:%d: %s", file, line_number, header))
                        end
                    end
                end
            end
        end
    end
    table.sort(failures)
    for _, failure in ipairs(failures) do print(failure) end
    print("interop audit: %d consumer/thin-adapter files, %d boundary files, %d violations", checked, bridges, #failures)
    assert(#failures == 0, "Move external headers to their interop module")
end
