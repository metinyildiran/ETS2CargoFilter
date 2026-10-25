$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$projectRoot = Split-Path -Parent $PSScriptRoot
[System.IO.Directory]::CreateDirectory((Join-Path $projectRoot 'workshop')) | Out-Null

# Authored geometric artwork; no game logos, screenshots or external assets.
function Write-Cover([string]$Path, [int]$Width, [int]$Height, [bool]$Compact) {
    $bitmap = [System.Drawing.Bitmap]::new($Width, $Height)
    $g = [System.Drawing.Graphics]::FromImage($bitmap)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
    $g.ScaleTransform($Width / 1104.0, $Height / 648.0)
    $bg = [System.Drawing.SolidBrush]::new([System.Drawing.ColorTranslator]::FromHtml('#182127'))
    $panel = [System.Drawing.SolidBrush]::new([System.Drawing.ColorTranslator]::FromHtml('#293740'))
    $yellow = [System.Drawing.SolidBrush]::new([System.Drawing.ColorTranslator]::FromHtml('#F4B619'))
    $white = [System.Drawing.SolidBrush]::new([System.Drawing.ColorTranslator]::FromHtml('#F2F4F5'))
    $muted = [System.Drawing.SolidBrush]::new([System.Drawing.ColorTranslator]::FromHtml('#AEBEC6'))
    $line = [System.Drawing.Pen]::new([System.Drawing.ColorTranslator]::FromHtml('#4A5D67'), 3)
    $big = [System.Drawing.Font]::new('Segoe UI', 74, [System.Drawing.FontStyle]::Bold, [System.Drawing.GraphicsUnit]::Pixel)
    $small = [System.Drawing.Font]::new('Segoe UI', 27, [System.Drawing.FontStyle]::Bold, [System.Drawing.GraphicsUnit]::Pixel)
    $label = [System.Drawing.Font]::new('Segoe UI', 34, [System.Drawing.FontStyle]::Bold, [System.Drawing.GraphicsUnit]::Pixel)
    try {
        $g.FillRectangle($bg, 0, 0, 1104, 648)
        $g.FillRectangle($yellow, 0, 0, 12, 648)
        $g.FillRectangle($panel, 60, 62, 145, 56)
        $g.DrawString('ETS2', $label, $yellow, 82, 68)
        $g.DrawString('CARGO', $big, $white, 56, 142)
        $g.DrawString('FILTER', $big, $white, 56, 223)
        $g.DrawString('OWNED TRAILERS', $small, $muted, 62, 326)
        # Trailer silhouette plus filter selection rows.
        $g.FillRectangle($panel, 620, 130, 420, 136)
        $g.DrawRectangle($line, 620, 130, 420, 136)
        $g.FillRectangle($yellow, 620, 252, 420, 14)
        foreach ($x in @(890, 940, 990)) {
            $g.FillEllipse($bg, $x, 253, 40, 40)
            $g.FillEllipse($muted, ($x + 11), 264, 18, 18)
        }
        $g.FillRectangle($muted, 650, 266, 8, 27)
        foreach ($y in @(339, 385, 431)) {
            $g.FillRectangle($panel, 620, $y, 420, 34)
            $g.FillRectangle($yellow, 630, ($y + 9), 16, 16)
            $g.FillRectangle($muted, 664, ($y + 13), 220, 8)
        }
        $g.FillRectangle($yellow, 60, 529, 980, 66)
        $g.DrawString('PLUGIN REQUIRED', $label, $bg, 84, 538)
        if (-not $Compact) { $g.DrawString('43 LOCALES  /  WINDOWS x64', $small, $muted, 62, 415) }
        $bitmap.Save($Path, [System.Drawing.Imaging.ImageFormat]::Jpeg)
    } finally {
        foreach ($item in @($g, $bitmap, $bg, $panel, $yellow, $white, $muted, $line, $big, $small, $label)) { $item.Dispose() }
    }
}
Write-Cover (Join-Path $projectRoot 'mod/mod_icon.jpg') 276 162 $true
Write-Cover (Join-Path $projectRoot 'workshop/preview.jpg') 640 360 $false
