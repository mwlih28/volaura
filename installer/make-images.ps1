Add-Type -AssemblyName System.Drawing
$src = (Resolve-Path "$PSScriptRoot\..\assets\volaura-logo.png").Path
$img = [System.Drawing.Image]::FromFile($src)

function ResizeBmp($img, $w, $h, $out, $bgColor) {
    $bmp = New-Object System.Drawing.Bitmap($w, $h)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = 'AntiAlias'
    $g.InterpolationMode = 'HighQualityBicubic'
    $g.Clear($bgColor)
    $ratio = [Math]::Min($w / $img.Width, $h / $img.Height) * 0.65
    $nw = [int]($img.Width * $ratio)
    $nh = [int]($img.Height * $ratio)
    $x = [int](($w - $nw) / 2)
    $y = [int](($h - $nh) / 2)
    $g.DrawImage($img, $x, $y, $nw, $nh)
    $g.Dispose()
    $bmp.Save($out, [System.Drawing.Imaging.ImageFormat]::Bmp)
    $bmp.Dispose()
    Write-Host "Created: $out ($w x $h)"
}

$brandBg = [System.Drawing.Color]::FromArgb(15, 12, 32)
$smallBg = [System.Drawing.Color]::FromArgb(31, 22, 56)

$outDir = "$PSScriptRoot\images"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
ResizeBmp $img 497 312 "$outDir\wizard-large.bmp" $brandBg
ResizeBmp $img 164 314 "$outDir\wizard-classic.bmp" $brandBg
ResizeBmp $img 55  58  "$outDir\wizard-small.bmp" $smallBg
ResizeBmp $img 110 116 "$outDir\wizard-small2x.bmp" $smallBg
$img.Dispose()
Write-Host "DONE"
