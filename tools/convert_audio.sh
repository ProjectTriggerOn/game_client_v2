#!/usr/bin/env bash
# Batch-convert the Low Poly Shooter Pack sources into the mono 16-bit 44.1kHz
# WAVs the audio system expects.  Point sources MUST be mono: a stereo source
# fed to a 3D spatializer loses its direction.  Ambient loops stay stereo
# because they are never spatialized.
#
# Usage (from the repo root):  bash tools/convert_audio.sh
#   AUDIO_SRC=/path/to/"Low Poly Shooter Pack"/Audio bash tools/convert_audio.sh
#
# The default SRC is the author's local checkout of the pack, which is NOT in
# this repository (it is a licensed Unity Asset Store download).  Override it
# with AUDIO_SRC on any other machine.  The generated WAVs are committed, so
# this script only needs to run when the sound set itself changes.
set -euo pipefail

SRC="${AUDIO_SRC:-/d/WORKPLACE/TriggerOn/Low Poly Shooter Pack/Audio}"
DST="resource/audio"

# Fail once, with a sentence, instead of 26 times with ffmpeg's exit status.
if ! command -v ffmpeg >/dev/null 2>&1; then
    echo "error: ffmpeg is not on PATH. Install it (https://ffmpeg.org/) and retry." >&2
    exit 1
fi
if [ ! -d "$SRC" ]; then
    echo "error: source directory not found: $SRC" >&2
    echo "       Set AUDIO_SRC to the Low Poly Shooter Pack's Audio/ folder." >&2
    exit 1
fi

mono() {  # mono <src-relative> <dst-relative>
    mkdir -p "$(dirname "$DST/$2")"
    ffmpeg -y -loglevel error -i "$SRC/$1" -ac 1 -ar 44100 -sample_fmt s16 "$DST/$2"
    echo "  mono   $2"
}
stereo() {
    mkdir -p "$(dirname "$DST/$2")"
    ffmpeg -y -loglevel error -i "$SRC/$1" -ac 2 -ar 44100 -sample_fmt s16 "$DST/$2"
    echo "  stereo $2"
}

echo "weapons:"
for i in 001 002 003 004 005; do
    mono "SFX/Weapons/Fire/S_WEP_Fire_$i.wav" "weapons/fire_$i.wav"
done
mono "SFX/Weapons/Fire/S_WEP_Fire_Empty.wav"        "weapons/fire_empty.wav"
mono "SFX/Weapons/Reloads/S_WEP_AR_01_Reload.wav"       "weapons/reload.wav"
mono "SFX/Weapons/Reloads/S_WEP_AR_01_Reload_Empty.wav" "weapons/reload_empty.wav"
mono "SFX/Weapons/Aiming/S_WEP_Aim_In.wav"          "weapons/ads_in.wav"
mono "SFX/Weapons/Swishes/S_WEP_Swish_02.wav"       "weapons/ads_out.wav"
# No take-out: S_WEP_Unholster.wav turned out to be the weapon-against-wall
# lower/release foley, not a draw (see the design doc's section 14). It was
# picked by filename alone, without listening, hence the wrong take.

echo "character:"
for i in 001 002 003 004 005; do
    mono "SFX/Character/Movement/S_CH_Footstep_$i.wav" "character/footstep_$i.wav"
done
mono "SFX/Character/Jumping/S_CH_Jump_Start.wav" "character/jump_start.wav"
mono "SFX/Character/Jumping/S_CH_Jump_Land.wav"  "character/jump_land.wav"
mono "SFX/Impacts/S_Thud.wav"                    "character/death.wav"
for i in 01 02 03 04; do
    mono "SFX/Impacts/S_WEP_Impact_Bullet_$i.wav" "character/take_damage_$i.wav"
done

echo "ui:"
# No hitmarker: there is no hit-feedback feature to trigger one (see the design
# doc's section 14), and a sound nothing can play is dead weight in the repo.
mono "SFX/Target/S_TargetGoDown.wav"                   "ui/kill_confirm.wav"
mono "SFX/Weapons/Attachments/S_WEP_Flashlight_Click.wav" "ui/click.wav"

echo "ambient:"
stereo "Ambient/S_Demo_Warzone_Ambient_Loop.wav" "ambient/warzone_loop.wav"

echo "done."
