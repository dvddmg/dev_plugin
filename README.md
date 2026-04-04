# Info

Creare una cartella per ciascun plugin in src.

Comandi utili

```bash
hvcc nome_file.pd -g dpf -m nome_file.json
make
```

Nel file di metadata deve essere presente il percorso che punta al fork della repository DPF.
La variabile che deve essere sempre presente è `"dpf_path": "../../dep/"`