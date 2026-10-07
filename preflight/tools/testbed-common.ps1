# Shared helpers for the Pre-Flight harness scripts (dot-sourced). ASCII only.

# Writes a short silent 16-bit mono 48 kHz PCM WAV. Used as a hardware-free audio source for the
# fixture mic: an ffmpeg_source always loads and has the audio flag, unlike a wasapi capture of a
# device that does not exist.
function New-SilentWav([string]$Path, [double]$Seconds = 1.0) {
  $rate = 48000
  $dataLen = [int]($rate * $Seconds) * 2
  $ms = New-Object IO.MemoryStream
  $bw = New-Object IO.BinaryWriter($ms)
  $bw.Write([Text.Encoding]::ASCII.GetBytes("RIFF"))
  $bw.Write([int](36 + $dataLen))
  $bw.Write([Text.Encoding]::ASCII.GetBytes("WAVE"))
  $bw.Write([Text.Encoding]::ASCII.GetBytes("fmt "))
  $bw.Write([int]16)
  $bw.Write([int16]1)
  $bw.Write([int16]1)
  $bw.Write([int]$rate)
  $bw.Write([int]($rate * 2))
  $bw.Write([int16]2)
  $bw.Write([int16]16)
  $bw.Write([Text.Encoding]::ASCII.GetBytes("data"))
  $bw.Write([int]$dataLen)
  $bw.Write((New-Object byte[] $dataLen))
  $bw.Flush()
  New-Item -ItemType Directory -Force (Split-Path -Parent $Path) | Out-Null
  [IO.File]::WriteAllBytes($Path, $ms.ToArray())
}
