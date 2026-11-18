# Project-local loading policy. State is a receipt, never authorization or plan content.
# UTF-8 BOM is intentional for Windows PowerShell 5.1 Korean request matching.
function Get-LMPlanningRequest([string]$Prompt) {
    $text = [regex]::Replace($Prompt, '(?s)```.*?```', '')
    $text = [regex]::Replace($text, '(?m)^\s*>.*$', '')
    $text = [regex]::Replace($text, '["“][^"”]*["”]|''[^'']*''|`[^`]*`', '')
    if ($text -match '(?i)설계\s*(?:하지\s*(?:마|말)|하지\s*않|말고|없이)|계획\s*(?:하지\s*마|말고|없이)|(?:do\s+not|don''t|without)\s+(?:design|plan)') { return 'ordinary' }
    if ($text -match '(?i)\$planning-with-files\b|(?:^|\s)/lm-design\b') { return 'design' }
    if ($text -match '(?i)(?:계획|설계|컨텍스트|작업\s*상태).{0,20}(?:다시\s*읽|복구|불러와)|(?:restore|reload|recover|resume)\s+(?:the\s+)?(?:plan|design|context)|(?:^|\s)/lm-recover\b') { return 'recover' }
    if ($text -match '(?i)(?:새|새로운|다른)\s*.{0,20}설계(?:를)?\s*(?:해|하자|부탁|요청|작성|만들)|(?:design|create|plan)\s+(?:a\s+)?(?:new|different)\b') { return 'newdesign' }
    if ($text -match '(?i)계획(?:을)?\s*(?:세워|작성|짜|수립)|(?:이전|기존|같은|동일한)\s*설계.{0,10}(?:이어서|계속)|(?:can|could|would)\s+you\s+(?:design|plan)\b') { return 'design' }
    if ($text -match '(?i)(?:설계|아키텍처|구조)(?:를|을)?\s*(?:해|해줘|해줄|해봐|해보|해보자|해주세요|하자|부탁|요청|작성|정리|검토|짜)|설계.{0,12}(?:계속|검토|수정)|(?:please\s+)?(?:design|plan)\s+(?:a|an|the|this|our|my)\b|(?:create|review|revise)\s+(?:a|an|the|this)?\s*(?:design|architecture|plan)\b') { return 'design' }
    if ($text -match '(?i)^\s*(?:다음\s*(?:단계|설계)|이어서|계속해|계속\s*(?:진행|해)|continue\b|next\s+step\b)') { return 'continue' }
    return 'ordinary'
}

function Get-LMLoadingState([string]$ProjectRoot, [string]$SessionKey) {
    $script:lmStatePath = $null
    $empty = @{version=1; loadedPlan=''; prompted=$false; active=$false; nudged=$false; pendingNew=$false; stamp=''}
    if (-not $SessionKey) { return $empty }
    # Refuse linked ancestors as well as a linked receipt; never follow Saved outside the project.
    $dir = $ProjectRoot
    foreach ($part in @('Saved','LMContext','Loading')) {
        $dir = Join-Path $dir $part
        if (Test-Path -LiteralPath $dir) {
            $item = Get-Item -LiteralPath $dir -Force
            if (-not $item.PSIsContainer -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw 'Invalid loading-state directory.' }
        }
    }
    $script:lmStatePath = Join-Path $dir ($SessionKey + '.json')
    if (-not (Test-Path -LiteralPath $script:lmStatePath)) { return $empty }
    $item = Get-Item -LiteralPath $script:lmStatePath -Force
    if ($item.PSIsContainer -or $item.Length -gt 4096 -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw 'Invalid loading-state receipt.' }
    try { $saved = Get-Content -LiteralPath $script:lmStatePath -Raw -Encoding utf8 | ConvertFrom-Json } catch { return $empty }
    if ($saved.version -ne 1 -or $saved.loadedPlan -isnot [string] -or ($saved.loadedPlan -and $saved.loadedPlan -cnotmatch '^[A-Za-z0-9_][A-Za-z0-9._-]*$')) { return $empty }
    foreach ($flag in @('prompted','active','nudged')) { if ($saved.$flag -isnot [bool]) { return $empty } }
    return @{version=1; loadedPlan=[string]$saved.loadedPlan; prompted=$saved.prompted; active=$saved.active; nudged=$saved.nudged; pendingNew=($saved.pendingNew -eq $true); stamp=[string]$saved.stamp}
}

function Save-LMLoadingState([hashtable]$State) {
    if (-not $script:lmStatePath) { return }
    $dir = Split-Path -Parent $script:lmStatePath
    New-Item -ItemType Directory -Path $dir -Force | Out-Null
    $temporary = Join-Path $dir ([Guid]::NewGuid().ToString('N') + '.tmp')
    try {
        [IO.File]::WriteAllText($temporary, ($State | ConvertTo-Json -Compress), [Text.UTF8Encoding]::new($false))
        Move-Item -LiteralPath $temporary -Destination $script:lmStatePath -Force
    } finally {
        if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary }
    }
}
function Get-LMPlanStamp([string]$ProjectRoot, [string]$PlanId) {
    $dir = $ProjectRoot
    foreach ($part in @('.planning', $PlanId)) {
        $dir = Join-Path $dir $part
        $item = Get-Item -LiteralPath $dir -Force
        if (-not $item.PSIsContainer -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw 'Invalid selected plan directory.' }
    }
    $hashes = foreach ($name in @('task_plan.md','findings.md','progress.md')) {
        $path = Join-Path $dir $name
        if (-not (Test-Path -LiteralPath $path)) { 'missing'; continue }
        $item = Get-Item -LiteralPath $path -Force
        if ($item.PSIsContainer -or $item.Length -gt 4MB -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw 'Invalid selected plan record.' }
        $sha = [Security.Cryptography.SHA256]::Create()
        try { ([BitConverter]::ToString($sha.ComputeHash([IO.File]::ReadAllBytes($path)))).Replace('-','') } finally { $sha.Dispose() }
    }
    return $hashes -join ':'
}