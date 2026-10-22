@echo off
setlocal enabledelayedexpansion

rem =============================================================
rem  release.cmd
rem  Tags the latest commit on main with a version and pushes the
rem  tag, which triggers .github\workflows\release.yml to build the
rem  apps and publish a GitHub release.
rem
rem  main is protected: every change reaches it through a pull
rem  request, so this script never commits or pushes a branch.
rem  Merge your changes (including the new section in release.md)
rem  first, then run this.
rem
rem  Only a pushed version tag (vX.Y.Z) on main makes a release.
rem  Ordinary commits and merges to main never do.
rem =============================================================

rem --- Always work from the repo root (where this file lives) ---------
cd /d "%~dp0"

rem --- Make sure we're actually inside a git repo -----------------
git rev-parse --is-inside-work-tree >nul 2>&1
if errorlevel 1 (
    echo ERROR: This doesn't look like a git repository.
    pause
    exit /b 1
)

rem --- Releases are only made from main ------------------------------
for /f "delims=" %%b in ('git rev-parse --abbrev-ref HEAD') do set "BRANCH=%%b"
if /i not "%BRANCH%"=="main" (
    echo ERROR: You are on branch "%BRANCH%". Releases are only made from main.
    echo        Run: git checkout main
    pause
    exit /b 1
)

rem --- Nothing may be uncommitted: the tag marks exactly what is on GitHub ---
set "DIRTY="
for /f "delims=" %%s in ('git status --porcelain') do set "DIRTY=1"
if defined DIRTY (
    echo ERROR: You have uncommitted changes. They would NOT be in the release.
    echo        Put them in a pull request and merge it first, then run: git pull
    pause
    exit /b 1
)

rem --- main must match GitHub exactly -----------------------------------
git fetch origin >nul 2>&1
if errorlevel 1 (
    echo ERROR: Could not reach GitHub to check main.
    pause
    exit /b 1
)
set "BEHIND=0"
set "AHEAD=0"
for /f "delims=" %%n in ('git rev-list --count HEAD..origin/main') do set "BEHIND=%%n"
for /f "delims=" %%n in ('git rev-list --count origin/main..HEAD') do set "AHEAD=%%n"
if not "%BEHIND%"=="0" (
    echo ERROR: Your main is %BEHIND% commit^(s^) behind GitHub. Run: git pull
    pause
    exit /b 1
)
if not "%AHEAD%"=="0" (
    echo ERROR: Your main has %AHEAD% commit^(s^) that are not on GitHub. Direct pushes to main
    echo        are blocked, so they can never be released. Move them to a branch and open a pull
    echo        request, then run: git reset --hard origin/main
    pause
    exit /b 1
)

rem --- Prompt for version number ------------------------------------
set "VERSION="
set /p VERSION="Version number (e.g. 0.1.0 or v0.1.0): "
if "%VERSION%"=="" (
    echo ERROR: Version number cannot be empty.
    pause
    exit /b 1
)

rem --- Normalize to a "vX.Y.Z" tag (add the "v" if the user left it off) ---
set "TAG=%VERSION%"
if /i not "%TAG:~0,1%"=="v" set "TAG=v%TAG%"
set "VER=%TAG:~1%"

rem --- The release workflow only runs for tags like v1.2.3 ----------------
echo %TAG%| findstr /r /c:"^v[0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*$" >nul
if errorlevel 1 (
    echo ERROR: "%TAG%" is not a version like v1.2.3. The release workflow would ignore it.
    pause
    exit /b 1
)

rem --- Bail out early if this tag already exists (here or on GitHub) --------
git rev-parse "%TAG%" >nul 2>&1
if not errorlevel 1 (
    echo ERROR: Tag %TAG% already exists. Choose a different version.
    pause
    exit /b 1
)
git ls-remote --exit-code --tags origin "refs/tags/%TAG%" >nul 2>&1
if not errorlevel 1 (
    echo ERROR: Tag %TAG% already exists on GitHub. Choose a different version.
    pause
    exit /b 1
)

rem --- Warn if release.md has no notes for this version --------------------
findstr /b /c:"## %VER% " release.md >nul 2>&1
if errorlevel 1 (
    echo.
    echo WARNING: release.md has no "## %VER% - date" section, so the release
    echo          will use GitHub's generated notes instead of yours.
)

rem --- Warn if the firmware version differs from the release version ------
set "FWVER="
for /f "tokens=3 delims= " %%v in ('findstr /c:"#define FIRMWARE_VERSION" MIDI-Toolbox\SerialProtocol.h') do set "FWVER=%%~v"
if defined FWVER if not "%FWVER%"=="%VER%" (
    echo.
    echo WARNING: The firmware version in MIDI-Toolbox\SerialProtocol.h is %FWVER%,
    echo          but you are releasing %VER%. That is fine if the firmware did not change.
)

for /f "delims=" %%c in ('git log -1 --oneline') do set "HEADLINE=%%c"

echo.
echo ------------------------------------------------------------
echo  Branch:  %BRANCH%
echo  Commit:  %HEADLINE%
echo  Tag:     %TAG%
echo ------------------------------------------------------------
echo  This will tag the commit above and push the tag. Pushing the
echo  tag builds the apps and publishes a public GitHub release.
echo ------------------------------------------------------------
set /p CONFIRM="Proceed? (y/n): "
if /i not "%CONFIRM%"=="y" (
    echo Cancelled.
    pause
    exit /b 0
)

rem --- Tag it -----------------------------------------------------------
git tag -a "%TAG%" -m "MIDI Toolbox %TAG%"
if errorlevel 1 (
    echo ERROR: git tag failed.
    pause
    exit /b 1
)

rem --- Push the tag (this is what fires the release job) -----------------------
echo.
echo Pushing tag %TAG%...
git push origin "%TAG%"
if errorlevel 1 (
    echo ERROR: git push of tag failed. Remove the local tag with: git tag -d %TAG%
    pause
    exit /b 1
)

echo.
echo Done. Pushing tag %TAG% should trigger the build + release workflow.
echo Check the Actions tab on GitHub to watch it run:
echo   https://github.com/yorkshiretwist/MidiToolbox/actions
pause
