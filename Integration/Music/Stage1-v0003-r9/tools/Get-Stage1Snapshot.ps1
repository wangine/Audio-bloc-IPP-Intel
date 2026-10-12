param([string]$ControlEndpoint, [switch]$Catalog, [int]$CatalogOffset = 0)
$ErrorActionPreference = 'Stop'
if (!$ControlEndpoint) {
    $sid = [Security.Principal.WindowsIdentity]::GetCurrent().User.Value
    $ControlEndpoint = '\\.\pipe\Entropy.StagePublish.Music.Source.V2.' + $sid + '.Control.V1'
}
$request = New-Object byte[] 680
[BitConverter]::GetBytes([uint64]0x3143544D31535445).CopyTo($request, 0)
[BitConverter]::GetBytes([uint32]0x10000).CopyTo($request, 8)
[BitConverter]::GetBytes([uint32]680).CopyTo($request, 12)
[BitConverter]::GetBytes([uint32][bool]$Catalog).CopyTo($request, 16)
[BitConverter]::GetBytes([uint32]$CatalogOffset).CopyTo($request, 28)
$pipe = New-Object IO.Pipes.NamedPipeClientStream('.', $ControlEndpoint.Substring(9), [IO.Pipes.PipeDirection]::InOut, [IO.Pipes.PipeOptions]::Asynchronous)
function Read-Exact([byte[]]$bytes, [int]$offset, [int]$length) {
    while ($length -gt 0) {
        $read = $pipe.ReadAsync($bytes, $offset, $length)
        if (!$read.Wait(3000)) { throw 'Control read timed out' }
        if (!$read.Result) { throw 'Core closed control pipe before full response' }
        $offset += $read.Result; $length -= $read.Result
    }
}
try {
    $pipe.Connect(3000)
    # The server sends one fixed-size response and disconnects; read that whole ABI
    # frame at once, as the native client does, rather than first reading a header.
    $bytes = New-Object byte[] 49232
    # Arm the read before sending: this diagnostic client is slower than C++,
    # and the server disconnects immediately after completing its single write.
    $firstRead = $pipe.ReadAsync($bytes, 0, $bytes.Length)
    $pipe.Write($request, 0, $request.Length)
    if (!$firstRead.Wait(3000) -or !$firstRead.Result) { throw 'Control response was not received' }
    Read-Exact $bytes $firstRead.Result ($bytes.Length-$firstRead.Result)
    if ([BitConverter]::ToUInt64($bytes,0) -ne [uint64]0x3143544D31535445 -or [BitConverter]::ToUInt32($bytes,8) -ne 0x10000 -or [BitConverter]::ToUInt32($bytes,12) -ne 49232) { throw 'Unexpected control response ABI' }
} finally { $pipe.Dispose() }
function U32([int]$offset) { [BitConverter]::ToUInt32($bytes,$offset) }
function U64([int]$offset) { [BitConverter]::ToUInt64($bytes,$offset) }
function F64([int]$offset) { [BitConverter]::ToDouble($bytes,$offset) }
function Text([int]$offset,[int]$length) { [Text.Encoding]::UTF8.GetString($bytes,$offset,$length).Split([char]0)[0] }
$sources = @()
for ($i=0; $i -lt 2; $i++) {
    $base=368+880*$i
    $sources += [ordered]@{
        role=@('MUSIC','AUX')[$i]; providerId=(Text $base 96); endpointId=(Text ($base+96) 512)
        state=(U32 ($base+608)); representation=(U32 ($base+612)); mode=(U32 ($base+616)); rate=(U32 ($base+620)); containerBits=(U32 ($base+624)); validBits=(U32 ($base+628)); fillFrames=(U32 ($base+632)); metadataFlags=(U32 ($base+636))
        generation=(U64 ($base+640)); packets=(U64 ($base+648)); framesIn=(U64 ($base+656)); framesOut=(U64 ($base+664)); lostFrames=(U64 ($base+672)); overruns=(U64 ($base+680)); underruns=(U64 ($base+688)); faults=(U64 ($base+696)); qpcFrequency=(U64 ($base+728))
        samplePeakL=(F64 ($base+736)); samplePeakR=(F64 ($base+744)); inputRmsL=(F64 ($base+752)); inputRmsR=(F64 ($base+760)); inputMidRms=(F64 ($base+768)); inputSideRms=(F64 ($base+776)); outputPeakL=(F64 ($base+784)); outputPeakR=(F64 ($base+792)); outputRmsL=(F64 ($base+800)); outputRmsR=(F64 ($base+808)); outputMidRms=(F64 ($base+816)); outputSideRms=(F64 ($base+824)); truePeakL=(F64 ($base+832)); truePeakR=(F64 ($base+840)); startupPeakL=(F64 ($base+848)); startupPeakR=(F64 ($base+856)); startupFrames=(U64 ($base+864)); truePeakReady=(U32 ($base+872)); startupComplete=(U32 ($base+876))
    }
}
$endpoints=@()
if ($Catalog) {
    for($i=0;$i -lt (U32 28);$i++) {
        $base=2128+1472*$i
        $endpoints += [ordered]@{providerId=(Text $base 96); roles=(U32 ($base+104)); eligible=(U32 ($base+108)); accessModes=(U32 ($base+120)); endpointId=(Text ($base+128) 512); name=(Text ($base+640) 256); windowsEndpointId=(Text ($base+896) 512)}
    }
}
[ordered]@{status=(U32 24); message=(Text 112 256); blocksPublished=(U64 40); publisherDrops=(U64 48); controlRevision=(U64 56); clockQpcFrequency=(U64 72); clockLateBlocks=(U64 80); clockRunning=(U32 88); outputRate=(U32 92); sessionId=([BitConverter]::ToString($bytes,96,16).Replace('-','')); sources=$sources; catalogTotal=(U32 36); catalogTruncated=(U32 32); endpoints=$endpoints} | ConvertTo-Json -Depth 6
