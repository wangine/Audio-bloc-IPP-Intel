param([string]$Endpoint='EntropyAudioCore.Control.v1')
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$expected=[IO.Path]::GetFullPath((Join-Path $root 'runtime/EntropyAudioCore.exe'))
if($Endpoint -notmatch '^EntropyAudioCore\.[A-Za-z0-9_.-]+$'){Write-Host 'Stop refused: invalid local control endpoint. No process stopped.' -ForegroundColor Red;exit 1}
Add-Type -TypeDefinition 'using System;using System.Runtime.InteropServices;using Microsoft.Win32.SafeHandles;public static class MusicCinemaPipePid{[DllImport("kernel32.dll",SetLastError=true)]public static extern bool GetNamedPipeServerProcessId(SafePipeHandle h,out uint pid);}'
$pipe=[IO.Pipes.NamedPipeClientStream]::new('.',$Endpoint,[IO.Pipes.PipeDirection]::InOut)
try {
 $pipe.Connect(2000);[uint32]$pidValue=0
 if(![MusicCinemaPipePid]::GetNamedPipeServerProcessId($pipe.SafePipeHandle,[ref]$pidValue)){throw 'Could not identify the control-pipe owner; shutdown refused'}
 $owner=Get-Process -Id ([int]$pidValue) -ErrorAction Stop
 $ownerPath=[string]$owner.Path
 if(!$ownerPath -or ![string]::Equals([IO.Path]::GetFullPath($ownerPath),$expected,[StringComparison]::OrdinalIgnoreCase)){throw 'Stop refused: this endpoint is not owned by this package. No other Core stopped.'}
 $writer=[IO.BinaryWriter]::new($pipe,[Text.Encoding]::UTF8,$true);$reader=[IO.BinaryReader]::new($pipe,[Text.Encoding]::UTF8,$true)
 $payload=[Text.Encoding]::UTF8.GetBytes('{"action":"shutdown"}');$writer.Write([uint32]1);$writer.Write([uint32](16+$payload.Length));$writer.Write([uint32]2);$writer.Write([uint32]1);$writer.Write($payload);$writer.Flush()
 $version=$reader.ReadUInt32();$length=$reader.ReadUInt32();$command=$reader.ReadUInt32();$request=$reader.ReadUInt32();if($version -ne 1 -or $request -ne 1 -or $length -lt 16 -or $length -gt 1048576){throw 'Invalid shutdown response'}
 $bytes=$reader.ReadBytes($length-16);if($bytes.Length -ne $length-16){throw 'Incomplete shutdown response'};$reply=[Text.Encoding]::UTF8.GetString($bytes)|ConvertFrom-Json;if(!$reply.ok){throw $reply.error};Write-Host 'This package Core accepted safe IPC shutdown.'
}finally{$pipe.Dispose()}
