param([Parameter(Mandatory=$true)][int]$AppProcessId, [ValidateSet('debug','include')][string]$Mode, [ValidateSet('OK','ERROR')][string]$Expected='OK')
$ErrorActionPreference='Stop'
[Console]::OutputEncoding=[Text.UTF8Encoding]::new($false)
Add-Type @'
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class LineCalcExtensionTest {
    public delegate bool Callback(IntPtr h,IntPtr l);
    [DllImport("user32.dll")] private static extern bool EnumWindows(Callback cb,IntPtr l);
    [DllImport("user32.dll")] private static extern uint GetWindowThreadProcessId(IntPtr h,out uint p);
    [DllImport("user32.dll",CharSet=CharSet.Unicode)] private static extern int GetClassName(IntPtr h,StringBuilder s,int n);
    [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr h,int id);
    [DllImport("user32.dll",EntryPoint="SendMessageW",CharSet=CharSet.Unicode)] public static extern IntPtr ReadText(IntPtr h,uint m,IntPtr w,StringBuilder t);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h,uint m,IntPtr w,IntPtr l);
    public static IntPtr Find(uint process,string target) {
        IntPtr found=IntPtr.Zero;
        EnumWindows((h,l)=>{
            uint owner; GetWindowThreadProcessId(h,out owner);
            var name=new StringBuilder(256); GetClassName(h,name,256);
            if(owner==process && name.ToString()==target){found=h;return false;}
            return true;
        },IntPtr.Zero);
        return found;
    }
}
'@
$className=if($Mode -eq 'debug'){'LineCalcText'}else{'LineCalcWindow'}
$window=[IntPtr]::Zero
for($attempt=0;$attempt -lt 150;$attempt++){
    $window=[LineCalcExtensionTest]::Find([uint32]$AppProcessId,$className)
    if($window -ne [IntPtr]::Zero){break}
    Start-Sleep -Milliseconds 100
}
if($window -eq [IntPtr]::Zero){throw "Test process $AppProcessId has no $className window"}
if($Mode -eq 'debug'){
    $text=[Text.StringBuilder]::new(16384)
    [void][LineCalcExtensionTest]::ReadText([LineCalcExtensionTest]::GetDlgItem($window,101),0xD,[IntPtr]$text.Capacity,$text)
    if($Expected -eq 'OK' -and !$text.ToString().EndsWith('OK')){throw "Unexpected debug result: $text"}
    if($Expected -eq 'ERROR' -and !$text.ToString().Contains('SyntaxError')){throw "Expected a syntax error: $text"}
    [void][LineCalcExtensionTest]::PostMessage($window,0x111,[IntPtr]1,[IntPtr]::Zero)
    Write-Output "debug $Expected"
    if($Expected -eq 'ERROR'){Write-Output $text.ToString()}
}else{
    [void][LineCalcExtensionTest]::PostMessage($window,0x10,[IntPtr]::Zero,[IntPtr]::Zero)
    Write-Output 'include OK'
}
