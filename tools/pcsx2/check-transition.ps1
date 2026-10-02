param([int]$EmulatorId)
$captureDirectory = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../pcsx2-test'))
$null = New-Item -ItemType Directory -Path $captureDirectory -Force
Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Runtime.InteropServices;
public class XmbSmoke {
 [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
 [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr d, uint f);
 [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out R r);
 [DllImport("user32.dll")] public static extern void keybd_event(byte k,byte s,uint f,UIntPtr e);
 public struct R { public int L,T,Right,B; }
}
'@
$windowHandle = (Get-Process -Id $EmulatorId).MainWindowHandle
$shell = New-Object -ComObject WScript.Shell
$null = $shell.AppActivate($EmulatorId)
if ([XmbSmoke]::GetForegroundWindow() -ne $windowHandle) { throw 'PCSX2 did not receive focus' }
function PressKey([byte]$key) {
    [XmbSmoke]::keybd_event($key,0,0,[UIntPtr]::Zero)
    Start-Sleep -Milliseconds 100
    [XmbSmoke]::keybd_event($key,0,2,[UIntPtr]::Zero)
    Start-Sleep -Milliseconds 250
}
function Capture([string]$name) {
    $rect = New-Object XmbSmoke+R
    $null = [XmbSmoke]::GetWindowRect($windowHandle,[ref]$rect)
    $bitmap = New-Object System.Drawing.Bitmap(($rect.Right-$rect.L),($rect.B-$rect.T))
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $dc = $graphics.GetHdc()
    $null = [XmbSmoke]::PrintWindow($windowHandle,$dc,2)
    $graphics.ReleaseHdc($dc)
    $bitmap.Save((Join-Path $captureDirectory $name))
    $graphics.Dispose()
    $bitmap.Dispose()
}
# Close the missing-network-adapter notice, then open Settings.
PressKey 0x4C
PressKey 0x0D
Capture 'new-settings.png'
# Sample the real transition to Games and back, not a mocked renderer.
foreach ($direction in @(0x27,0x25)) {
    [XmbSmoke]::keybd_event($direction,0,0,[UIntPtr]::Zero)
    for ($frame = 0; $frame -lt 10; $frame++) {
        Capture "transition-$direction-$frame.png"
        if ($frame -eq 0) { [XmbSmoke]::keybd_event($direction,0,2,[UIntPtr]::Zero) }
        Start-Sleep -Milliseconds 30
    }
}
Capture 'new-settings-final.png'
'Captured both transitions and the final menu.'
