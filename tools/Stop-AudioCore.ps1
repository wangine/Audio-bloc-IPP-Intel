param([string]$Endpoint='EntropyAudioCore.Control.v1')
$ErrorActionPreference='Stop'
$pipe=$null
try {
    if($Endpoint -notmatch '^EntropyAudioCore\.[A-Za-z0-9_.-]+$'){throw 'Invalid IPC endpoint'}
    $pipe=[IO.Pipes.NamedPipeClientStream]::new('.', $Endpoint, [IO.Pipes.PipeDirection]::InOut)
    $pipe.Connect(2000)
    $writer=[IO.BinaryWriter]::new($pipe,[Text.Encoding]::UTF8,$true)
    $reader=[IO.BinaryReader]::new($pipe,[Text.Encoding]::UTF8,$true)
    $payload=[Text.Encoding]::UTF8.GetBytes('{"action":"shutdown"}')
    $writer.Write([uint32]1);$writer.Write([uint32](16+$payload.Length));$writer.Write([uint32]2);$writer.Write([uint32]1);$writer.Write($payload);$writer.Flush()
    $version=$reader.ReadUInt32();$length=$reader.ReadUInt32();$command=$reader.ReadUInt32();$request=$reader.ReadUInt32()
    if($version -ne 1 -or $request -ne 1 -or $length -lt 16 -or $length -gt 1048576){throw 'Invalid Core reply'}
    $bytes=$reader.ReadBytes($length-16)
    if($bytes.Length -ne $length-16){throw 'Incomplete Core reply'}
    $reply=[Text.Encoding]::UTF8.GetString($bytes) | ConvertFrom-Json
    if(!$reply.ok){throw $reply.error}
    Write-Host 'AudioCore accepted safe shutdown; Stage2 ownership will be released. No DAC opened.'
}catch{Write-Host ('Cannot stop Core: '+$_.Exception.Message+'. Close Qt first, then retry.') -ForegroundColor Red;exit 1}
finally{if($pipe){$pipe.Dispose()}}
