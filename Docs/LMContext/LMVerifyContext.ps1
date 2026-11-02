# Run from any folder. -StaticOnly does not execute upstream hook scripts.
param([switch]$StaticOnly)
$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [Text.UTF8Encoding]::new($false)
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$passed = [Collections.Generic.List[string]]::new()
function Check([bool]$Condition, [string]$Name) {
    if (-not $Condition) { throw "FAIL: $Name" }
    $passed.Add($Name)
    Write-Output "PASS: $Name"
}
$lock = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'upstream-lock.json') -Raw -Encoding utf8 | ConvertFrom-Json
foreach ($file in $lock.files) {
    $path = Join-Path $root $file.path
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing: $($file.path)" }
    if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ine $file.installedSha256) { throw "Hash mismatch: $($file.path)" }
}
Check ($lock.commit -eq 'b6580cd24af1d8a95ab268794456e05608a1afd3') 'Pinned upstream commit and all vendored file hashes'
$config = Get-Content -LiteralPath (Join-Path $root '.codex/hooks.json') -Raw -Encoding utf8 | ConvertFrom-Json
$expected = @('SessionStart','UserPromptSubmit','PreToolUse','PermissionRequest','PostToolUse','PreCompact','Stop')
Check (@($config.hooks.PSObject.Properties.Name).Count -eq 7) 'Seven expected lifecycle event registrations'
foreach ($event in $expected) { if (-not $config.hooks.$event) { throw "Missing event: $event" } }
Check ('compact' -match $config.hooks.SessionStart[0].matcher) 'SessionStart matches compact'
$plain = Get-Content -LiteralPath (Join-Path $root '.codex/hooks/LMHookEntry.txt') -Raw -Encoding utf8
$command = [string]$config.hooks.SessionStart[0].hooks[0].commandWindows
$decoded = [Text.Encoding]::Unicode.GetString([Convert]::FromBase64String(($command -split ' ')[-1]))
Check ($decoded -ceq $plain) 'Windows encoded entry equals the reviewable plaintext entry'
Check ($command -notmatch 'ExecutionPolicy|Bypass') 'No execution-policy or hook-trust bypass'
$tokens = $null; $errors = $null
[Management.Automation.Language.Parser]::ParseFile((Join-Path $root '.codex/hooks/LMInvokePlanning.ps1'), [ref]$tokens, [ref]$errors) | Out-Null
Check ($errors.Count -eq 0) 'Windows adapter PowerShell syntax'
foreach ($relative in @('resolve-plan-dir.sh','session-start.sh','stop.sh')) {
    $text = Get-Content -LiteralPath (Join-Path $root ".codex/hooks/$relative") -Raw -Encoding utf8
    if ($text -notmatch '\.agents/skills/planning-with-files') { throw "Wrong shared skill path: $relative" }
}
Check $true 'All three runtime references use the canonical .agents skill'
if ($StaticOnly) { Write-Output "Static verification passed ($($passed.Count) checks). Hook execution and real Codex compact were not tested."; exit 0 }

$fixture = Join-Path ([IO.Path]::GetTempPath()) ('LMContext test ' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $fixture -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $root '.agents') -Destination $fixture -Recurse
Copy-Item -LiteralPath (Join-Path $root '.codex') -Destination $fixture -Recurse
$utf8 = [Text.UTF8Encoding]::new($false)
foreach ($id in @('task-a','task-b')) {
    $dir = Join-Path $fixture ".planning/$id"
    New-Item -ItemType Directory -Path $dir -Force | Out-Null
    [IO.File]::WriteAllText((Join-Path $dir 'task_plan.md'), "# Task $id`n## Goal`nGOAL-$id`n## Next Step`nNEXT-$id`n## Current Phase`nPhase 1`n### Phase 1: Verification`n**Status:** in_progress`n", $utf8)
    [IO.File]::WriteAllText((Join-Path $dir 'findings.md'), "# Findings`nDECISION-$id`n", $utf8)
    [IO.File]::WriteAllText((Join-Path $dir 'progress.md'), "# Progress`nPROGRESS-$id`n", $utf8)
}
function Bind-Task([string]$Session, [string]$Id) {
    $sha = [Security.Cryptography.SHA256]::Create()
    try { $key = ([BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($Session)))).Replace('-', '').ToLowerInvariant() }
    finally { $sha.Dispose() }
    $dir = Join-Path $fixture 'Saved/LMContext/Sessions'
    New-Item -ItemType Directory -Path $dir -Force | Out-Null
    [IO.File]::WriteAllText((Join-Path $dir "$key.plan"), $Id, $utf8)
}
function Run-Hook([string]$Event, [string]$Session='qa-a', [string]$Source='startup', [string]$WorkDir=$fixture, [bool]$Disabled=$false, [string]$Pin='') {
    $start = [Diagnostics.ProcessStartInfo]::new()
    $start.FileName = Join-Path $env:SystemRoot 'System32/WindowsPowerShell/v1.0/powershell.exe'
    $start.Arguments = $command.Substring($command.IndexOf(' ') + 1)
    $start.WorkingDirectory = $WorkDir
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.RedirectStandardInput = $true
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    $start.StandardOutputEncoding = $utf8
    $start.StandardErrorEncoding = $utf8
    foreach ($key in @('PLAN_ID','PWF_PLAN_ROOT','PLANNING_DISABLED','PWF_SESSION_ID','PYTHON_BIN')) { $start.EnvironmentVariables.Remove($key) }
    if ($Disabled) { $start.EnvironmentVariables['PLANNING_DISABLED']='1' }
    if ($Pin) { $start.EnvironmentVariables['PLAN_ID']=$Pin }
    $process = [Diagnostics.Process]::Start($start)
    $payload = @{hook_event_name=$Event;session_id=$Session;cwd=$WorkDir;source=$Source;trigger='manual';tool_name='apply_patch';stop_hook_active=$false} | ConvertTo-Json -Compress
    $process.StandardInput.Write($payload)
    $process.StandardInput.Close()
    $stdout = $process.StandardOutput.ReadToEnd()
    $stderr = $process.StandardError.ReadToEnd()
    if (-not $process.WaitForExit(20000)) { $process.Kill(); throw "Hook timeout: $Event" }
    if ($process.ExitCode -ne 0 -or $stderr) { throw "Hook error: $Event $stderr" }
    $process.Dispose()
    return $stdout.Trim()
}
Bind-Task 'qa-a' 'task-a'
Bind-Task 'qa-b' 'task-b'
$before = Get-ChildItem -LiteralPath (Join-Path $fixture '.planning') -Recurse -File | Get-FileHash -Algorithm SHA256
foreach ($source in @('startup','resume','compact','fork')) {
    $result = Run-Hook -Event SessionStart -Source $source | ConvertFrom-Json
    $context = [string]$result.hookSpecificOutput.additionalContext
    Check ($context.Contains('NEXT-task-a') -and $context.Contains('PROGRESS-task-a') -and $context.Contains('findings.md')) "SessionStart $source restores selected plan/progress and requests full findings"
}
$contextB = (Run-Hook -Event UserPromptSubmit -Session 'qa-b' | ConvertFrom-Json).hookSpecificOutput.additionalContext
Check ($contextB.Contains('NEXT-task-b') -and -not $contextB.Contains('NEXT-task-a')) 'Concurrent conversation uses its own task'
$unbound = Run-Hook -Event SessionStart -Session 'qa-unbound'
Check ($unbound.Contains('No task is bound') -and -not $unbound.Contains('NEXT-task')) 'Unbound conversation never guesses a task'
$invalid = Run-Hook -Event SessionStart -Pin '../task-a'
Check ($invalid.Contains('Recovery unavailable') -and -not $invalid.Contains('NEXT-task')) 'Traversal selector is refused without fallback'
Check ([string]::IsNullOrEmpty((Run-Hook -Event SessionStart -Disabled $true))) 'PLANNING_DISABLED produces no hook context'
$nested = Join-Path $fixture 'Source/Nested Folder'
New-Item -ItemType Directory -Path $nested -Force | Out-Null
$nestedContext = (Run-Hook -Event SessionStart -WorkDir $nested | ConvertFrom-Json).hookSpecificOutput.additionalContext
Check ($nestedContext.Contains('NEXT-task-a')) 'Windows command resolves from a nested directory and path with spaces'
$precompact = Run-Hook -Event PreCompact | ConvertFrom-Json
Check ($precompact.continue -eq $true -and $precompact.systemMessage.Contains('PreCompact')) 'PreCompact emits an advisory reminder'
foreach ($event in @('PreToolUse','PermissionRequest','PostToolUse','Stop')) {
    $out = Run-Hook -Event $event
    if ($out) { $json = $out | ConvertFrom-Json; if ($out.Contains('Recovery unavailable')) { throw "Runtime failure: $event" } }
}
Check $true 'Remaining four upstream hooks execute with valid output or permitted silence'
Bind-Task 'qa-a' 'task-a'
$repeat = (Run-Hook -Event SessionStart -Source compact | ConvertFrom-Json).hookSpecificOutput.additionalContext
Check ($repeat.Contains('NEXT-task-a')) 'Repeated binding/recovery retains the task'
foreach ($file in $before) {
    if ((Get-FileHash -LiteralPath $file.Path -Algorithm SHA256).Hash -ne $file.Hash) { throw "Plan was modified by recovery: $($file.Path)" }
}
Check $true 'Recovery never modified any of the six fixture record files'
Write-Output "Verification passed ($($passed.Count) checks). Fixture retained at: $fixture"
Write-Output 'This simulates lifecycle inputs; actual Codex trust/loading and /compact remain a separate user check.'
