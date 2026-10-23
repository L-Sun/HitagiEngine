# Windows Debugging

## LLDB cannot find python311.dll

Apply this workaround only when the installed `lldb.exe` reports `unable to find 'python311.dll'`. It is specific to an LLDB build requiring Python 3.11; other builds may require a different runtime.

Provide Python 3.11 through `uv` and add its directory to the current PowerShell session's PATH:

```powershell
uv python install 3.11
$lldbPythonDir = Split-Path (uv python find 3.11)
$env:PATH = "$lldbPythonDir;$env:PATH"
```

Then run an already-built test binary under LLDB. This example assumes the release output path; adjust it to the active build configuration and replace `SuiteName` with the suite to debug:

```powershell
lldb --batch -o "run --gtest_filter=SuiteName.*" -o "bt" -- build\windows\x64\release\unit_tests.exe
```

Save debugger logs and crash artifacts under `temp/`.
