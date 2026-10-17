param([string]$Code = "$env:LOCALAPPDATA\Programs\Microsoft VS Code\bin\code.cmd")
$ErrorActionPreference='Stop'
$extensionRoot=Split-Path $PSScriptRoot -Parent
$testRoot=Join-Path $extensionRoot ('.test-run-' + [Guid]::NewGuid().ToString('N'))
$profile=Join-Path $testRoot 'profile'
$extensions=Join-Path $testRoot 'extensions'
$workspace=Join-Path $testRoot 'workspace'
$report=Join-Path $testRoot 'report.json'
[void](New-Item -ItemType Directory -Path $profile,$extensions,$workspace -Force)
$env:LINECALC_TEST_REPORT=$report
$arguments=@(
    '--new-window',"--user-data-dir=$profile","--extensions-dir=$extensions",
    "--extensionDevelopmentPath=$extensionRoot","--extensionTestsPath=$PSScriptRoot\vscode.integration.js",
    '--disable-extensions','--disable-workspace-trust','--skip-welcome','--skip-release-notes',$workspace
)
$quoted=($arguments | ForEach-Object {'"'+$_+'"'}) -join ' '
[void](Start-Process -FilePath $Code -ArgumentList $quoted -WindowStyle Hidden -PassThru)
for($attempt=0;$attempt -lt 240;$attempt++){
    if(Test-Path -LiteralPath $report){break}
    Start-Sleep -Milliseconds 500
}
if(!(Test-Path -LiteralPath $report)){throw "Integration test timed out. Logs: $profile\logs"}
$result=Get-Content -LiteralPath $report -Raw | ConvertFrom-Json
if(!$result.success){throw $result.error}
Write-Output "VS Code integration: $($result.checks) checks passed"
Write-Output "Test report: $report"
