PLUGIN VST3 — INSTALLAZIONE
===========================

Ogni cartella .vst3 funziona su macOS, Windows e Linux: copiala intera
nella cartella dei plugin VST3 del tuo sistema, poi fai una nuova scansione
dei plugin nella DAW (in Reaper: Preferences > Plug-ins > VST > Re-scan).

macOS
    /Library/Audio/Plug-Ins/VST3/
    (oppure ~/Library/Audio/Plug-Ins/VST3/ solo per il tuo utente)

    I plugin non sono firmati da Apple: macOS può bloccarli perché scaricati
    da internet. Se la DAW non li carica, apri il Terminale ed esegui, per ogni plugin:

        xattr -dr com.apple.quarantine /Library/Audio/Plug-Ins/VST3/<nome>.vst3

Windows
    C:\Program Files\Common Files\VST3\

Linux
    ~/.vst3/

Le novità di questa versione sono in NOTES.md.
