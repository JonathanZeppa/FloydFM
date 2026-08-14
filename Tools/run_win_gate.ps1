# FloydFM -- Windows VST3 Phase 5 gate.
#
# Thin wrapper. The protocol lives ONCE in audacious\tools\run_win_gate.ps1 so a change
# to the gate reaches every plugin instead of one copy -- the fleet already lost months to
# exactly that, with only one plugin's script asserting DISTINCT seeds while the rest
# merely counted them.
#
#   .\tools\run_win_gate.ps1 -Label "v1.0.0-gate"
#
# Exits non-zero on any failure.

param(
    [string] $Label  = "gate",
    [string] $Target = "",
    # NOT -Pv: PowerShell reserves that as an alias for -PipelineVariable and the call
    # fails before the gate starts.
    [string] $PluginvalExe = "D:\Zedtronics\tools\pluginval\pluginval.exe"
)

& "D:\Zedtronics\audacious\tools\run_win_gate.ps1" `
    -Plugin "FloydFM" -Label $Label -Target $Target -PluginvalExe $PluginvalExe `
    -RepoDir $PSScriptRoot\..
exit $LASTEXITCODE
