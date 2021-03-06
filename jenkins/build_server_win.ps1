﻿<#
.SYNOPSIS
    A script for the Couchbase official build servers to use to build LiteCore for Windows Desktop and UWP
.DESCRIPTION
    This tool will build various flavors of LiteCore and package them according to the format the the Couchbase build server
    is used to dealing with.  It is the responsibility of the build job to then take the artifacts and put them somewhere.  It
    is meant for the official Couchbase build servers.  Do not try to use it, it will only confuse you.  You have been warned.
.PARAMETER Version
    The version number to give to the build (e.g. 2.0.0)
.PARAMETER ShaVersion
    The commit SHA that this build was built from
.PARAMETER Edition
    The edition to build (community vs enterprise)
#>
param(
    [Parameter(Mandatory=$true, HelpMessage="The version number to give to the build (e.g. 2.0.0)")][string]$Version,
    [Parameter(Mandatory=$true, HelpMessage="The commit SHA that this build was built from")][string]$ShaVersion,
    [Parameter(Mandatory=$true, HelpMessage="The edition to build (community vs enterprise)")][string]$Edition
)

$RelPkgDir = "MinSizeRel"
$DebugPkgDir = "Debug"
$VSVersion = "15 2017"
$WindowsMinimum = "10.0.16299.0"

function Make-Package() {
    param(
        [Parameter(Mandatory=$true, Position = 0)][string]$directory,
        [Parameter(Mandatory=$true, Position = 1)][string]$filename,
        [Parameter(Mandatory=$true, Position = 2)][string]$architecture,
        [Parameter(Mandatory=$true, Position = 3)][string]$config
    )

    Write-Host "Creating pkg - pkgdir:$directory, pkgname:$filename, arch:$architecture, flavor:$config"
    Push-Location $directory
    & 7za a -tzip -mx9 $env:WORKSPACE\$filename LiteCore.dll LiteCore.lib LiteCore.pdb
    if($LASTEXITCODE -ne 0) {
        throw "Zip failed"
    }

    $PropFile = "$env:WORKSPACE\publish_$arch.prop"
    New-Item -ItemType File -ErrorAction Ignore -Path $PropFile
    Add-Content $PropFile "PRODUCT=couchbase-lite-core"
    Add-Content $PropFile "VERSION=$ShaVersion"
    Add-Content $PropFile "${config}_PACKAGE_NAME_$architecture=$filename"
    Pop-Location
}

function Build-Store() {
    param(
        [Parameter(Mandatory=$true, Position = 0)][string]$directory,
        [Parameter(Mandatory=$true, Position = 1)][string]$architecture,
        [Parameter(Mandatory=$true, Position = 2)][string]$config
    )

    Write-Host "Building blddir:$directory, arch:$architecture, flavor:$config"
    New-Item -ItemType Directory -ErrorAction Ignore $directory
    Push-Location $directory
    $MsArchStore = ""
    if($architecture -ne "Win32") {
        $MsArchStore = " $architecture"
    }

    & "C:\Program Files\CMake\bin\cmake.exe" -G "Visual Studio $VSVersion$MsArchStore" -DCMAKE_SYSTEM_NAME=WindowsStore -DCMAKE_SYSTEM_VERSION="10.0" -DCMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION="$WindowsMinimum" -DEDITION="$Edition" ..
    if($LASTEXITCODE -ne 0) {
        throw "CMake failed"
    }

    & "C:\Program Files\CMake\bin\cmake.exe" --build . --config $config --target LiteCore
    if($LASTEXITCODE -ne 0) {
        throw "Build failed"
    }

    Pop-Location
}

function Build() {
    param(
        [Parameter(Mandatory=$true, Position = 0)][string]$directory,
        [Parameter(Mandatory=$true, Position = 1)][string]$architecture,
        [Parameter(Mandatory=$true, Position = 2)][string]$config
    )

    Write-Host "Building blddir:$directory, arch:$architecture, flavor:$config"
    New-Item -ItemType Directory -ErrorAction Ignore $directory
    Push-Location $directory
    $MsArch = ""
    if($architecture -ne "Win32") {
        $MsArch = " $architecture"
    }

    & "C:\Program Files\CMake\bin\cmake.exe" -G "Visual Studio $VSVersion$MsArch" -DEDITION="$Edition" ..
    if($LASTEXITCODE -ne 0) {
        throw "CMake failed"
    }

    & "C:\Program Files\CMake\bin\cmake.exe" --build . --config $config
    if($LASTEXITCODE -ne 0) {
        throw "Build failed ($LASTEXITCODE)"
    }

    Pop-Location
}

function Run-UnitTest() {
    param(
        [Parameter(Mandatory=$true, Position = 0)][string]$directory,
        [Parameter(Mandatory=$true, Position = 1)][string]$architecture
    )

    Write-Host "Testing testdir:$directory, arch:$architecture"
    New-Item -ItemType Directory -ErrorAction Ignore C:\tmp
    New-Item -ItemType Directory -ErrorAction Ignore $directory\C\tests\data
    Push-Location $directory\C\tests\data
    if(-Not (Test-Path $directory\C\tests\data\geoblocks.json)) {
        Copy-Item $env:WORKSPACE\couchbase-lite-core\C\tests\data\geoblocks.json $directory\C\tests\data\geoblocks.json
    }

    if(-Not (Test-Path $directory\C\tests\data\names_300000.json)) {
        Copy-Item $env:WORKSPACE\couchbase-lite-core\C\tests\data\names_300000.json $directory\C\tests\data\names_300000.json
    }

    Pop-Location
    Push-Location $directory\LiteCore\tests\MinSizeRel
    $env:LiteCoreTestsQuiet=1
    & .\CppTests -r list
    $env:LiteCoreTestsQuiet=0
    if($LASTEXITCODE -ne 0) {
        throw "CppTests failed"
    }

    Pop-Location
    Push-Location $directory\C\tests\MinSizeRel
    $env:LiteCoreTestsQuiet=1
    & .\C4Tests -r list
    $env:LiteCoreTestsQuiet=0
    if($LASTEXITCODE -ne 0) {
        throw "C4Tests failed"
    }

    Pop-Location
}

foreach ($arch in @("Win32", "Win64", "ARM")) {
    $Target = "${arch}_Debug"
    $arch_lower = $arch.ToLowerInvariant()
    Build-Store "${env:WORKSPACE}\build_cmake_store_${Target}" $arch "Debug"
    if($arch -ne "ARM") {
        Build "${env:WORKSPACE}\build_${Target}" $arch "Debug"
        Make-Package "${env:WORKSPACE}\build_${Target}\couchbase-lite-core\$DebugPkgDir" "couchbase-lite-core-$Version-$ShaVersion-windows-debug-$arch_lower.zip" "$arch" "DEBUG"
    }

    Make-Package "${env:WORKSPACE}\build_cmake_store_${Target}\couchbase-lite-core\$DebugPkgDir" "couchbase-lite-core-$Version-$ShaVersion-windows-debug-${arch_lower}-winstore.zip" "STORE_$arch" "DEBUG"

    $Target = "${arch}_MinSizeRel"
    Build-Store "${env:WORKSPACE}\build_cmake_store_${Target}" $arch "MinSizeRel"
    if($arch -ne "ARM") {
        Build "${env:WORKSPACE}\build_${Target}" $arch "MinSizeRel"
        if($Edition -eq "enterprise") {
            Run-UnitTest "${env:WORKSPACE}\build_${Target}\couchbase-lite-core" $arch
        }

        Make-Package "${env:WORKSPACE}\build_${Target}\couchbase-lite-core\$RelPkgDir" "couchbase-lite-core-$Version-$ShaVersion-windows-$arch_lower.zip" "$arch" "RELEASE"
    }

    Make-Package "${env:WORKSPACE}\build_cmake_store_${Target}\couchbase-lite-core\$RelPkgDir" "couchbase-lite-core-$Version-$ShaVersion-windows-${arch_lower}-winstore.zip" "STORE_$arch" "RELEASE"
}