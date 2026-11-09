#!/bin/bash
# JE-8086 (JP-8000 emulator) - speed test for older Intel Macs (OS X 10.9 Mavericks and newer).
# Double-click this file (the first time: right-click > Open). The JP-8000/JP-8080 ROM (a .bin dump or
# the eight Roland update .mid files) must be in the same folder as this file.

cd "$(dirname "$0")"
clear
echo "=============================================="
echo "  JE-8086 - snelheidstest"
echo "=============================================="
echo

# any .bin or .mid file, in any letter case (ls with several patterns fails as soon as one of them matches
# nothing, which is why the first version reported a missing ROM next to eight .mid files)
if ! ls -1 | grep -iqE '\.(bin|mid)$'; then
    echo "Geen ROM gevonden (.bin of .mid-bestanden van de JP-8000/JP-8080)."
    echo "Zet ze in deze map:"
    echo "  $(pwd)"
    echo
    read -n 1 -s -r -p "Druk op een toets om te sluiten."
    exit 1
fi

xattr -dr com.apple.quarantine ./JE8086TestConsole 2>/dev/null
chmod +x ./JE8086TestConsole

echo "Processor: $(sysctl -n machdep.cpu.brand_string)"
echo "macOS:     $(sw_vers -productVersion)"
echo
echo "De emulator start op en speelt 30 seconden van de demosong."
echo "Dit duurt hooguit een paar minuten - even geduld..."
echo

./JE8086TestConsole --demo-seconds 30 --wav test-geluid.wav > resultaat.txt 2>&1
STATUS=$?

LINE=$(grep "% of real time" resultaat.txt | tail -1)
[ -z "$LINE" ] && LINE=$(grep "total average" resultaat.txt | tail -1)
if [ -z "$LINE" ]; then
    echo "De test is niet gelukt (code $STATUS). Stuur het bestand resultaat.txt naar Claude."
    echo
    tail -20 resultaat.txt
    echo
    read -n 1 -s -r -p "Druk op een toets om te sluiten."
    exit 1
fi

PCT=$(echo "$LINE" | sed -E 's/.* ([0-9]+)% of real time.*/\1/; s/.*total average ([0-9]+)%.*/\1/')

echo "----------------------------------------------"
echo "  $LINE"
echo "  Snelheid: ${PCT}% van realtime"
echo "----------------------------------------------"
echo
awk -v p="$PCT" 'BEGIN {
    if (p >= 130)      print "GOED: je Mac is snel genoeg. Een plugin-versie heeft kans van slagen.";
    else if (p >= 100) print "KRAP: net snel genoeg. In Cubase gaat het waarschijnlijk haperen.";
    else               print "TE TRAAG: je Mac kan de emulator niet bijhouden.";
}'
echo
echo "(Volledige uitvoer staat in resultaat.txt, het testgeluid in test-geluid.wav)"
echo
read -n 1 -s -r -p "Druk op een toets om te sluiten."
