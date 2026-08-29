@echo off
:: Run as Administrator once — allows PSP to reach the music server
netsh advfirewall firewall delete rule name="PSP Music Server 8084" >nul 2>&1
netsh advfirewall firewall add rule name="PSP Music Server 8084" dir=in action=allow protocol=TCP localport=8084
echo.
echo Firewall rule added for TCP 8084.
echo Keep the server running:  cd server ^&^& python -m app
echo PSP Setup IP must be this PC LAN IP (e.g. 192.168.0.2) port 8084
pause
