#!/bin/bash
# Readout regression check. Run on the Pi from the build directory after every change
# to the capture path, PIO setup, piolib or firmware:
#   linearcam_pi/tests/regress.sh [report file]
# Compares against regress_ref.txt and prints PASS/FAIL per check. Exit code 1 on any FAIL.
# The sensor must look at a static scene (the check does not move the slide): every line is
# compared with the median line to find lines shifted by whole pixel clocks (TG timing).
cd "$(dirname "$0")/../.." || exit 2
B=./linearcam_pi/linearcam_pi
T=linearcam_pi/tests
OUT=${1:-/dev/stdout}
LINES=${LINES:-100}
SKIP=8 # Start-up lines: 0-3 blank or saturated, 4-5 partially exposed
TMP=$(mktemp)

versions(){
    echo "# $(date -Iseconds)"
    echo "# firmware_revision $(cat /boot/firmware/.firmware_revision 2>/dev/null)"
    echo "# kernel $(uname -r) $(uname -v)"
    echo "# bootloader $(vcgencmd bootloader_version 2>/dev/null | sed -n 2p)"
    echo "# rp1_firmware $(dmesg 2>/dev/null | sed -n 's/.*RP1 Firmware version //p')"
    echo "# libpio_md5 $(md5sum /usr/local/lib/libpio.a | cut -c1-32)"
    echo "# throttled $(vcgencmd get_throttled 2>/dev/null)"
}

run(){
versions
if [ -n "$(pgrep -x LinearScanCam)" ]; then
    echo "ABORT LinearScanCam is running and holds the PIO, stop it first (pkill -x LinearScanCam)"
    return
fi

# Raw readout at every resolution: markers, byte lane alignment per line, readout time
while read -r res words lag ref; do
    case "$res" in ''|\#*) continue;; esac
    r=$($B pio -r "$res" -l "$LINES" -o /tmp/regress.bin 2>&1)
    clk=$(echo "$r" | sed -n 's/^CCD clock \([0-9e.+]*\) Hz.*/\1/p')
    rd=$(echo "$r" | sed -n 's/^Readout.*mean *\([0-9.]*\).*/\1/p')
    bl=$(python3 $T/badlines.py /tmp/regress.bin "$words" "$lag" $SKIP)
    ls=$(python3 $T/lineshift.py /tmp/regress.bin "$words" $SKIP)
    nsh=$(echo "$ls" | sed -n 's/.*shifted \([0-9]*\).*/\1/p')
    nbad=$(echo "$bl" | sed -n 's/.*bad lines \([0-9]*\).*/\1/p')
    nmk=$(echo "$bl" | sed -n 's/.*markers bad \([0-9]*\).*/\1/p')
    dev=$(awk -v a="$rd" -v b="$ref" 'BEGIN{printf "%.1f", (a-b)/b*100}')
    ok=$(awk -v d="$dev" 'BEGIN{print (d < 2 && d > -2) ? 1 : 0}')
    if [ "$nbad" = 0 ] && [ "$nmk" = 0 ] && [ "$nsh" = 0 ] && [ "$ok" = 1 ]; then st=PASS; else st=FAIL; fi
    echo "$st res $res clk $clk readout ${rd}us (ref ${ref}us, ${dev}%) | $bl | shifted lines $nsh"
done < $T/regress_ref.txt
rm -f /tmp/regress.bin
}

run > "$TMP"
grep -q '^FAIL\|^ABORT' "$TMP" && echo "RESULT FAIL" >> "$TMP" || echo "RESULT PASS" >> "$TMP"
cat "$TMP" | tee -a "$OUT" > /dev/null
[ "$OUT" != /dev/stdout ] && cat "$TMP"
grep -q '^RESULT PASS' "$TMP"; st=$?
rm -f "$TMP"
exit $st
