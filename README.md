# InternetGuard

[English](#english) · [Italiano](#italiano)

## English

**Functional plugin; author development discontinued.** InternetGuard has been
superseded by **[NetLink Teacher Control](https://github.com/lellomele/NetLink-Teacher-Control)**,
where future development is focused. Existing source and releases remain available.
NetLink is a separate, currently pre-release suite, not a drop-in plugin update.

### Features

- Block/allow Internet commands in Veyon Master.
- Toolbar toggle and commands for selected student computers.
- Native Windows Firewall rules on the client, without an external command process.
- One installer that detects Qt 5/Qt 6 and deploys the corresponding plugin.
- No additional runtime software beyond a compatible Windows Veyon installation.

### Install and use

Download **[v1.2.0](https://github.com/lellomele/veyon-internet-guard/releases/tag/v1.2.0)**.
Run **[install-internet-guard.exe](https://github.com/lellomele/veyon-internet-guard/releases/download/v1.2.0/install-internet-guard.exe)**
on the teacher and student PCs, confirm the Veyon folder and allow elevation
when required. Restart Veyon Master and Veyon Server.

| Veyon range for the distributed builds | Qt | Manual alternative |
|---|---|---|
| 4.7.5–4.8.x | Qt 5 | [internet-guard-qt5.dll](https://github.com/lellomele/veyon-internet-guard/releases/download/v1.2.0/internet-guard-qt5.dll) |
| 4.9.0–4.10.x | Qt 6 | [internet-guard-qt6.dll](https://github.com/lellomele/veyon-internet-guard/releases/download/v1.2.0/internet-guard-qt6.dll) |

For manual installation copy only the matching DLL into Veyon's `plugins`
folder on both sides, then restart. Core/Qt binary compatibility matters:
the wrong build can fail to load or crash. Version warnings are not proof
of compatibility with an unlisted Veyon release.

Use the toolbar to toggle the feature, or right-click selected computers and
choose **Block Internet** / **Allow Internet** for those targets.
Explicitly allow Internet again when finished: the rules are persistent.

### Limits

Student PCs must run Windows; the Veyon service needs firewall privileges.
The plugin blocks TCP 80/443/53/853/8080/8443/3128 and UDP 53/443/853.
These port blocks also apply to LAN destinations: there is no LAN web exemption.
It is not per-site filtering or a guarantee against every protocol, proxy, VPN
or local administrator. Check the actual traffic and local security policies.
Veyon's control ports are not targeted.

### Build and license

Build with CMake 3.16+, a compatible Qt/MinGW toolchain, matching Veyon headers
and core import library. Qt 5 targets core 4.7.5; the Qt 6 distribution targets
core 4.9.0 using Qt 6.7. Sources, commands and verification steps:
[ARCHITECTURE.md](ARCHITECTURE.md).

Copyright © 2025-26 prof. Ing. Raffaele Mele.
GPL-2.0-or-later: [LICENSE](LICENSE). Veyon-derived import libraries and symbol
lists retain Veyon Community attribution and the same license.
Third-party plugin, not an official Veyon release.

## Italiano

**Plugin funzionante; sviluppo interrotto dall'autore.** InternetGuard è ormai
superato dagli sviluppi di
**[NetLink Teacher Control](https://github.com/lellomele/NetLink-Teacher-Control)**,
sul quale si concentrerà lo sviluppo futuro. Codice e release restano disponibili.
NetLink è una suite separata, attualmente preliminare, non un aggiornamento
del plugin da copiare in Veyon.

### Funzionalità

- Comandi Blocca/Consenti Internet nella Console Veyon Master.
- Pulsante generale e comandi mirati alle postazioni selezionate.
- Regole native del Firewall di Windows sui client, senza processi esterni.
- Installer unico con rilevamento Qt 5/Qt 6 e scelta della DLL corrispondente.
- Nessun software aggiuntivo per l'uso, oltre a Veyon Windows compatibile.

### Installazione e uso

Scaricare **[v1.2.0](https://github.com/lellomele/veyon-internet-guard/releases/tag/v1.2.0)**
ed eseguire **[install-internet-guard.exe](https://github.com/lellomele/veyon-internet-guard/releases/download/v1.2.0/install-internet-guard.exe)**
su docente e client. Confermare la cartella Veyon, autorizzare l'elevazione
quando richiesta e riavviare Veyon Master e Veyon Server.

Le DLL distribuite sono `internet-guard-qt5.dll` per Veyon 4.7.5–4.8.x e
`internet-guard-qt6.dll` per 4.9.0–4.10.x; sono collegate nella tabella sopra.
In alternativa copiarne soltanto quella corretta nella sottocartella `plugins`
di Veyon su entrambi i lati e riavviare.
La compatibilità binaria con core/Qt è essenziale: una DLL errata può non
caricarsi o causare crash. Un avviso di versione non certifica versioni diverse.

Usare il pulsante della toolbar oppure clic destro sui PC selezionati e
**Block Internet** / **Allow Internet**. Al termine consentire esplicitamente
Internet: le regole sono persistenti.

### Limiti

I client devono usare Windows e il servizio Veyon deve poter modificare il firewall.
Sono bloccate TCP 80/443/53/853/8080/8443/3128 e UDP 53/443/853.
Il blocco di queste porte vale anche nella LAN: non è prevista un'esenzione
per siti web locali. Non è filtraggio per singolo sito né una garanzia contro
qualsiasi protocollo, proxy, VPN o amministratore locale.
Verificare traffico reale e criteri di sicurezza. Le porte di controllo Veyon
non sono oggetto delle regole.

### Compilazione e licenza

Servono CMake 3.16+, Qt/MinGW compatibili, header Veyon e import library del core
corrispondente. La build Qt 5 punta al core 4.7.5; quella Qt 6 distribuita usa
Qt 6.7 e core 4.9.0. Dettagli e verifiche: [ARCHITECTURE.md](ARCHITECTURE.md).

Copyright © 2025-26 prof. Ing. Raffaele Mele — GPL-2.0-or-later, [LICENSE](LICENSE).
Le import library e gli elenchi di simboli derivati da Veyon conservano
attribuzione Veyon Community e relativa licenza.
Plugin di terze parti, non una distribuzione ufficiale Veyon.
