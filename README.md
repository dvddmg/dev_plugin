# Info

Ambiente di sviluppo di plugin partendo da patch PureData.

Link utili per studio:

- https://github.com/Wasted-Audio/hvcc/tree/develop
- https://github.com/Wasted-Audio/heavylib.git
- https://github.com/distrho/dpf
- https://github.com/DISTRHO/DPF-Widgets.git

Questi sono anche i submoudle presenti all'interno di questo progetto, in particolare dpf e dpf-widgets sono utilizzate come dipendenze per la compilazinoe dei plugin.

HVCC è disponibile come libreria python, quindi è necessario creare un python enviroment e installare le dipendenze contenute in `requirements.txt`.

```bash
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt
```

All'interno della cartella `./src` ci sono le cartelle per ogni plugin in cui troviamo principalmente questi due file necessari alla compilazione:

- *.pd = patch di PureData
- *.json = Metadata per la compilazione

```bash
hvcc nome_file.pd -g dpf -m nome_file.json
make
```

Nel file di metadata deve essere presente il percorso che punta al fork della repository DPF.
La variabile che deve essere sempre presente è `"dpf_path": "../../dep/"`