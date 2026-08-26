# OPCG Multirole 1회 교체 스크립트 (서버 PC에서 실행; 관리자 권한 불필요, E:\run 쓰기 권한만 필요)
# 사용법: powershell -ExecutionPolicy Bypass -File deploy-multirole-once.ps1 [-Force]
#  1) GitHub(kkfromkor/Multiroptcg 릴리스)에서 새 multirole.exe/hornet.exe 다운로드
#  2) 방(룸)이 0개일 때까지 대기(-Force 면 즉시)
#  3) 실행 중인 multirole 종료 → .prev 백업 → 교체 → 재시작
# 실패 시 .prev로 되돌리고 시끄럽게 알린다.
param([switch]$Force)
$ErrorActionPreference = "Stop"
try { [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12 } catch {}
$runDir = "E:\run"
$assetBase = "https://raw.githubusercontent.com/kkfromkor/Multiroptcg/master/deploy"
$lobbyUrl = "http://127.0.0.1:7922/"   # LobbyListing 포트(방 수 조회)

function Get-RoomCount {
    try {
        $resp = Invoke-WebRequest -Uri $lobbyUrl -UseBasicParsing -TimeoutSec 10
        $bytes = $resp.Content
        if ($bytes -is [byte[]]) { $text = [Text.Encoding]::UTF8.GetString($bytes) } else { $text = [string]$bytes }
        $json = $text | ConvertFrom-Json
        return @($json.rooms).Count
    } catch { return -1 }
}

Write-Host "== OPCG multirole 1회 교체 =="
if (-not (Test-Path (Join-Path $runDir "multirole.exe"))) { throw "E:\run\multirole.exe 없음 - runDir 확인" }

$tmp = Join-Path $env:TEMP "multirole-deploy"
New-Item -ItemType Directory -Force $tmp | Out-Null
foreach ($name in @("multirole.exe", "hornet.exe", "cacert.pem")) {
    $url = "$assetBase/$name"
    Write-Host "다운로드: $url"
    Invoke-WebRequest -Uri $url -OutFile (Join-Path $tmp $name) -UseBasicParsing -TimeoutSec 300
    $minSize = if ($name -eq "cacert.pem") { 50000 } else { 100000 }
    if ((Get-Item (Join-Path $tmp $name)).Length -lt $minSize) { throw "$name 다운로드 산출물이 비정상적으로 작음" }
}

if (-not $Force) {
    Write-Host "방 0개 대기 중... (-Force 로 건너뜀)"
    while ($true) {
        $n = Get-RoomCount
        Write-Host ("rooms={0}" -f $n)
        if ($n -eq 0) { break }
        Start-Sleep -Seconds 30
    }
}

$proc = Get-Process multirole -ErrorAction SilentlyContinue
if ($proc) { Write-Host "multirole 종료(pid=$($proc.Id))"; Stop-Process -Id $proc.Id -Force; Start-Sleep -Seconds 3 }

foreach ($name in @("multirole.exe", "hornet.exe", "cacert.pem")) {
    $dst = Join-Path $runDir $name
    if (Test-Path $dst) { Copy-Item $dst "$dst.prev" -Force }
    Copy-Item (Join-Path $tmp $name) $dst -Force
}

Write-Host "재시작..."
Start-Process -FilePath (Join-Path $runDir "multirole.exe") -WorkingDirectory $runDir
Start-Sleep -Seconds 8
if (Get-Process multirole -ErrorAction SilentlyContinue) {
    Write-Host "완료: multirole 기동 확인. 문제가 생기면 .prev 파일로 되돌리세요." -ForegroundColor Green
} else {
    Write-Host "!!! 재시작 실패 - .prev로 롤백합니다" -ForegroundColor Red
    foreach ($name in @("multirole.exe", "hornet.exe", "cacert.pem")) {
        $dst = Join-Path $runDir $name
        if (Test-Path "$dst.prev") { Copy-Item "$dst.prev" $dst -Force }
    }
    Start-Process -FilePath (Join-Path $runDir "multirole.exe") -WorkingDirectory $runDir
    throw "새 빌드 기동 실패 - 롤백 후 재기동 시도함. 로그 확인 필요."
}
