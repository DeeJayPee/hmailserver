<#
Grant modify permissions for the hMailServer installation folder.

This script will relaunch itself elevated if not already running as Administrator.
It grants 'Everyone' Modify permissions recursively to:
  C:\Program Files (x86)\hMailServer

Use this before running the test suite if your tests need write access to the
installation directory. Run with care — granting 'Everyone' write access is
insecure on multi-user machines. Prefer running tests in a dedicated test VM.
#>

$target = 'C:\Program Files (x86)\hMailServer'

function Test-IsElevated {
    $current = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($current)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

if (-not (Test-IsElevated)) {
    Write-Host "Not running elevated — relaunching as Administrator..."
    try {
        Start-Process -FilePath 'powershell.exe' -ArgumentList '-NoProfile','-ExecutionPolicy','Bypass','-File',$PSCommandPath -Verb RunAs -WindowStyle Normal
    } catch {
        Write-Error "Failed to elevate. Aborting."
    }
    exit
}

if (-not (Test-Path -Path $target)) {
    Write-Error "Target path not found: $target"
    exit 1
}

Write-Host "Applying permissions to: $target"

# Use icacls for robust recursive ACL changes. Modify (M) is granted; change to F for Full Control if desired.
$cmd = "icacls `"$target`" /grant Everyone:(OI)(CI)M /T"
Write-Host $cmd

$proc = Start-Process -FilePath 'icacls' -ArgumentList @("`"$target`"", "/grant", "Everyone:(OI)(CI)M", "/T") -NoNewWindow -Wait -PassThru

if ($proc.ExitCode -eq 0) {
    Write-Host "Permissions applied successfully."
    exit 0
} else {
    Write-Error "icacls exited with code $($proc.ExitCode). You may need to run this script in an elevated session manually."
    exit $proc.ExitCode
}
