param([string]$Executable = "$PSScriptRoot\..\x64\Debug\LineCalc.exe")
$ErrorActionPreference = 'Stop'
$executablePath = (Resolve-Path -LiteralPath $Executable).Path
Add-Type @'
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class LineCalcDebugUi {
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindowEx(IntPtr p, IntPtr a, string c, string t);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint p);
    [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr h, int id);
    [DllImport("user32.dll", EntryPoint="SendMessageW", CharSet=CharSet.Unicode)] public static extern IntPtr ReadText(IntPtr h, uint m, IntPtr w, StringBuilder l);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
}
'@
$testDirectory = Join-Path $PSScriptRoot ('debug-smoke-' + [Guid]::NewGuid().ToString('N'))
[void](New-Item -ItemType Directory -Path $testDirectory)
function Test-Debug([string]$Arguments, [string]$Title, [string]$ExpectedText, [int]$ExitCode) {
    $process = Start-Process -FilePath $executablePath -ArgumentList $Arguments -WorkingDirectory $testDirectory -WindowStyle Hidden -PassThru
    try {
        $dialog = [IntPtr]::Zero
        for ($attempt=0; $attempt -lt 150; $attempt++) {
            $candidate = [IntPtr]::Zero
            do {
                $candidate = [LineCalcDebugUi]::FindWindowEx([IntPtr]::Zero,$candidate,'LineCalcText',$Title)
                if ($candidate -ne [IntPtr]::Zero) {
                    [uint32]$ownerId = 0
                    [void][LineCalcDebugUi]::GetWindowThreadProcessId($candidate,[ref]$ownerId)
                    if ($ownerId -eq $process.Id) { $dialog=$candidate; break }
                }
            } while ($candidate -ne [IntPtr]::Zero)
            if ($dialog -ne [IntPtr]::Zero -or $process.HasExited) { break }
            Start-Sleep -Milliseconds 100
        }
        if ($dialog -eq [IntPtr]::Zero) { throw "Missing result dialog: $Arguments" }
        # 読み取り専用EDITのテキストをプロセス間メッセージで取得する。
        $input = [LineCalcDebugUi]::GetDlgItem($dialog,101)
        $buffer = [Text.StringBuilder]::new(16384)
        [void][LineCalcDebugUi]::ReadText($input,0xD,[IntPtr]$buffer.Capacity,$buffer)
        if (!$buffer.ToString().Contains($ExpectedText)) { throw "Unexpected result: $buffer" }
        $main = [LineCalcDebugUi]::FindWindowEx([IntPtr]::Zero,[IntPtr]::Zero,'LineCalcWindow','LineCalc')
        if ($main -ne [IntPtr]::Zero) {
            [uint32]$ownerId=0
            [void][LineCalcDebugUi]::GetWindowThreadProcessId($main,[ref]$ownerId)
            if ($ownerId -eq $process.Id) { throw 'Debug mode opened calculator window' }
        }
        [void][LineCalcDebugUi]::PostMessage($dialog,0x111,[IntPtr]1,[IntPtr]::Zero)
        if (!$process.WaitForExit(5000)) { throw 'Debug process did not exit after confirmation' }
        if ($process.ExitCode -ne $ExitCode) { throw "Wrong exit code: $($process.ExitCode)" }
        $script:checks++
    }
    finally {
        if (!$process.HasExited) { $process.Kill(); $process.WaitForExit() }
        $process.Dispose()
    }
}
$script:checks=0
try {
    [IO.File]::WriteAllText((Join-Path $testDirectory 'valid.js'), 'function triple(v){return v*3;} triple(2);')
    [IO.File]::WriteAllText((Join-Path $testDirectory 'syntax.js'), "function good(){return 1;}`nlet bad = ;")
    [IO.File]::WriteAllText((Join-Path $testDirectory 'runtime.js'), "function bad(){`nreturn missing();`n}`nbad();")
    Test-Debug '-debug valid.js' 'LineCalc — OK' 'OK' 0
    Test-Debug '-debug -include valid.js' 'LineCalc — OK' 'OK' 0
    Test-Debug '-include valid.js -debug' 'LineCalc — OK' 'OK' 0
    Test-Debug '-debug syntax.js' 'LineCalc — ERROR' '行 2、列 11' 1
    Test-Debug '-debug runtime.js' 'LineCalc — ERROR' ':2:' 1
    Test-Debug '-debug absent.js' 'LineCalc — ERROR' 'absent.js' 1
    Test-Debug '-createjs ./generated.js' 'LineCalc — OK' '雛形を作成しました。' 0
    $generatedFile = Join-Path $testDirectory 'generated.js'
    if (!(Test-Path -LiteralPath $generatedFile -PathType Leaf)) { throw 'Template was not created in current directory' }
    $generatedBytes = [IO.File]::ReadAllBytes($generatedFile)
    Test-Debug '-debug generated.js' 'LineCalc — OK' 'OK' 0
    Test-Debug '-createjs ./generated.js' 'LineCalc — ERROR' 'generated.js' 1
    if ([Convert]::ToBase64String([IO.File]::ReadAllBytes($generatedFile)) -ne [Convert]::ToBase64String($generatedBytes)) {
        throw 'Existing template was modified'
    }
    Write-Output "$script:checks debug CLI checks passed"
}
finally {
    foreach ($name in @('valid.js','syntax.js','runtime.js','generated.js')) {
        Remove-Item -LiteralPath (Join-Path $testDirectory $name) -ErrorAction SilentlyContinue
    }
    Remove-Item -LiteralPath $testDirectory
}
