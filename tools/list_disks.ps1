$log = Join-Path $PSScriptRoot 'disk_list.log'
Remove-Item $log -ErrorAction SilentlyContinue

"=== Win32_DiskDrive ===" | Out-File $log
Get-CimInstance -ClassName Win32_DiskDrive | ForEach-Object {
    "Index=$($_.Index) Model=$($_.Model) SizeGB=$([math]::Round($_.Size/1GB,1)) Interface=$($_.InterfaceType) Media=$($_.MediaType)"
} | Out-File $log -Append

"=== Win32_DiskPartition ===" | Out-File $log -Append
Get-CimInstance -ClassName Win32_DiskPartition | ForEach-Object {
    "Disk=$($_.DiskIndex) StartLBA=$([math]::Floor($_.StartingOffset/512)) SizeBytes=$($_.Size) Type=$($_.Type)"
} | Out-File $log -Append

"=== Win32_LogicalDisk (removable) ===" | Out-File $log -Append
Get-CimInstance -ClassName Win32_LogicalDisk -Filter "DriveType=2" | ForEach-Object {
    "Drive=$($_.DeviceID) Label=$($_.VolumeName) SizeGB=$([math]::Round($_.Size/1GB,1))"
} | Out-File $log -Append

"DONE" | Out-File $log -Append
