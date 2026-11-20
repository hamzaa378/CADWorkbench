# Upload CADWorkbench Sprint 14 to GitHub

## Which files to upload

Upload the **contents of this `CADWorkbench` folder** as the root of **one repository**. Do not upload older Sprint 3–10 ZIPs into the repository; Sprint 14 is the consolidated full source, including prior work. The `src/`, `tests/`, `tools/`, `scripts/`, `docs/`, `.github/`, `qa/` and older engineering handover folders are preserved in the archive. Include `CMakeLists.txt`, `vcpkg.json`, the SHA-256 manifests, `.gitignore` and `.gitattributes`.

The archive intentionally excludes `build/`, compiled executable/DLLs, local `vcpkg_installed/`, virtual environments and personal audit logs. These are local machine outputs, not distributable source. **Keep the Windows-verified Sprint 11 baseline build and audit log outside GitHub** as rollback evidence until a fresh checkout is confirmed.

## Recommended: push with Git (fresh Windows PowerShell)

Extract `CADWorkbench_v170_Sprint14_CoordinatedCAD_Integration_GitHub_Source_PRELIMINARY_20261117.zip` in Downloads. In a fresh PowerShell session:

```powershell
Set-Location "$env:USERPROFILE\Downloads\CADWorkbench_Sprint14_GitHub\CADWorkbench"

# Make sure Git for Windows is installed and you have configured your identity.
git --version
git init -b main
git add .
git status --short
git commit -m "CADWorkbench v1.70 Sprint 14 preliminary integration source"

# FIRST create an empty GitHub repository through github.com/new.
# Choose no auto-generated README, .gitignore or license.
# Replace USER_OR_ORG and REPOSITORY_NAME with your actual values.
git remote add origin https://github.com/USER_OR_ORG/REPOSITORY_NAME.git
git push -u origin main
```

You may need to sign in via Git Credential Manager or a GitHub authentication flow. Do not paste access tokens into commands, source files or chat. For a private engineering repository, choose **Private** at creation.

If extracting into a different folder, update only the first `Set-Location` path. Do not run `git add` from Downloads itself.

For the GitHub website uploader, create an empty repository and upload the extracted **contents** of `CADWorkbench`, not the ZIP as a binary blob. Git CLI is safer for a project with hundreds of files and hidden `.github`/`.gitignore` entries.

## Verify a fresh checkout

```powershell
Set-Location "$env:USERPROFILE\Downloads"
git clone https://github.com/USER_OR_ORG/REPOSITORY_NAME.git CADWorkbench_Verify_Clone
Set-Location .\CADWorkbench_Verify_Clone
python .\tools\preflight_major_build.py --root .
python .\tools\audit_full_source.py --root .
python .\tools\verify_130_tests.py
```

The hash manifest is for **every tracked source and historical QA evidence file**. The `.gitignore` intentionally keeps historical `qa/*.log` test evidence tracked while ignoring new local build output logs. A successful fresh-clone preflight proves the repository is complete; it is not a new Windows native-build acceptance.

## When older folders can be removed

After GitHub shows the full source, the fresh clone passes source checks, and you have an offline backup of the ZIP and Windows evidence, you can remove old **extracted development Sprint folders** and redundant Sprint ZIPs. **Do not yet delete** the Windows-verified Sprint 11 `build/Release/` executable and its dependencies, the Sprint 11 full audit logs/reports, and at least one offline ZIP/source backup. These provide the known-good comparison if a later sprint regresses. Keep any project-specific work, datasets or manual acceptance results not in this source archive.

## Security and repository visibility

The packaged source has been scanned for obvious private key filenames and credential markers, but a static scan cannot guarantee absence of private information or license restrictions. Review documentation and history before publishing. GitHub Actions on Ubuntu run source and portable checks only. Engineering acceptance remains **0/60 recorded**, so do not label this a production-certified release.
