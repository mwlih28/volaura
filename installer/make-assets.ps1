#
# VoLaura logosundan Inno Setup varliklarini uretir:
#   assets\volaura-logo.ico         (exe ikonu, multi-res)
#   installer\wizard-banner.bmp     (164x314 sol banner)
#   installer\wizard-small.bmp      (55x58  sag ust)
#
# Kullanim: powershell -ExecutionPolicy Bypass -File installer\make-assets.ps1
#

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$root       = Split-Path -Parent $PSScriptRoot
$srcPng     = Join-Path $root 'assets\volaura-logo.png'
$outIco     = Join-Path $root 'assets\volaura-logo.ico'
$outBanner  = Join-Path $root 'installer\wizard-banner.bmp'
$outSmall   = Join-Path $root 'installer\wizard-small.bmp'

if (-not (Test-Path $srcPng)) { throw "Bulunamadi: $srcPng" }

$src = [System.Drawing.Image]::FromFile($srcPng)

function New-Bmp([int]$w, [int]$h, [System.Drawing.Color]$bg, [int]$logoSize) {
    $bmp = New-Object System.Drawing.Bitmap $w, $h, ([System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
    $g   = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode     = 'AntiAlias'
    $g.InterpolationMode = 'HighQualityBicubic'
    $g.Clear($bg)
    $x = [int](($w - $logoSize) / 2)
    $y = [int](($h - $logoSize) / 2)
    $g.DrawImage($src, $x, $y, $logoSize, $logoSize)
    $g.Dispose()
    return $bmp
}

# Koyu tema rengi (VoLaura kart arka plani)
$dark = [System.Drawing.Color]::FromArgb(17, 20, 28)   # #11141c

# 1) 164x314 banner
$banner = New-Bmp 164 314 $dark 120
$banner.Save($outBanner, [System.Drawing.Imaging.ImageFormat]::Bmp)
$banner.Dispose()
Write-Host "[OK] $outBanner"

# 2) 55x58 kucuk ikon
$small = New-Bmp 55 58 $dark 48
$small.Save($outSmall, [System.Drawing.Imaging.ImageFormat]::Bmp)
$small.Dispose()
Write-Host "[OK] $outSmall"

# 3) Multi-resolution .ico
$sizes = @(16, 32, 48, 64, 128, 256)
$pngBytes = @()
foreach ($sz in $sizes) {
    $bmp = New-Object System.Drawing.Bitmap $sz, $sz, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g   = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode     = 'AntiAlias'
    $g.InterpolationMode = 'HighQualityBicubic'
    $g.Clear([System.Drawing.Color]::Transparent)
    $g.DrawImage($src, 0, 0, $sz, $sz)
    $g.Dispose()
    $ms = New-Object System.IO.MemoryStream
    $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
    $pngBytes += ,($ms.ToArray())
    $ms.Dispose()
    $bmp.Dispose()
}

# ICO formatı: ICONDIR + ICONDIRENTRY[] + data
$fs = [System.IO.File]::Create($outIco)
$bw = New-Object System.IO.BinaryWriter $fs
$bw.Write([UInt16]0)              # reserved
$bw.Write([UInt16]1)              # type: icon
$bw.Write([UInt16]$sizes.Count)   # count
$offset = 6 + (16 * $sizes.Count)
for ($i = 0; $i -lt $sizes.Count; $i++) {
    $sz = $sizes[$i]
    $data = $pngBytes[$i]
    $bw.Write([byte]([math]::Min($sz, 255) % 256))  # width (0 = 256)
    $bw.Write([byte]([math]::Min($sz, 255) % 256))  # height
    $bw.Write([byte]0)                              # palette
    $bw.Write([byte]0)                              # reserved
    $bw.Write([UInt16]1)                            # color planes
    $bw.Write([UInt16]32)                           # bpp
    $bw.Write([UInt32]$data.Length)                 # data size
    $bw.Write([UInt32]$offset)                      # data offset
    $offset += $data.Length
}
for ($i = 0; $i -lt $sizes.Count; $i++) { $bw.Write($pngBytes[$i]) }
$bw.Flush(); $bw.Close(); $fs.Close()
Write-Host "[OK] $outIco"

$src.Dispose()
Write-Host "`nHepsi hazir."
