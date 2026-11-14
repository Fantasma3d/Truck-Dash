# Instala TruckDash.dll em <jogo>\bin\win_x64\plugins (procura nas bibliotecas da Steam)
$ErrorActionPreference = 'SilentlyContinue'
$dll = Join-Path $PSScriptRoot 'TruckDash.dll'
$games = @('Euro Truck Simulator 2', 'American Truck Simulator')
$roots = @()
$steam = (Get-ItemProperty 'HKCU:\Software\Valve\Steam').SteamPath
if ($steam) {
    $roots += (Join-Path $steam 'steamapps\common')
    $vdf = Join-Path $steam 'steamapps\libraryfolders.vdf'
    if (Test-Path $vdf) {
        Select-String -Path $vdf -Pattern '"path"\s+"([^"]+)"' | ForEach-Object {
            $p = $_.Matches[0].Groups[1].Value -replace '\\\\', '\'
            $roots += (Join-Path $p 'steamapps\common')
        }
    }
}
$found = 0
foreach ($r in ($roots | Select-Object -Unique)) {
    foreach ($g in $games) {
        $bin = Join-Path $r "$g\bin\win_x64"
        if (Test-Path $bin) {
            $plug = Join-Path $bin 'plugins'
            New-Item -ItemType Directory -Force -Path $plug | Out-Null
            Copy-Item $dll $plug -Force
            $txt = Join-Path $PSScriptRoot 'TruckDash.txt'
            if (Test-Path $txt) { Copy-Item $txt $plug -Force }
            Write-Host "OK: plugin instalado em $plug" -ForegroundColor Green
            $found++
        }
    }
}
if ($found -eq 0) {
    Write-Host 'Nao encontrei o jogo automaticamente.' -ForegroundColor Yellow
    Write-Host 'Copie TruckDash.dll para: <pasta do jogo>\bin\win_x64\plugins (crie a pasta plugins se nao existir)' -ForegroundColor Yellow
}
$admin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if ($admin) {
    netsh advfirewall firewall delete rule name="TruckDash" | Out-Null
    netsh advfirewall firewall add rule name="TruckDash" dir=in action=allow protocol=UDP localport=45454 profile=any | Out-Null
    Write-Host 'OK: porta UDP 45454 liberada no firewall do Windows' -ForegroundColor Green
} else {
    Write-Host 'Opcional: clique com o botao direito > Executar como administrador para liberar a porta UDP 45454 no firewall.'
}
