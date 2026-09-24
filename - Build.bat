@echo off
setlocal EnableExtensions

if /i "%~1"=="--run-build" goto :run_build

set "RESULTS_LOG=%~dp0Results.log"
set "SEQWENCER_BUILD_SCRIPT=%~f0"

echo Starting Seqwencer build...
echo Progress and test output will appear below.
echo.

where powershell.exe >nul 2>nul
if errorlevel 1 (
    echo BUILD FAILED
    echo   - FAIL: Windows PowerShell was not found
    echo.
    pause
    exit /b 1
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -Command ^
  "$writer = [System.IO.StreamWriter]::new($env:RESULTS_LOG, $false, [System.Text.UTF8Encoding]::new($false));" ^
  "$exitCode = 1;" ^
  "try {" ^
  "  & $env:SEQWENCER_BUILD_SCRIPT --run-build 2>&1 | ForEach-Object {" ^
  "    $line = $_.ToString();" ^
  "    [Console]::WriteLine($line);" ^
  "    $writer.WriteLine($line);" ^
  "    $writer.Flush();" ^
  "  };" ^
  "  $exitCode = $LASTEXITCODE;" ^
  "} finally { $writer.Dispose(); };" ^
  "exit $exitCode"
set "BUILD_EXIT_CODE=%ERRORLEVEL%"

echo.
echo Results saved:
echo   %RESULTS_LOG%
echo.
pause
exit /b %BUILD_EXIT_CODE%

:run_build
title Seqwencer - v1.3.24.0 Build

set "PROJECT_ROOT=%~dp0"
set "SOURCE_DIR=%PROJECT_ROOT%source"
set "BUILD_DIR=%PROJECT_ROOT%build"
set "DIST_DIR=%PROJECT_ROOT%dist"
set "CMAKE_EXE=%PROJECT_ROOT%..\_Tools\cmake\_4.4.2\bin\cmake.exe"
set "JUCE_CMAKE=%PROJECT_ROOT%..\_Tools\JUCE\_8.0.15\CMakeLists.txt"
set "TEST_EXE=%BUILD_DIR%\Release\SeqwencerCoreTests.exe"
set "PROBE_TEST_EXE=%BUILD_DIR%\SeqwencerVST3ProbeTests_artefacts\Release\SeqwencerVST3ProbeTests.exe"
set "BUNDLE_BINARY=%BUILD_DIR%\Seqwencer_artefacts\Release\VST3\Seqwencer.vst3\Contents\x86_64-win\Seqwencer.vst3"
set "FINAL_PLUGIN=%DIST_DIR%\Seqwencer.vst3"

echo.
echo Seqwencer 64-bit VST3 - v1.3.24.0 Build
echo ========================================
echo.

echo Closing PolyHostInterface.exe if it is running...
taskkill.exe /F /IM "PolyHostInterface.exe" >nul 2>&1
if errorlevel 1 (
    echo       No running instance found
) else (
    echo       CLOSED
)
echo.

echo [1/7] Checking required tools and source files...
if not exist "%CMAKE_EXE%" goto :missing_cmake
if not exist "%JUCE_CMAKE%" goto :missing_juce
if not exist "%SOURCE_DIR%\CMakeLists.txt" goto :missing_source
if not exist "%SOURCE_DIR%\PluginProcessor.cpp" goto :missing_source
if not exist "%SOURCE_DIR%\PluginProcessor.h" goto :missing_source
if not exist "%SOURCE_DIR%\PluginEditor.cpp" goto :missing_source
if not exist "%SOURCE_DIR%\PluginEditor.h" goto :missing_source
if not exist "%SOURCE_DIR%\SeqwencerBridgeProtocol.h" goto :missing_source
if not exist "%SOURCE_DIR%\SequencerCore.h" goto :missing_source
if not exist "%SOURCE_DIR%\tests\SequencerCoreTests.cpp" goto :missing_source
if not exist "%SOURCE_DIR%\tests\VST3ProbeTests.cpp" goto :missing_source
where powershell.exe >nul 2>nul
if errorlevel 1 goto :missing_powershell
echo       PASS
echo.

echo [2/7] Configuring Visual Studio Community 2026 x64...
"%CMAKE_EXE%" -S "%SOURCE_DIR%" -B "%BUILD_DIR%" -G "Visual Studio 18 2026" -A x64
if errorlevel 1 goto :configure_failed
echo       PASS
echo.

echo [3/7] Building and running sequencer core tests...
"%CMAKE_EXE%" --build "%BUILD_DIR%" --config Release --target SeqwencerCoreTests --clean-first --parallel
if errorlevel 1 goto :test_build_failed
if not exist "%TEST_EXE%" goto :test_not_found
"%TEST_EXE%"
if errorlevel 1 goto :tests_failed
echo       PASS
echo.

echo [4/7] Building Release VST3...
"%CMAKE_EXE%" --build "%BUILD_DIR%" --config Release --target Seqwencer_VST3 --parallel
if errorlevel 1 goto :build_failed
if not exist "%BUNDLE_BINARY%" goto :binary_not_found
echo       PASS
echo.

echo [5/7] Creating the portable single-file output...
if not exist "%DIST_DIR%" mkdir "%DIST_DIR%"
if errorlevel 1 goto :dist_failed
if not exist "%DIST_DIR%\Data\Presets" mkdir "%DIST_DIR%\Data\Presets"
if errorlevel 1 goto :dist_failed
if exist "%FINAL_PLUGIN%\" rmdir /s /q "%FINAL_PLUGIN%"
if exist "%FINAL_PLUGIN%" del /f /q "%FINAL_PLUGIN%"
copy /y "%BUNDLE_BINARY%" "%FINAL_PLUGIN%" >nul
if errorlevel 1 goto :copy_failed
if not exist "%FINAL_PLUGIN%" goto :copy_failed
echo       PASS
echo.

echo [6/7] Building and running the independent VST3 editor probe...
"%CMAKE_EXE%" --build "%BUILD_DIR%" --config Release --target SeqwencerVST3ProbeTests --parallel
if errorlevel 1 goto :probe_test_build_failed
if not exist "%PROBE_TEST_EXE%" goto :probe_test_not_found
"%PROBE_TEST_EXE%" "%FINAL_PLUGIN%"
set "PROBE_EXIT_CODE=%ERRORLEVEL%"
if not "%PROBE_EXIT_CODE%"=="0" goto :probe_tests_failed
echo       PASS
echo.

echo [7/7] Validating x64 format and dist contents...
powershell.exe -NoProfile -Command ^
  "$path = [IO.Path]::GetFullPath('%FINAL_PLUGIN%');" ^
  "$bytes = [IO.File]::ReadAllBytes($path);" ^
  "if ($bytes.Length -lt 64) { exit 1 };" ^
  "$pe = [BitConverter]::ToInt32($bytes, 60);" ^
  "if ($pe -lt 0 -or ($pe + 6) -gt $bytes.Length) { exit 1 };" ^
  "if ([BitConverter]::ToUInt32($bytes, $pe) -ne 0x00004550) { exit 1 };" ^
  "if ([BitConverter]::ToUInt16($bytes, $pe + 4) -ne 0x8664) { exit 1 };"
if errorlevel 1 goto :wrong_architecture

powershell.exe -NoProfile -Command ^
  "$extra = Get-ChildItem -LiteralPath '%DIST_DIR%' -Force | Where-Object {" ^
  "  $_.Name -ne 'Seqwencer.vst3' -and -not ($_.Name -eq 'Data' -and $_.PSIsContainer)" ^
  "};" ^
  "if ($extra) { $extra | ForEach-Object { Write-Host ('Unexpected: ' + $_.FullName) }; exit 1 }"
if errorlevel 1 goto :unexpected_dist_files
echo       PASS
echo.

echo BUILD SUMMARY
echo   - PASS: Required tools and source files found
echo   - PASS: Visual Studio Community 2026 x64 configured
echo   - PASS: Sequencer timing, pattern tools, all FX, routing and HOST SYNC tests passed
echo   - PASS: Release VST3 compiled
echo   - PASS: Single-file VST3 created
echo   - PASS: Independent VST3 editor probe passed
echo   - PASS: Output verified as Windows x64
echo.
echo Output:
echo   %FINAL_PLUGIN%
echo.
echo No plug-in files were installed elsewhere.
echo.
exit /b 0

:missing_cmake
echo.
echo BUILD FAILED
echo   - FAIL: CMake 4.4.2 was not found
echo Expected:
echo   %CMAKE_EXE%
goto :failed

:missing_juce
echo.
echo BUILD FAILED
echo   - FAIL: JUCE 8.0.15 was not found
echo Expected:
echo   %JUCE_CMAKE%
goto :failed

:missing_source
echo.
echo BUILD FAILED
echo   - FAIL: A required source file was not found
echo Expected source folder:
echo   %SOURCE_DIR%
goto :failed

:missing_powershell
echo.
echo BUILD FAILED
echo   - FAIL: Windows PowerShell was not found
goto :failed

:configure_failed
echo.
echo BUILD FAILED
echo   - PASS: Required tools and source files found
echo   - FAIL: CMake could not configure Visual Studio Community 2026 x64
goto :failed

:test_build_failed
echo.
echo BUILD FAILED
echo   - PASS: CMake configuration completed
echo   - FAIL: Sequencer core tests could not compile
goto :failed

:test_not_found
echo.
echo BUILD FAILED
echo   - FAIL: The compiled sequencer test program was not found
echo Expected:
echo   %TEST_EXE%
goto :failed

:tests_failed
echo.
echo BUILD FAILED
echo   - FAIL: Sequencer timing, gate or target-routing behaviour failed its tests
goto :failed

:probe_test_build_failed
echo.
echo BUILD FAILED
echo   - PASS: Sequencer timing, gate and target-routing tests passed
echo   - PASS: Release VST3 compiled
echo   - PASS: Single-file VST3 created
echo   - FAIL: Independent VST3 editor probe could not compile
goto :failed

:probe_test_not_found
echo.
echo BUILD FAILED
echo   - FAIL: The compiled VST3 editor probe was not found
echo Expected:
echo   %PROBE_TEST_EXE%
goto :failed

:probe_tests_failed
echo.
echo BUILD FAILED
echo   - FAIL: Seqwencer failed the independent VST3 bridge/editor probe
echo   - Probe exit code: %PROBE_EXIT_CODE%
goto :failed

:build_failed
echo.
echo BUILD FAILED
echo   - PASS: Sequencer core tests passed
echo   - FAIL: Release VST3 compilation failed
goto :failed

:binary_not_found
echo.
echo BUILD FAILED
echo   - PASS: Compilation command completed
echo   - FAIL: JUCE's internal x64 VST3 binary was not found
echo Expected:
echo   %BUNDLE_BINARY%
goto :failed

:dist_failed
echo.
echo BUILD FAILED
echo   - FAIL: The dist folder could not be created
goto :failed

:copy_failed
echo.
echo BUILD FAILED
echo   - FAIL: Seqwencer.vst3 could not be copied into dist
goto :failed

:wrong_architecture
echo.
echo BUILD FAILED
echo   - FAIL: dist\Seqwencer.vst3 is not a Windows x64 PE binary
goto :failed

:unexpected_dist_files
echo.
echo BUILD FAILED
echo   - FAIL: dist contains an unexpected file or folder
echo Allowed:
echo   Seqwencer.vst3
echo   Data  ^(optional existing runtime folder^)
goto :failed

:failed
echo.
echo Please send Results.log.
echo.
exit /b 1
