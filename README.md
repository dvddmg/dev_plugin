# Develop plugin with HVCC e DPF in PureData

Questa è una repository per studiare come sviluppare un plugin in PureData grazie al compilatore `hvcc` e al framework `dpf`.

## Info 

Ambiente di sviluppo di plugin partendo da patch PureData.

Link utili per studio:

- https://github.com/Wasted-Audio/hvcc/tree/develop
- https://github.com/Wasted-Audio/heavylib.git
- https://github.com/distrho/dpf
- https://github.com/DISTRHO/DPF-Widgets.git

Questi sono anche i submoudle presenti all'interno di questo progetto, in particolare dpf e dpf-widgets sono utilizzate come dipendenze per la compilazinoe dei plugin.

## Create enviroment

HVCC è disponibile come libreria python, quindi è necessario creare un python enviroment e installare le dipendenze contenute in `requirements.txt`.

```bash
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt
```

## Develop and build

All'interno della cartella `./src` ci sono le cartelle per ogni plugin in cui troviamo principalmente questi due file necessari alla compilazione:

- *.pd = patch di PureData
- *.json = Metadata per la compilazione

Il file json deve avere questo contenuto base

```json
{
    "name": PLUGIN_NAME,
    "dpf": {
        "dpf_path": "../../dep/",
        "enable_ui": true,
        "description": DESCRIPTION,
        "maker": DEVELOPER,
        "brand_id": BRAND_ID,
        "unique_id": UNIQUE_ID,
        "homepage": "https://www.davidebardi.com/",
        "plugin_uri": "https://www.davidebardi.com/",
        "version": VERSION,
        "license": "GPL-3.0-or-later",
        "midi_input": 0,
        "midi_output": 0,
        "plugin_formats": [
            PLUGIN_FORMAT
        ]
    }
}
```

Le variabili in capslock devono essere sotituire con le proprie. Di seguito le descrizioni prese dalla documentazione.

- **name**: Name of plugin

- **brand_id**: A 4-character symbol that identifies a brand or manufacturer, with at least one non-lower case character. Plugins from the same brand should use the same symbol. Required when using AU.

- **unique_id**: A 4-character symbol which identifies a plugin. It must be unique within at least a set of plugins from the brand. Required when using AU

- **version**: Version of plugin with this format "1, 1, 1"

- **plugin_formats**: list of format, avilable lv2_sep, vst2, vst3, au, clap, jack

Maggiori dettagli a questo [link](https://wasted-audio.github.io/hvcc/generators/dpf/)

```bash
hvcc nome_file.pd -g dpf -m nome_file.json
make
```

Anche qui per il comando di compilazione è disponibile maggior documentazione a quessto [link](https://wasted-audio.github.io/hvcc/)

## Automatic

Per semmplificare i comandi sono disponibili due script per generare la cartella di lavoro all'interno di `/src` e per la compilazione.

Nello specifico con lo script `new.sh` è possibile creare una cartella con all'interno un file .pd di partenza e il suo file .json per i metadati. Le informazioni richieste da questo script vengono messe all'interno del file di metadati.

Succesivamente allo sviluppo della patch di Pd è possibile compilare l'oggetto della directory principale del progetto con lo script `build.sh` seguito dal nome del plugin che vogliamo compilare all'interno della cartella src.

Questi file sono sati scritti da Claude per velocizzare la prototipazione. Infatti il file .json è possible presonalizzarlo con le informazioni che lo script `new.sh` non richiede.

## Special guest

All'interno della cartella `/parse_max_pd` è presente un prototipo di parser per convertire patch max in pd. In via di sviluppo, non tutto è implementato, i due software hanno logiche molto diverse per alcune cose e al momento non sono gestiti casi specifici (es. non esiste il corssispettivo 1:1 di delay~ in pd, quindi è necessario fare delwrite~ e delread~ convertendo i campioni dati in max in millisecondi).

Per provare il file è sufficiente chiamare lo script, con il venv attivo, passando come argomento il nome del file .maxpat che si vuole convertire.