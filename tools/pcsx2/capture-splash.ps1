param([int]$EmulatorId)
$captureDirectory = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../pcsx2-test'))
Add-Type -AssemblyName System.Drawing
$deadline = (Get-Date).AddSeconds(140)
while ((Get-Date) -lt $deadline) {
    & "$PSScriptRoot\view-card.ps1" -EmulatorId $EmulatorId -CaptureName 'splash-probe.png'
    $probe = [System.Drawing.Bitmap]::FromFile("$captureDirectory\splash-probe.png")
    $pixel = $probe.GetPixel([int]($probe.Width * 0.5),[int]($probe.Height * 0.2))
    $isSplash = $pixel.R -gt 80 -and [Math]::Abs([int]$pixel.R - [int]$pixel.G) -lt 25 -and [Math]::Abs([int]$pixel.G - [int]$pixel.B) -lt 25
    $probe.Dispose()
    if ($isSplash) {
        Copy-Item -LiteralPath "$captureDirectory\splash-probe.png" -Destination "$captureDirectory\caduceus-splash-confirmed.png" -Force
        'Caduceus splash captured'
        exit 0
    }
    Start-Sleep -Milliseconds 100
}
throw 'Splash was not captured before the deadline'
