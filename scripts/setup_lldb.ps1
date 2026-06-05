#!/usr/bin/env pwsh
$ErrorActionPreference = "Stop"

$tag    = "llvmorg-22.1.6"
$root   = Split-Path -Parent $PSScriptRoot
$extern = Join-Path $root "extern\llvm-project"

if (-not (Test-Path "$extern\.git")) {
    git clone --depth 1 --branch $tag --filter=blob:none --sparse `
        https://github.com/llvm/llvm-project.git $extern
}
git -C $extern sparse-checkout set lldb/include lldb/scripts llvm/include/llvm/BinaryFormat

py "$extern\lldb\scripts\generate-sbapi-dwarf-enum.py" `
    "$extern\llvm\include\llvm\BinaryFormat\Dwarf.def" `
    --output "$extern\lldb\include\lldb\API\SBLanguages.h"

Write-Host "LLDB SB headers ready at $extern\lldb\include"
