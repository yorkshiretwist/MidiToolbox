@echo off
setlocal enabledelayedexpansion

rem =============================================================
rem  release.cmd
rem  Commits current changes, tags the commit, and pushes both
rem  to trigger the .github\workflows\release.yml build + release.
rem
rem  Only a pushed version tag (vX.Y.Z) on main makes a release.
rem  Ordinary commits and pushes to main never do.
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

rem --- Make sure main is not behind GitHub ----------------------------
git fetch origin >nul 2>&1
set "BEHIND=0"
for /f "delims=" %%n in ('git rev-list --count HEAD..origin/main') do set "BEHIND=%%n"
if not "%BEHIND%"=="0" (
    echo ERROR: Your main is %BEHIND% commit^(s^) behind GitHub. Run: git pull
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

rem --- Bail out early if this tag already exists ---------------------
git rev-parse "%TAG%" >nul 2>&1
if not errorlevel 1 (
    echo ERROR: Tag %TAG% already exists. Choose a different version.
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

rem --- Prompt for commit message -----------------------------------
echo.
set "COMMIT_MSG="
set /p COMMIT_MSG="Commit message (used for the tag too): "
if "%COMMIT_MSG%"=="" (
    echo ERROR: Commit message cannot be empty.
    pause
    exit /b 1
)

echo.
echo ------------------------------------------------------------
echo  Branch:  %BRANCH%
echo  Commit:  %COMMIT_MSG%
echo  Tag:     %TAG%
echo ------------------------------------------------------------
echo  This will stage ALL changes, commit, tag, and push. Pushing the
echo  tag builds the apps and publishes a public GitHub release.
echo ------------------------------------------------------------
set /p CONFIRM="Proceed? (y/n): "
if /i not "%CONFIRM%"=="y" (
    echo Cancelled.
    pause
    exit /b 0
)

rem --- Stage everything and commit ------------------------------------
git add -A

git diff --cached --quiet
if errorlevel 1 (
    git commit -m "%COMMIT_MSG%"
    if errorlevel 1 (
        echo ERROR: git commit failed.
        pause
        exit /b 1
    )
) else (
    echo No staged changes to commit - tagging the current HEAD as-is.
)

rem --- Tag it -----------------------------------------------------------
git tag -a "%TAG%" -m "%COMMIT_MSG%"
if errorlevel 1 (
    echo ERROR: git tag failed.
    pause
    exit /b 1
)

rem --- Push the branch, then the tag (tag push is what fires the release job) ---
echo.
echo Pushing branch %BRANCH%...
git push origin "%BRANCH%"
if errorlevel 1 (
    echo ERROR: git push of branch failed. The tag %TAG% exists locally but was not pushed.
    echo        Fix the problem, then run: git push origin %TAG%
    pause
    exit /b 1
)

echo Pushing tag %TAG%...
git push origin "%TAG%"
if errorlevel 1 (
    echo ERROR: git push of tag failed.
    pause
    exit /b 1
)

echo.
echo Done. Pushing tag %TAG% should trigger the build + release workflow.
echo Check the Actions tab on GitHub to watch it run:
echo   https://github.com/yorkshiretwist/MidiToolbox/actions
pause
