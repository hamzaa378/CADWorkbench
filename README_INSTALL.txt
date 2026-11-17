CADWorkbench v1.70 - Windows MSB3075 Access-Denied Build Fix

Problem: MSVC compiled the application, but concurrent vcpkg z-applocal
post-build dependency deployment to build\Release failed with code 5.

Overlay on top of existing Fix 3 + Fix 4 + Fix 5 project (not into build\).
This changes ONLY scripts\build_windows.ps1 and the three checksum manifests.
It does not change CAD geometry, feature behavior, or native tests.

Apply in PowerShell from the CADWorkbench project root:
  Expand-Archive -LiteralPath "$env:USERPROFILE\Downloads\CADWorkbench_v170_MSBuild_AccessDenied_Serial_Fix.zip" -DestinationPath . -Force

Close any running CADWorkbench.exe and native test executables before rerunning.
Run the single failing regression first:
  powershell -NoProfile -ExecutionPolicy Bypass -File .\RUN_PREVIOUS_FAILURES.ps1 -VcpkgRoot C:\vcpkg -OnlyInterleavedHistory

Then run the whole suite:
  powershell -NoProfile -ExecutionPolicy Bypass -File .\RUN_WINDOWS_TESTS.ps1 -VcpkgRoot C:\vcpkg

The build now defaults to one job (-BuildJobs 1), preventing concurrent
vcpkg DLL copies to the same output directory. This fixes the known race,
but it cannot eliminate a lock held by antivirus or a running executable.
If Access is still denied: close application/test processes, restart the
terminal (or Windows), and try again. Do not remove source integrity checks.

This does not claim the native 130 CTests have passed; those remain to be run.
