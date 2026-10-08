@echo off
REM Schedulix - QNX Pi 4 bring-up launcher
REM Double-click this. Plug Ethernet in first, unplug when it finishes.
powershell -ExecutionPolicy Bypass -NoProfile -File "%~dp0bringup.ps1"
echo.
echo ---------------------------------------------------------------------------
echo If the report says the audit did not run, the SSH login failed.
echo Try editing bringup.ps1 and changing $PiUser from "qnxuser" to "root".
echo The full log is in bringup_report.txt in the repository root.
echo ---------------------------------------------------------------------------
pause