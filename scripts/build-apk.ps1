# 装好 Android Studio / SDK 后在仓库根目录跑：
#   powershell -File scripts/build-apk.ps1
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
if (-not $env:JAVA_HOME) {
  $jdk = "C:\Program Files\Microsoft\jdk-17.0.20.8-hotspot"
  if (Test-Path $jdk) { $env:JAVA_HOME = $jdk }
}
if (-not $env:ANDROID_HOME) {
  $sdk = Join-Path $env:LOCALAPPDATA "Android\Sdk"
  if (Test-Path $sdk) {
    $env:ANDROID_HOME = $sdk
    $env:ANDROID_SDK_ROOT = $sdk
  }
}
Set-Location "$root\app"
flutter build apk --release --dart-define=API_BASE=http://192.168.1.63:11111
$apk = Join-Path $root "app\build\app\outputs\flutter-apk\app-release.apk"
$destDir = Join-Path $root "cloud\static\downloads"
New-Item -ItemType Directory -Force -Path $destDir | Out-Null
$dest = Join-Path $destDir "bondwatch.apk"
Copy-Item $apk $dest -Force
Write-Host "APK copied to $dest"
Write-Host "Download: http://192.168.1.63:11111/downloads/bondwatch.apk"
Write-Host "Page:     http://192.168.1.63:11111/download"
