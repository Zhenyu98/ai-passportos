param([Parameter(Mandatory=$true)][string]$InputFile,
      [Parameter(Mandatory=$true)][string]$OutputFile,
      [string]$Voice)
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Speech
$passportSynth=New-Object System.Speech.Synthesis.SpeechSynthesizer
try {
    if($Voice) {$passportSynth.SelectVoice($Voice)}
    else {
        $passportChinese=@($passportSynth.GetInstalledVoices() | Where-Object { $_.Enabled -and $_.VoiceInfo.Culture.Name -like 'zh-*' })
        if(-not $passportChinese.Count) {throw 'Install a Chinese Windows speech voice first'}
        $passportSynth.SelectVoice($passportChinese[0].VoiceInfo.Name)
    }
    $passportFormat=New-Object System.Speech.AudioFormat.SpeechAudioFormatInfo(
        16000,[System.Speech.AudioFormat.AudioBitsPerSample]::Sixteen,
        [System.Speech.AudioFormat.AudioChannel]::Mono)
    $passportSynth.SetOutputToWaveFile($OutputFile,$passportFormat)
    $passportText=[System.IO.File]::ReadAllText($InputFile,[System.Text.Encoding]::UTF8)
    $passportSynth.Speak($passportText)
    $passportSynth.SetOutputToNull()
} finally {$passportSynth.Dispose()}
