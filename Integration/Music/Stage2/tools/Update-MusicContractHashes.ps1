$ErrorActionPreference='Stop'
$taskRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$taskContract=Join-Path $taskRoot 'contract/MusicStageBusContract.json'
$taskModel=Get-Content -LiteralPath $taskContract -Raw | ConvertFrom-Json
$taskDigest=[Security.Cryptography.SHA256]::Create()
try {
 $taskRows=@('#pragma once','#include <stdint.h>')
 $taskBytes=[IO.File]::ReadAllBytes($taskContract)
 $taskHash=$taskDigest.ComputeHash($taskBytes)
 $taskRows+='inline constexpr uint8_t musicContractHash[32] = {'+($taskHash -join ',')+'};'
 $taskRows+='inline constexpr char musicContractHex[] = "'+([Convert]::ToHexString($taskHash).ToLowerInvariant())+'";'
 foreach($taskLayout in $taskModel.layouts){
  $taskHash=$taskDigest.ComputeHash([Text.Encoding]::UTF8.GetBytes(($taskLayout.slots -join ',')))
  $taskRows+='inline constexpr uint8_t layoutHash'+$taskLayout.slots.Count+'[32] = {'+($taskHash -join ',')+'};'
 }
 [IO.File]::WriteAllText((Join-Path $taskRoot 'source/Shared/StageBusSDK/include/MusicStageBusHashes.h'),($taskRows -join "`n")+"`n",[Text.UTF8Encoding]::new($false))
} finally {$taskDigest.Dispose()}
