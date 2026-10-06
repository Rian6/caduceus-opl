param([int]$EmulatorId, [int]$Key = -1, [string]$CaptureName = 'card.png', [switch]$ScreenCapture)
$captureDirectory = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../pcsx2-test'))
$null = New-Item -ItemType Directory -Path $captureDirectory -Force
Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Runtime.InteropServices;
public class CardView {
 [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
 [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
 [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h,int n);
 [DllImport("user32.dll")] public static extern bool IsIconic(IntPtr h);
 [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h,IntPtr p);
 [DllImport("kernel32.dll")] public static extern uint GetCurrentThreadId();
 [DllImport("user32.dll")] public static extern bool AttachThreadInput(uint a,uint b,bool attach);
 [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr d, uint f);
 [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out R r);
 [DllImport("user32.dll")] public static extern void keybd_event(byte k,byte s,uint f,UIntPtr e);
 public struct R { public int L,T,Right,B; }
}
'@
$h = (Get-Process -Id $EmulatorId).MainWindowHandle
if (!$h) { throw 'A janela solicitada nao esta disponivel' }
if ([CardView]::IsIconic($h)) { $null=[CardView]::ShowWindow($h,9); Start-Sleep -Milliseconds 300 }
$shell = New-Object -ComObject WScript.Shell
$null = $shell.AppActivate($EmulatorId)
Start-Sleep -Milliseconds 300
if ($Key -ge 0) {
    if ([CardView]::GetForegroundWindow() -ne $h) {
        $foregroundThread = [CardView]::GetWindowThreadProcessId([CardView]::GetForegroundWindow(),[IntPtr]::Zero)
        $callingThread = [CardView]::GetCurrentThreadId()
        $null = [CardView]::AttachThreadInput($callingThread,$foregroundThread,$true)
        try { $null = [CardView]::ShowWindow($h,9); $null = [CardView]::SetForegroundWindow($h) }
        finally { $null = [CardView]::AttachThreadInput($callingThread,$foregroundThread,$false) }
        Start-Sleep -Milliseconds 200
    }
    if ([CardView]::GetForegroundWindow() -ne $h) { throw 'PCSX2 is not focused' }
    $keyFlags = if ($Key -ge 33 -and $Key -le 46) { 1 } else { 0 }
    [CardView]::keybd_event($Key,0,$keyFlags,[UIntPtr]::Zero)
    Start-Sleep -Milliseconds 100
    [CardView]::keybd_event($Key,0,($keyFlags -bor 2),[UIntPtr]::Zero)
    Start-Sleep -Milliseconds 500
}
$r = New-Object CardView+R
$null = [CardView]::GetWindowRect($h,[ref]$r)
$b = New-Object System.Drawing.Bitmap(($r.Right-$r.L),($r.B-$r.T))
$g = [System.Drawing.Graphics]::FromImage($b)
if ($ScreenCapture) {
    $g.CopyFromScreen($r.L, $r.T, 0, 0, $b.Size)
} else {
    $d = $g.GetHdc()
    $null = [CardView]::PrintWindow($h,$d,2)
    $g.ReleaseHdc($d)
}
$b.Save((Join-Path $captureDirectory $CaptureName))
$g.Dispose()
$b.Dispose()
