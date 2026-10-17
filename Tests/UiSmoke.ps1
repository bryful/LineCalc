param([string]$Executable = "$PSScriptRoot\..\x64\Debug\LineCalc.exe")
$ErrorActionPreference = 'Stop'
Add-Type @'
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class LineCalcUi {
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindowEx(IntPtr p, IntPtr a, string c, string t);
    [DllImport("user32.dll", EntryPoint="SendMessageW", CharSet=CharSet.Unicode)] private static extern IntPtr WriteText(IntPtr h, uint m, IntPtr w, string t);
    public static bool SetWindowText(IntPtr h, string t) { return WriteText(h, 0xC, IntPtr.Zero, t) != IntPtr.Zero; }
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr h, StringBuilder s, int n);
    [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
    [DllImport("user32.dll", EntryPoint="SendMessageW", CharSet=CharSet.Unicode)] public static extern IntPtr ReadText(IntPtr h, uint m, IntPtr w, StringBuilder l);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
    [DllImport("user32.dll")] public static extern bool IsWindowEnabled(IntPtr h);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h,int n);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern bool DestroyWindow(IntPtr h);
    [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern IntPtr CreateWindowEx(uint ex,string c,string t,uint s,int x,int y,int w,int h,IntPtr p,IntPtr m,IntPtr i,IntPtr d);
    [DllImport("user32.dll")] public static extern int GetWindowLong(IntPtr h, int i);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint p);
    [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr h,int id);
    public delegate bool WindowCallback(IntPtr h,IntPtr l);
    [DllImport("user32.dll")] private static extern bool EnumWindows(WindowCallback cb,IntPtr l);
    [DllImport("user32.dll",CharSet=CharSet.Unicode)] private static extern int GetClassName(IntPtr h,StringBuilder s,int n);
    public static IntPtr FindDialog(uint processId) {
        IntPtr found=IntPtr.Zero;
        EnumWindows((h,l)=>{ uint owner; GetWindowThreadProcessId(h,out owner);
            var name=new StringBuilder(256); GetClassName(h,name,256);
            if(owner==processId && name.ToString()=="#32770") { found=h; return false; }
            return true;
        },IntPtr.Zero);
        return found;
    }
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out Rect r);
    [DllImport("user32.dll")] private static extern IntPtr MonitorFromWindow(IntPtr h,uint flags);
    [DllImport("user32.dll",EntryPoint="GetMonitorInfoW")] private static extern bool GetMonitorInfo(IntPtr m,ref MonitorInfo info);
    [StructLayout(LayoutKind.Sequential)] private struct MonitorInfo { public uint Size; public Rect Monitor, Work; public uint Flags; }
    public static Rect WorkArea(IntPtr window) {
        var info=new MonitorInfo(); info.Size=(uint)Marshal.SizeOf<MonitorInfo>();
        if(!GetMonitorInfo(MonitorFromWindow(window,2),ref info)) throw new InvalidOperationException("Cannot get monitor work area");
        return info.Work;
    }
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out Rect r);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
    [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h,IntPtr a,int x,int y,int w,int hgt,uint f);
    [StructLayout(LayoutKind.Sequential)] public struct Rect { public int Left, Top, Right, Bottom; }
}
'@
function Assert($Condition, [string]$Message) {
    if (!$Condition) { throw $Message }
    $script:Checks++
}
function Read-Text([IntPtr]$Handle) {
    $buffer = [Text.StringBuilder]::new(16384)
    [void][LineCalcUi]::ReadText($Handle, 0xD, [IntPtr]$buffer.Capacity, $buffer)
    $buffer.ToString()
}
function Send-Key([int]$Key) {
    [void][LineCalcUi]::SendMessage($script:Edit, 0x100, [IntPtr]$Key, [IntPtr]::Zero)
}
function Assert-PopupInWorkArea([IntPtr]$Popup) {
    $work=[LineCalcUi]::WorkArea($main)
    $bounds=[LineCalcUi+Rect]::new()
    [void][LineCalcUi]::GetWindowRect($Popup,[ref]$bounds)
    Assert ($bounds.Left -ge $work.Left -and $bounds.Top -ge $work.Top -and $bounds.Right -le $work.Right -and $bounds.Bottom -le $work.Bottom) 'Popup extends outside monitor work area'
}
function Evaluate([string]$Source, [string]$Expected) {
    [void][LineCalcUi]::SetWindowText($script:Edit, $Source)
    Send-Key 13
    $actual = Read-Text $script:Edit
    Assert ($actual -ceq $Expected) "Unexpected UI result for $Source : [$actual], expected [$Expected]"
}
function Invoke-TextFunction([string]$Source,[string]$Title) {
    [void][LineCalcUi]::SetWindowText($script:Edit,$Source)
    [void][LineCalcUi]::PostMessage($script:Edit,0x100,[IntPtr]13,[IntPtr]::Zero)
    $dialog=[IntPtr]::Zero
    for($attempt=0;$attempt -lt 100;$attempt++) {
        $dialog=[LineCalcUi]::FindWindowEx([IntPtr]::Zero,[IntPtr]::Zero,'LineCalcText',$Title)
        if($dialog -ne [IntPtr]::Zero) { break }
        Start-Sleep -Milliseconds 100
    }
    Assert ($dialog -ne [IntPtr]::Zero) "Missing dialog for $Source"
    Assert-PopupInWorkArea $dialog
    $text=Read-Text ([LineCalcUi]::GetDlgItem($dialog,101))
    [void][LineCalcUi]::PostMessage($dialog,0x111,[IntPtr]1,[IntPtr]::Zero)
    Start-Sleep -Milliseconds 100
    return $text
}
function Open-FontPicker {
    [void][LineCalcUi]::SetWindowText($script:Edit,'chooseDisplayFont()')
    [void][LineCalcUi]::PostMessage($script:Edit,0x100,[IntPtr]13,[IntPtr]::Zero)
    for($attempt=0;$attempt -lt 100;$attempt++) {
        $dialog=[LineCalcUi]::FindDialog([uint32]$script:AppProcessId)
        if($dialog -ne [IntPtr]::Zero) {
            $sizeControl=[LineCalcUi]::GetDlgItem($dialog,0x472)
            $styleControl=[LineCalcUi]::GetDlgItem($dialog,0x471)
            if($sizeControl -ne [IntPtr]::Zero -and $styleControl -ne [IntPtr]::Zero -and ![LineCalcUi]::IsWindowEnabled($styleControl)) { return $dialog }
        }
        Start-Sleep -Milliseconds 100
    }
    throw 'Font selection dialog did not open'
}
$script:Checks = 0
$definitionFile=Join-Path $PSScriptRoot ('ui-smoke-' + [Guid]::NewGuid().ToString('N') + '.js')
[IO.File]::WriteAllText($definitionFile,'// UI smoke test definitions')
$stateFile=Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) 'LineCalc\LineCalc.json'
$stateBackup=if(Test-Path -LiteralPath $stateFile) { [IO.File]::ReadAllBytes($stateFile) } else { $null }
[void][IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($stateFile))
[IO.File]::WriteAllText($stateFile,'{"x":900000,"y":900000,"width":720,"height":80}')
$process = Start-Process -FilePath (Resolve-Path -LiteralPath $Executable) -ArgumentList ('-include "' + $definitionFile + '"') -WindowStyle Hidden -PassThru
$script:AppProcessId=$process.Id
try {
    $ready = $process.WaitForInputIdle(15000)
    Assert $ready 'Application did not become ready'
    $main = [IntPtr]::Zero
    for ($attempt=0; $attempt -lt 100; $attempt++) {
        $process.Refresh()
        $candidate = [IntPtr]::Zero
        do {
            $candidate = [LineCalcUi]::FindWindowEx([IntPtr]::Zero,$candidate,'LineCalcWindow','LineCalc')
            if ($candidate -ne [IntPtr]::Zero) {
                [uint32]$ownerId = 0
                [void][LineCalcUi]::GetWindowThreadProcessId($candidate,[ref]$ownerId)
                if ($ownerId -eq $process.Id) { $main = $candidate; break }
            }
        } while ($candidate -ne [IntPtr]::Zero)
        if ($main -ne [IntPtr]::Zero -or $process.HasExited) { break }
        Start-Sleep -Milliseconds 100
    }
    Assert ($main -ne [IntPtr]::Zero) 'Main window missing'
    [void][LineCalcUi]::ShowWindow($main,5)
    [void][LineCalcUi]::SendMessage($main,6,[IntPtr]1,[IntPtr]::Zero)
    Assert-PopupInWorkArea $main
    $restoredBounds=New-Object LineCalcUi+Rect
    [void][LineCalcUi]::GetWindowRect($main,[ref]$restoredBounds)
    Assert (($restoredBounds.Right-$restoredBounds.Left) -eq 720) 'Saved width was not restored'
    $script:Edit = [LineCalcUi]::FindWindowEx($main, [IntPtr]::Zero, 'Edit', $null)
    Assert ($script:Edit -ne [IntPtr]::Zero) 'Input missing'
    Assert ([LineCalcUi]::FindWindowEx($main, $script:Edit, $null, $null) -eq [IntPtr]::Zero) 'Unexpected controls'
    $style = [LineCalcUi]::GetWindowLong($main, -16)
    Assert (($style -band 0x30000) -eq 0) 'Minimize/maximize buttons must be absent'
    Assert (($style -band 0x40000) -ne 0) 'Resize frame missing'
    Evaluate 'isTopmost()' 'true'
    $before = [LineCalcUi+Rect]::new()
    $after = [LineCalcUi+Rect]::new()
    [void][LineCalcUi]::GetWindowRect($main,[ref]$before)
    [void][LineCalcUi]::SetWindowPos($main,[IntPtr]::Zero,0,0,700,300,0x16)
    [void][LineCalcUi]::GetWindowRect($main,[ref]$after)
    Assert (($before.Bottom-$before.Top) -eq ($after.Bottom-$after.Top)) 'Vertical size must stay fixed'
    Assert (($after.Right-$after.Left) -eq 700) 'Horizontal resizing failed'
    Evaluate '1+2*3' '7'
    $toast = [LineCalcUi]::FindWindowEx([IntPtr]::Zero,[IntPtr]::Zero,'Static','  OK  ')
    Assert ($toast -ne [IntPtr]::Zero) 'Success notification missing'
    Start-Sleep -Milliseconds 700
    Assert ([LineCalcUi]::FindWindowEx([IntPtr]::Zero,[IntPtr]::Zero,'Static','  OK  ') -eq [IntPtr]::Zero) 'Success notification did not expire'
    [void][LineCalcUi]::SetWindowText($script:Edit,'draft+1')
    Send-Key 38
    Assert ((Read-Text $script:Edit) -eq '1+2*3') 'History up failed'
    Send-Key 40
    Assert ((Read-Text $script:Edit) -eq 'draft+1') 'Draft not restored'
    Evaluate 'M1=-3' '-3'
    [void][LineCalcUi]::SetWindowText($script:Edit,'2*')
    [void][LineCalcUi]::SendMessage($script:Edit,0xB1,[IntPtr]2,[IntPtr]2)
    Send-Key 112
    Assert ((Read-Text $script:Edit) -eq '2*(-3)') 'F1 insertion failed'
    Send-Key 13
    Assert ((Read-Text $script:Edit) -eq '-6') 'Inserted memory calculation failed'
    Evaluate 'setTopmost(true)' ''
    [void][LineCalcUi]::SetWindowText($script:Edit,'retained draft')
    [void][LineCalcUi]::SendMessage($script:Edit,0xB1,[IntPtr]2,[IntPtr]7)
    $selection=[LineCalcUi]::SendMessage($script:Edit,0xB0,[IntPtr]::Zero,[IntPtr]::Zero)
    $normal=[LineCalcUi+Rect]::new()
    $compact=[LineCalcUi+Rect]::new()
    $client=[LineCalcUi+Rect]::new()
    [void][LineCalcUi]::GetWindowRect($main,[ref]$normal)
    [void][LineCalcUi]::GetClientRect($main,[ref]$client)
    $normalClientHeight=$client.Bottom-$client.Top
    [void][LineCalcUi]::SendMessage($main,6,[IntPtr]::Zero,[IntPtr]::Zero)
    [void][LineCalcUi]::GetWindowRect($main,[ref]$compact)
    [void][LineCalcUi]::GetClientRect($main,[ref]$client)
    Assert (($client.Bottom-$client.Top) -eq 0) 'Inactive topmost window must have zero client height'
    Assert (($normal.Bottom-$normal.Top)-($compact.Bottom-$compact.Top) -eq $normalClientHeight) 'Collapse must remove exactly the input row'
    Assert ($compact.Left -eq $normal.Left -and $compact.Top -eq $normal.Top -and $compact.Right -eq $normal.Right) 'Collapse changed position or width'
    Assert (![LineCalcUi]::IsWindowVisible($script:Edit)) 'Collapsed input must be hidden'
    Assert ((Read-Text $script:Edit) -eq 'retained draft') 'Collapse changed input'
    Assert ([LineCalcUi]::SendMessage($script:Edit,0xB0,[IntPtr]::Zero,[IntPtr]::Zero) -eq $selection) 'Collapse changed selection'
    [void][LineCalcUi]::SetWindowPos($main,[IntPtr]::Zero,0,0,750,300,0x16)
    [void][LineCalcUi]::GetClientRect($main,[ref]$client)
    Assert (($client.Bottom-$client.Top) -eq 0) 'Resizing expanded collapsed window'
    [void][LineCalcUi]::SendMessage($main,6,[IntPtr]1,[IntPtr]::Zero)
    [void][LineCalcUi]::GetClientRect($main,[ref]$client)
    Assert (($client.Bottom-$client.Top) -eq $normalClientHeight) 'Activation did not restore input height'
    Assert ([LineCalcUi]::IsWindowVisible($script:Edit)) 'Activation did not show input'
    Assert ((Read-Text $script:Edit) -eq 'retained draft') 'Activation changed draft'
    Assert ([LineCalcUi]::SendMessage($script:Edit,0xB0,[IntPtr]::Zero,[IntPtr]::Zero) -eq $selection) 'Activation changed selection'
    [void][LineCalcUi]::SendMessage($main,6,[IntPtr]::Zero,[IntPtr]::Zero)
    Evaluate 'setTopmost(false)' ''
    [void][LineCalcUi]::GetClientRect($main,[ref]$client)
    Assert (($client.Bottom-$client.Top) -eq $normalClientHeight) 'Disabling topmost must expand inactive window'
    [void][LineCalcUi]::SendMessage($main,6,[IntPtr]1,[IntPtr]::Zero)
    [void][LineCalcUi]::SendMessage($main,6,[IntPtr]::Zero,[IntPtr]::Zero)
    [void][LineCalcUi]::GetClientRect($main,[ref]$client)
    Assert (($client.Bottom-$client.Top) -eq $normalClientHeight) 'Non-topmost window must not collapse'
    Evaluate 'setTopmost(true)' ''
    [void][LineCalcUi]::GetClientRect($main,[ref]$client)
    Assert (($client.Bottom-$client.Top) -eq 0) 'Enabling topmost while inactive must collapse window'
    Evaluate 'setDisplayFont("Segoe UI",24)' ''
    [void][LineCalcUi]::GetClientRect($main,[ref]$client)
    Assert (($client.Bottom-$client.Top) -eq 0) 'Font change must keep inactive topmost window collapsed'
    [void][LineCalcUi]::SendMessage($main,6,[IntPtr]1,[IntPtr]::Zero)
    [void][LineCalcUi]::GetClientRect($main,[ref]$client)
    Assert (($client.Bottom-$client.Top) -gt $normalClientHeight) 'Activation must restore updated font height'
    Evaluate 'setDisplayFont("Segoe UI",12)' ''
    Evaluate 'setTopmost(false)' ''
    Assert (([LineCalcUi]::GetWindowLong($main,-20) -band 8) -eq 0) 'Topmost disable failed'
    Evaluate 'hex(255)' '0xff'
    Send-Key 13
    Assert ((Read-Text $script:Edit) -eq '255') 'Hexadecimal result re-evaluation failed'
    Evaluate 'bin(-10)' '-0b1010'
    Send-Key 13
    Assert ((Read-Text $script:Edit) -eq '-10') 'Negative binary result re-evaluation failed'
    Evaluate 'ans' '-10'
    $fontBefore=[LineCalcUi]::SendMessage($script:Edit,0x31,[IntPtr]::Zero,[IntPtr]::Zero)
    [void][LineCalcUi]::GetWindowRect($main,[ref]$before)
    Evaluate 'setDisplayFont("Segoe UI",24)' ''
    [void][LineCalcUi]::GetWindowRect($main,[ref]$after)
    Assert (($after.Bottom-$after.Top) -gt ($before.Bottom-$before.Top)) 'Font change must increase one-line height'
    Assert (($after.Right-$after.Left) -eq ($before.Right-$before.Left)) 'Font change must preserve width'
    Assert ([LineCalcUi]::SendMessage($script:Edit,0x31,[IntPtr]::Zero,[IntPtr]::Zero) -ne $fontBefore) 'Font handle not updated'
    $fontText=Invoke-TextFunction 'getDisplayFont()' '表示フォント設定'
    Assert ($fontText -eq 'setDisplayFont("Segoe UI", 24);') 'Font configuration text mismatch'
    $fontList=Invoke-TextFunction 'fonts()' 'インストール済みフォント'
    Assert ($fontList.Contains('Segoe UI')) 'Font list missing installed face'
    $picker=Open-FontPicker
    Assert (![LineCalcUi]::IsWindowEnabled([LineCalcUi]::GetDlgItem($picker,0x471))) 'Font style control must be disabled'
    [void][LineCalcUi]::PostMessage($picker,0x111,[IntPtr]2,[IntPtr]::Zero)
    Start-Sleep -Milliseconds 200
    $fontText=Invoke-TextFunction 'getDisplayFont()' '表示フォント設定'
    Assert ($fontText -eq 'setDisplayFont("Segoe UI", 24);') 'Font picker cancel changed settings'
    $picker=Open-FontPicker
    [void][LineCalcUi]::SetWindowText([LineCalcUi]::GetDlgItem($picker,0x472),'22')
    [void][LineCalcUi]::PostMessage($picker,0x111,[IntPtr]1,[IntPtr]::Zero)
    Start-Sleep -Milliseconds 200
    $fontText=Invoke-TextFunction 'getDisplayFont()' '表示フォント設定'
    Assert ($fontText -eq 'setDisplayFont("Segoe UI", 22);') 'Font picker selection not applied'
    $fontBefore=[LineCalcUi]::SendMessage($script:Edit,0x31,[IntPtr]::Zero,[IntPtr]::Zero)
    $fontError=Invoke-TextFunction 'setDisplayFont("LineCalc-font-not-found",14)' 'LineCalc — ERROR'
    Assert ($fontError.Length -gt 0) 'Invalid font name must produce an error'
    Assert ([LineCalcUi]::SendMessage($script:Edit,0x31,[IntPtr]::Zero,[IntPtr]::Zero) -eq $fontBefore) 'Invalid font changed active font'
    Evaluate 'setDisplayFont("Segoe UI",12)' ''
    Evaluate 'setTopmost(true)' ''
    $probe=[LineCalcUi]::CreateWindowEx(0,'STATIC','LineCalc activation probe',0xC80000,0,0,240,80,[IntPtr]::Zero,[IntPtr]::Zero,[IntPtr]::Zero,[IntPtr]::Zero)
    Assert ($probe -ne [IntPtr]::Zero) 'Cannot create activation test window'
    try {
        [void][LineCalcUi]::ShowWindow($probe,5)
        Assert ([LineCalcUi]::SetForegroundWindow($probe)) 'Cannot activate test window'
        for($attempt=0;$attempt -lt 50;$attempt++) {
            [void][LineCalcUi]::GetClientRect($main,[ref]$client)
            if($client.Bottom -eq $client.Top) { break }
            Start-Sleep -Milliseconds 20
        }
        Assert (($client.Bottom-$client.Top) -eq 0) 'Real deactivation did not collapse window'
        Assert ([LineCalcUi]::SetForegroundWindow($main)) 'Cannot reactivate calculator'
        for($attempt=0;$attempt -lt 50;$attempt++) {
            [void][LineCalcUi]::GetClientRect($main,[ref]$client)
            if($client.Bottom -gt $client.Top) { break }
            Start-Sleep -Milliseconds 20
        }
        Assert (($client.Bottom-$client.Top) -eq $normalClientHeight) 'Real activation did not restore row'
    }
    finally { [void][LineCalcUi]::DestroyWindow($probe) }
    [void][LineCalcUi]::SetWindowText($script:Edit,'1+')
    [void][LineCalcUi]::PostMessage($script:Edit,0x100,[IntPtr]13,[IntPtr]::Zero)
    $dialog = [IntPtr]::Zero
    for ($attempt=0; $attempt -lt 50; $attempt++) {
        $dialog = [LineCalcUi]::FindWindowEx([IntPtr]::Zero,[IntPtr]::Zero,'LineCalcText','LineCalc — ERROR')
        if ($dialog -ne [IntPtr]::Zero) { break }
        Start-Sleep -Milliseconds 100
    }
    Assert ($dialog -ne [IntPtr]::Zero) 'Error confirmation missing'
    Assert (![LineCalcUi]::IsWindowEnabled($main)) 'Error must wait for confirmation'
    [void][LineCalcUi]::GetClientRect($main,[ref]$client)
    Assert (($client.Bottom-$client.Top) -eq 0) 'Topmost window must collapse while its error dialog is active'
    Assert ((Read-Text $script:Edit) -eq '1+') 'Error changed input'
    [void][LineCalcUi]::PostMessage($dialog,0x111,[IntPtr]1,[IntPtr]::Zero)
    Start-Sleep -Milliseconds 100
    Assert ([LineCalcUi]::IsWindowEnabled($main)) 'Confirmation did not restore input'
    [void][LineCalcUi]::GetClientRect($main,[ref]$client)
    Assert (($client.Bottom-$client.Top) -eq $normalClientHeight) 'Error confirmation did not restore row height'
    Evaluate 'clearHis()' 'clearHis()'
    Send-Key 38
    Assert ((Read-Text $script:Edit) -eq '') 'History was not cleared'
    Evaluate 'setTopmost(false)' ''
    $work=[LineCalcUi]::WorkArea($main)
    $mainBounds=[LineCalcUi+Rect]::new()
    [void][LineCalcUi]::GetWindowRect($main,[ref]$mainBounds)
    $width=$mainBounds.Right-$mainBounds.Left
    $height=$mainBounds.Bottom-$mainBounds.Top
    foreach($corner in @(
        @($work.Left,$work.Top),@(($work.Right-$width),$work.Top),
        @($work.Left,($work.Bottom-$height)),@(($work.Right-$width),($work.Bottom-$height))
    )) {
        [void][LineCalcUi]::SetWindowPos($main,[IntPtr]::Zero,[int]$corner[0],[int]$corner[1],0,0,0x15)
        Evaluate '1+1' '2'
        $notification=[LineCalcUi]::FindWindowEx([IntPtr]::Zero,[IntPtr]::Zero,'Static','  OK  ')
        Assert ($notification -ne [IntPtr]::Zero) 'Corner notification missing'
        Assert-PopupInWorkArea $notification
        $details=Invoke-TextFunction '1+' 'LineCalc — ERROR'
        Assert ($details.Length -gt 0) 'Corner error details missing'
    }
    Evaluate 'setTopmost(true)' ''
    [void][LineCalcUi]::SendMessage($main,6,[IntPtr]1,[IntPtr]::Zero)
    $details=Invoke-TextFunction '1+' 'LineCalc — ERROR'
    Assert ($details.Length -gt 0) 'Topmost corner error details missing'
    $normalBounds=New-Object LineCalcUi+Rect
    [void][LineCalcUi]::GetWindowRect($main,[ref]$normalBounds)
    [void][LineCalcUi]::SendMessage($main,6,[IntPtr]::Zero,[IntPtr]::Zero)
    [void]$process.CloseMainWindow()
    Assert ($process.WaitForExit(5000)) 'Application did not close'
    $savedState=[IO.File]::ReadAllText($stateFile) | ConvertFrom-Json
    Assert ($savedState.x -eq $normalBounds.Left -and $savedState.y -eq $normalBounds.Top) 'Window position was not saved'
    Assert ($savedState.width -eq ($normalBounds.Right-$normalBounds.Left)) 'Window width was not saved'
    Assert ($savedState.height -eq ($normalBounds.Bottom-$normalBounds.Top)) 'Collapsed window must save normal height'
    Write-Output "$script:Checks UI checks passed"
}
finally {
    if (!$process.HasExited) {
        [void]$process.CloseMainWindow()
        if (!$process.WaitForExit(3000)) { $process.Kill() }
    }
    $process.Dispose()
    if ($null -ne $stateBackup) { [IO.File]::WriteAllBytes($stateFile,$stateBackup) }
    else { Remove-Item -LiteralPath $stateFile -ErrorAction SilentlyContinue }
    Remove-Item -LiteralPath $definitionFile
}
