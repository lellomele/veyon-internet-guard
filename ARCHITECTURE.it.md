# InternetGuard: architettura e compilazione

[English](ARCHITECTURE.md) · [Italiano](ARCHITECTURE.it.md)

Funzionalità, installazione e stato di manutenzione: [README](README.md).

## Componenti e flusso dei comandi

`InternetGuardPlugin` fornisce il pulsante nella toolbar Master e le azioni sui
PC selezionati. I comandi attraversano il meccanismo autenticato delle funzionalità
Veyon. Il server studente invoca `WindowsFirewall`, che crea/rimuove regole
nominate usando `INetFwPolicy2`/`INetFwRule`.
Il backend attuale non avvia strumenti a riga di comando. Gli errori mantengono
gli HRESULT; la rimozione gestisce voci di proprietà duplicate/mancanti.

UUID del pulsante: `a4b3c2d1-e5f6-7890-abcd-ef1234567890`; UID funzionalità e
`uid()` devono coincidere. L'IID metadati `io.veyon.Veyon.Plugins.InternetGuard`
è un identificativo stabile separato. Le sottoazioni hanno UUID costanti distinti.
`VeyonCompat.h` centralizza inclusioni API, macro versione e decodifica comandi.

## Comportamento del firewall

| Regola di proprietà | Trasporto/porta |
|---|---|
| VeyonIG_BlockHTTP | TCP 80 |
| VeyonIG_BlockHTTPS | TCP 443 |
| VeyonIG_BlockDNS_UDP | UDP 53 |
| VeyonIG_BlockDNS_TCP | TCP 53 |
| VeyonIG_BlockQUIC | UDP 443 |
| VeyonIG_BlockDoT_TCP | TCP 853 |
| VeyonIG_BlockDoT_UDP | UDP 853 |
| VeyonIG_BlockProxy | TCP 8080,8443,3128 |

Le regole persistono fino alla rimozione. L'applicazione elimina i duplicati di
proprietà prima di ricrearli; lo sblocco rimuove anche il nome legacy
`VeyonIG_AllowLAN`. La precedenza del blocco impedisce a una regola LAN di
autorizzare le stesse porte bloccate. Non sono previsti filtri hostname/SNI
né modifiche al file hosts. Il blocco per porte non è un confine di sicurezza
generale: verificare flussi esistenti, altre porte/protocolli e policy effettiva.
Le porte di controllo Veyon non sono interessate.

## Compatibilità binaria

La compatibilità sorgente non dimostra compatibilità ABI.
Usare header e libreria d'importazione corrispondenti al core Veyon previsto,
Qt major/minor corretto e toolchain MinGW compatibile. Non mescolare DLL di build diverse.

| Distribuzione | Target header/import | Riferimento Qt/toolchain |
|---|---|---|
| Veyon 4.7.5–4.8.x | core 4.7.5 | Qt 5.12, MinGW g++ 7.3 |
| Veyon 4.9.0–4.10.x | core 4.9.0 | Qt 6.7, MinGW 13.1.0 |

Un plugin compilato con Qt minor più recente dell'host può essere rifiutato.
Ispezionare versione delle DLL Qt host e tag Qt del plugin.
Il sorgente adatta `FeatureMessage::command()` con `VEYON_DECODE_COMMAND`.
AUTOMOC genera gli export metadati; Qt 6 usa l'export query v2.

## Compilazione

Gli strumenti indicati sono requisiti sviluppatore, non prerequisiti runtime aggiuntivi.

### Qt 5

Installare Qt 5.12 con MinGW g++ 7.3 e procurarsi gli header Veyon 4.7.5.
In PowerShell dalla radice del repository:

```powershell
$env:PATH = "C:\Qt\Tools\mingw730_64\bin;$env:PATH"
cmake -S . -B build-qt5 -G "MinGW Makefiles" `
  -DCMAKE_CXX_COMPILER="C:/Qt/Tools/mingw730_64/bin/g++.exe" `
  -DCMAKE_MAKE_PROGRAM="C:/Qt/Tools/mingw730_64/bin/mingw32-make.exe" `
  -DVEYON_TARGET_VERSION=4.7.5 `
  -DVEYON_SOURCE_DIR="C:/src/veyon-4.7.5"
cmake --build build-qt5
```

Output: `build-qt5/internet-guard-qt5.dll`.

### Qt 6

Usare Qt 6.7.2, MinGW compatibile e header Veyon 4.9.0.
Se necessario ottenere l'ambiente Qt sviluppatore con aqtinstall:

```powershell
py -m pip install aqtinstall
py -m aqt install-qt windows desktop 6.7.2 win64_mingw --outputdir C:\Qt-aqt
py -m aqt install-tool windows desktop tools_mingw1310 --outputdir C:\Qt-aqt
git clone --depth 1 --branch v4.9.0 https://github.com/veyon/veyon.git ..\veyon-src-490
$mingw = "C:\Qt-aqt\Tools\mingw1310_64\bin"
$qt6 = "C:\Qt-aqt\6.7.2\mingw_64"
$env:PATH = "$mingw;$qt6\bin;$env:PATH"
cmake -S . -B build-qt6 -G "MinGW Makefiles" `
  -DWITH_QT6=ON `
  -DCMAKE_CXX_COMPILER="$mingw/g++.exe" `
  -DCMAKE_MAKE_PROGRAM="$mingw/mingw32-make.exe" `
  -DCMAKE_PREFIX_PATH="$qt6" `
  -DVEYON_SOURCE_DIR="..\veyon-src-490"
cmake --build build-qt6
```

Output: `build-qt6/internet-guard-qt6.dll`.
CMake seleziona la libreria d'importazione core inclusa in base al ramo Qt.
Per un altro core generarla dalla relativa `veyon-core.dll` e impostare
`VEYON_CORE_LIBRARY` e `VEYON_SOURCE_DIR` su input corrispondenti:

```powershell
gendef veyon-core.dll
dlltool -d veyon-core.def -l libveyon-core-qt6.dll.a -D veyon-core.dll
```

Opzioni CMake importanti: `WITH_QT6`, `VEYON_TARGET_VERSION`,
`VEYON_SOURCE_DIR`, `VEYON_CORE_LIBRARY`. Default in CMakeLists.txt.

### Installer

Dopo aver compilato entrambe le DLL:

```powershell
pwsh -File installer\build-installer.ps1 `
  -PluginDllQt5 build-qt5\internet-guard-qt5.dll `
  -PluginDllQt6 build-qt6\internet-guard-qt6.dll
```

L'installer Win32 autonomo incorpora entrambe le varianti, rileva Qt, legge la
versione Veyon, seleziona la destinazione e richiede elevazione quando necessaria.
Un avviso di versione non dimostra compatibilità oltre l'intervallo documentato.

## Verifiche e risorse

Prima della distribuzione ispezionare importazioni DLL, tag Qt ed export metadati.
Su PC dedicati installare su Master e Server, riavviarli e verificare visibilità
pulsante, destinatari selezionati, traffico reale e rimozione regole di proprietà.
`tools/fw_test.cpp` è un test backend manuale: con elevazione modifica regole
firewall reali. Non eseguirlo su sistemi operativi senza autorizzazione.
Il plugin non dispone di una suite di test unitari automatici.

L'icona runtime è il PNG incorporato `network-offline.png`; il sorgente modificabile
è `network-offline.svg`. Non introdurre dipendenza runtime da Qt Svg.
Nell'SVG usare colori/opacità espliciti evitando sintassi fill non supportate.

Copyright e licenza: [README](README.md) e [LICENSE](LICENSE).
