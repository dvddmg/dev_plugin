# Develop plugin with HVCC e DPF in PureData

Ambiente di lavoro per creare plugin audio partendo da patch Pure Data, grazie al compilatore `hvcc` e al framework `DPF`. Da un'unica patch si ottiene un plugin VST3 per macOS, Windows e Linux, con un'interfaccia ImGui personalizzabile.

Link utili per studio:

- https://github.com/Wasted-Audio/hvcc/tree/develop
- https://github.com/Wasted-Audio/heavylib.git
- https://github.com/distrho/dpf
- https://github.com/DISTRHO/DPF-Widgets.git

Sono anche i submodule del progetto: `dpf` e `dpf-widgets` sono le dipendenze usate per compilare i plugin.

## Struttura

```
dev_plugin/
├── new.sh               crea un nuovo plugin in src/ dal template
├── build.sh             genera (hvcc) e compila un plugin, per una o più piattaforme
├── config.sh            impostazioni comuni: produttore, brand_id, cartelle di installazione
├── common/ui/           base comune delle interfacce (PluginUIBase.hpp, Palette.hpp)
├── templates/plugin/    template usato da new.sh (patch, plugin.json, UI)
├── docker/linux/        immagine Docker per compilare la versione Linux
├── dep/                 submodule: dpf, dpf-widgets, hvcc
└── src/<plugin>/
    ├── <plugin>.pd      la patch (il DSP)
    ├── plugin.json      metadati per hvcc/DPF
    └── ui/HeavyDPF_<plugin>_UI.cpp   l'interfaccia del plugin
```

Tutto il resto dentro `src/<plugin>/` (`plugin/`, `build*/`, `bin/`, `c/`, `hv/`, `ir/`) è generato dal build e non va nel repository.

## Preparazione

Clona con i submodule (anche quelli interni di DPF):

```bash
git clone --recurse-submodules <url-del-repo>
```

hvcc è una libreria Python: crea l'ambiente e installa le dipendenze.

```bash
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt
```

`build.sh` attiva il venv da solo se hvcc non è già disponibile.

Controlla i dati del produttore in `config.sh` (`MAKER`, `BRAND_ID`, `HOMEPAGE`…) e le cartelle di installazione dei VST3.

## Nuovo plugin

```bash
./new.sh
```

Chiede nome, descrizione, `unique_id` (4 caratteri, diverso per ogni plugin) e formati. Crea `src/<nome>/` con:

- una patch stereo di partenza con un parametro `gain`;
- `plugin.json` già valido, con i dati di `config.sh`;
- un'interfaccia che mostra automaticamente un controllo per ogni parametro della patch.

I parametri si definiscono nella patch con `[r nome @hv_param min max default]` (tipi opzionali: `bool`, `int`, `log`, `dB`…). Per i menu a scelta si aggiunge `"enumerators"` in `plugin.json`.

## Build

```bash
./build.sh <plugin> [opzioni]
```

| Opzione | Cosa fa |
|---|---|
| *(nessuna)* o `--native` | compila per il sistema su cui stai lavorando |
| `--universal` | macOS universal: Apple Silicon + Intel (solo da Mac) |
| `--win` | Windows 64 bit: da Mac con MinGW, nativo su Windows |
| `--linux` | Linux 64 bit: da Mac con Docker, nativo su Linux |
| `--all` | `--universal` + `--win` + `--linux`: un solo bundle per i tre sistemi |
| `--install` | copia il bundle nella cartella VST3 indicata in `config.sh` |
| `--clean` | cancella `build*` e `bin` del plugin prima di compilare |

Esempi:

```bash
./build.sh orbita --install          # sviluppo: solo Mac, e installa
./build.sh orbita --all --install    # distribuzione: bundle per Mac, Windows e Linux
```

Cosa fa, in ordine:

1. rigenera i sorgenti con hvcc (cancellando quelli vecchi);
2. crea `HeavyParams.hpp`: nomi, indici, default e intervalli dei parametri per la UI;
3. copia `common/ui/` e `src/<plugin>/ui/` nei sorgenti;
4. compila per le piattaforme richieste: ognuna aggiunge la sua cartella in `bin/<plugin>.vst3/Contents/`;
5. aggiorna la configurazione IntelliSense di VS Code (`.vscode/c_cpp_properties.json`, una configurazione per plugin);
6. se richiesto, installa.

### Prerequisiti per le altre piattaforme (da Mac)

- **Windows**: `brew install mingw-w64`
- **Linux**: Docker Desktop aperto (su Apple Silicon attiva "Use Rosetta for x86_64/amd64 emulation"). L'immagine `dpf-linux` viene creata al primo build da `docker/linux/Dockerfile`.

### Compilare direttamente su Windows

Serve MSYS2 (shell "UCRT64") con `git make mingw-w64-ucrt-x86_64-gcc` e il Python ufficiale di Windows; il venv si attiva con `source venv/Scripts/activate`. Il file `.gitattributes` mantiene i fine riga Unix, necessari agli script.

## Interfaccia

Ogni UI deriva da `PluginUIBase` (`common/ui/PluginUIBase.hpp`) e implementa solo `drawContent()`. La base offre:

- `fParams[paramnome]`: valore di ogni parametro; `fZ`: scala di disegno (moltiplica ogni misura per `fZ`);
- finestra ridimensionabile con zoom, tema e palette comune (`Palette.hpp`);
- controlli collegati ai parametri, con automazione e doppio clic per il default: `knobParam`, `toggleButton`, `choiceButton`, `cycleButton`, `toggleCell`;
- elementi di layout: `drawTitle`, `sectionHeader`, `drawGenericParameters`.

Le modifiche alla parte comune vanno fatte in `common/ui/`, mai nelle copie dentro `plugin/source/`. `src/orbita/ui/` è un esempio completo di interfaccia personalizzata.

Nota Heavy: un bang sull'inlet sinistro di `[+] [*] [-] …` non ricalcola il risultato come in Pd. Per "ricalcolare con i valori memorizzati" manda il bang a un `[f]` messo prima della catena.

## Special guest

All'interno della cartella `/parse_max_pd` è presente un prototipo di parser per convertire patch max in pd. In via di sviluppo, non tutto è implementato, i due software hanno logiche molto diverse per alcune cose e al momento non sono gestiti casi specifici (es. non esiste il corssispettivo 1:1 di delay~ in pd, quindi è necessario fare delwrite~ e delread~ convertendo i campioni dati in max in millisecondi).

Per provare il file è sufficiente chiamare lo script, con il venv attivo, passando come argomento il nome del file .maxpat che si vuole convertire.

All'interno del file `table.json` è possibile aggiungere la conversione di oggetti per ampliare le possibilità del parser.
