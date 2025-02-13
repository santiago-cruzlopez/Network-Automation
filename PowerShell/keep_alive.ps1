$app = "terminal64.exe"
$path = "C:\Program Files\FOREX.com CA MT5 Terminal\terminal64.exe"

while ($true) {
    if (-not (Get-Process | Where-Object { $_.ProcessName -eq $app.Replace(".exe", "") })) {
        Start-Process $path
        Write-Output "$(Get-Date) - Application restarted" | Out-File -Append "C:\log\app_monitor.log"
    }
    Start-Sleep -Seconds 5
}