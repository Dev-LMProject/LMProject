# LMProject adapter for planning-with-files v3.27.0. No network or trust changes.
# Reads one task binding from Saved; never reads Codex transcripts.
param([switch]$FromParent)

$ErrorActionPreference = 'Stop'
[Console]::InputEncoding = [System.Text.UTF8Encoding]::new($false)
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)
$OutputEncoding = [Console]::OutputEncoding
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$entries = @{
    SessionStart = @('run_sh.py', 'session-start.sh')
    UserPromptSubmit = @('run_sh.py', 'user-prompt-submit.sh')
    PreCompact = @('run_sh.py', 'pre-compact.sh')
    PreToolUse = @('pre_tool_use.py')
    PermissionRequest = @('permission_request.py')
    PostToolUse = @('post_tool_use.py')
    Stop = @('stop.py')
}

function Emit-Context([string]$EventName, [string]$Message) {
    if ($EventName -in @('SessionStart', 'UserPromptSubmit')) {
        @{hookSpecificOutput = @{hookEventName = $EventName; additionalContext = $Message}} |
            ConvertTo-Json -Depth 6 -Compress
    } else {
        @{continue = $true; systemMessage = $Message} | ConvertTo-Json -Compress
    }
}

function Find-Python {
    if ($env:PYTHON_BIN) {
        if (Test-Path -LiteralPath $env:PYTHON_BIN -PathType Leaf) { return $env:PYTHON_BIN }
        throw 'PYTHON_BIN does not point to an existing Python executable.'
    }
    $command = Get-Command python.exe -ErrorAction SilentlyContinue
    if ($command -and $command.Source -notmatch '[\\/]WindowsApps[\\/]') { return $command.Source }
    $launcher = Get-Command py.exe -ErrorAction SilentlyContinue
    if ($launcher) {
        $candidate = & $launcher.Source -3 -c 'import sys; print(sys.executable)' 2>$null
        if ($LASTEXITCODE -eq 0 -and $candidate -and (Test-Path -LiteralPath $candidate -PathType Leaf)) {
            return [string]$candidate
        }
    }
    # Codex already bundles this runtime on this PC. No personal absolute path is shared.
    $bundled = Join-Path $env:USERPROFILE '.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe'
    if (Test-Path -LiteralPath $bundled -PathType Leaf) { return $bundled }
    throw 'Python 3 is unavailable. Install Python or set PYTHON_BIN before starting Codex.'
}

$eventName = 'SessionStart'
try {
    if ($env:PLANNING_DISABLED -eq '1') { exit 0 }
    $raw = [Console]::In.ReadToEnd()
    if ([string]::IsNullOrWhiteSpace($raw)) { exit 0 }
    $payload = $raw | ConvertFrom-Json
    $eventName = [string]$payload.hook_event_name
    if (-not $entries.ContainsKey($eventName)) { exit 0 }
    $cwd = [IO.Path]::GetFullPath([string]$payload.cwd)
    if ($FromParent) {
        # A parent-root registration must be silent in Dev: both config layers may load.
        if ($cwd.TrimEnd('\', '/') -ine (Split-Path -Parent $root).TrimEnd('\', '/')) { exit 0 }
    } elseif ($cwd -ine $root -and -not $cwd.StartsWith($root + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Hook cwd is outside this project.'
    }

    $sessionId = [string]$payload.session_id
    $bindingRelative = $null
    if ($sessionId) {
        $sha = [Security.Cryptography.SHA256]::Create()
        try { $key = ([BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($sessionId)))).Replace('-', '').ToLowerInvariant() }
        finally { $sha.Dispose() }
        $bindingRelative = 'Saved/LMContext/Sessions/' + $key + '.plan'
        $bindingPath = Join-Path $root $bindingRelative
        if (-not $env:PLAN_ID -and (Test-Path -LiteralPath $bindingPath -PathType Leaf)) {
            $bindingItem = Get-Item -LiteralPath $bindingPath
            if ($bindingItem.Length -gt 256 -or ($bindingItem.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
                throw 'Invalid local plan binding file.'
            }
            $env:PLAN_ID = (Get-Content -LiteralPath $bindingPath -Raw -Encoding utf8).Trim()
            if (-not $env:PLAN_ID) { throw 'Local plan binding is empty; choose this task explicitly.' }
        }
    }
    if ($env:PLAN_ID -and $env:PLAN_ID -cnotmatch '^[A-Za-z0-9_][A-Za-z0-9._-]*$') {
        throw 'PLAN_ID must be one plain task-folder name, with no path separators.'
    }
    if (-not $env:PLAN_ID) {
        if ($eventName -in @('SessionStart', 'UserPromptSubmit')) {
            $hint = if ($bindingRelative) { "Bind this conversation by writing ONLY its chosen PLAN_ID to $bindingRelative (relative to the project root)." } else { 'Set PLAN_ID in the environment before starting the Codex host.' }
            Emit-Context $eventName "[LMContext] No task is bound; no plan has been selected or injected. Read Docs/LMContext/README.ko.md and .agents/skills/planning-with-files/SKILL.md. Reuse the task explicitly requested by the user, or initialize a new named plan. $hint Do not choose another person's plan. Then read that task's task_plan.md, findings.md and progress.md. Project root: $root"
        }
        exit 0
    }
    $planFile = Join-Path $root ('.planning/' + $env:PLAN_ID + '/task_plan.md')
    if (-not (Test-Path -LiteralPath $planFile -PathType Leaf)) { throw 'The selected PLAN_ID has no task_plan.md. No fallback task was selected.' }

    $python = Find-Python
    $env:PYTHON_BIN = $python
    $env:PYTHONUTF8 = '1'
    $env:PYTHONDONTWRITEBYTECODE = '1'
    $env:PWF_PLAN_ROOT = $root
    $payload.cwd = $root
    $entry = $entries[$eventName]
    $pythonArgs = @('-B', '-X', 'utf8', (Join-Path $PSScriptRoot $entry[0]))
    if ($entry.Count -gt 1) { $pythonArgs += $entry[1] }
    Push-Location -LiteralPath $root
    try {
        $output = ($payload | ConvertTo-Json -Depth 50 -Compress) | & $python @pythonArgs
        if ($LASTEXITCODE -ne 0) { throw 'The planning hook runtime returned a nonzero exit code.' }
    } finally { Pop-Location }
    if ($eventName -in @('SessionStart', 'UserPromptSubmit')) {
        if (-not $output) { throw 'The planning hook returned no context. Check Git for Windows and task/session selection.' }
        $result = ($output -join "`n") | ConvertFrom-Json
        if (-not $result.hookSpecificOutput.additionalContext) { throw 'The planning hook returned no additionalContext.' }
        $result.hookSpecificOutput.additionalContext += "`n[LMContext] Selected task: .planning/$($env:PLAN_ID)/. Read all three files before continuing; injected excerpts are partial. Update next action and verification evidence during work, not only before compact."
        if ($bindingRelative) { $result.hookSpecificOutput.additionalContext += "`nLocal conversation binding: $bindingRelative (never share)." }
        $result | ConvertTo-Json -Depth 10 -Compress
    } elseif ($output) { $output }
} catch {
    Emit-Context $eventName ('[LMContext] Recovery unavailable: ' + $_.Exception.Message + ' Read Docs/LMContext/README.ko.md; do not infer that recovery succeeded.')
}
exit 0
