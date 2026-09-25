# Turns an fxc /Fo blob into the DWORD array form D3D9.cpp stores shaders in.
#
# The constant table fxc emits as a D3DSIO_COMMENT block right after the version
# token is dropped: CreateVertexShader ignores it, and the existing shaders in
# D3D9.cpp are stored without it. /Qstrip_reflect does nothing on a legacy
# target, so it is stripped here.
param(
	[Parameter(Mandatory = $true)][string]$Path,
	[Parameter(Mandatory = $true)][string]$Name
)

$bytes = [System.IO.File]::ReadAllBytes($Path)
# Ahead of the dword check, which an empty file passes: zero is a whole number of dwords.
if ($bytes.Length -eq 0) { throw "'$Path' is empty; expected an fxc /Fo blob" }
if (($bytes.Length % 4) -ne 0) { throw "not a whole number of dwords" }

$tokens = New-Object System.Collections.Generic.List[uint32]
for ($i = 0; $i -lt $bytes.Length; $i += 4) {
	$tokens.Add([System.BitConverter]::ToUInt32($bytes, $i))
}

$out = New-Object System.Collections.Generic.List[uint32]
$out.Add($tokens[0])            # version token

$i = 1
$stripped = 0
while ($i -lt $tokens.Count) {
	$t = $tokens[$i]
	# D3DSIO_COMMENT is 0xFFFE in the low 16 bits, length in dwords in bits 16..30
	if (($t -band 0xFFFF) -eq 0xFFFE) {
		$len = ($t -shr 16) -band 0x7FFF
		$i += 1 + $len
		$stripped += 1 + $len
		continue
	}
	# Past the leading comment block everything is instruction data
	break
}
while ($i -lt $tokens.Count) {
	$out.Add($tokens[$i])
	$i += 1
}

if ($out[$out.Count - 1] -ne 0x0000FFFF) { throw "blob does not end with the end token" }

# Part of the returned string, not a Write-Host call. The documented usage redirects stdout to a
# header, and Write-Host bypasses the pipeline.
$sb = New-Object System.Text.StringBuilder
$sb.AppendLine("// $Name : $($out.Count) dwords ($stripped stripped)") | Out-Null
$sb.AppendLine("extern const DWORD $Name[] = {") | Out-Null
for ($j = 0; $j -lt $out.Count; $j += 8) {
	$chunk = @()
	for ($k = $j; ($k -lt ($j + 8)) -and ($k -lt $out.Count); $k++) {
		$chunk += ("0x{0:X8}" -f $out[$k])
	}
	$sep = if (($j + 8) -lt $out.Count) { "," } else { "" }
	$sb.AppendLine("`t" + ($chunk -join ", ") + $sep) | Out-Null
}
$sb.AppendLine("};") | Out-Null
Write-Output $sb.ToString()
