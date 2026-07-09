# Progetto Universitario
Prevede la comunizione tra sistemi embedded con un drone in tempo reale.

# Componenti
Il progetto prevede la comunicazione fra due schede una **ESP32 C5** e una **STM32F303**, inoltre la comunicazione avverrà con il drone **NMY300**
con l'ausilio di due display LCD e un USB host shield il cui compito sarà quello di permettere di catturare i pacchetti in arrivo dal dispositivo **Dualsense** 
usato per inviare i comandi di volo al drone.

# Struttura cartelle 
_Cartella main_: Prevede il codice usato per la ESP32 che gestisce l'interfaccia _UART_, l'interfaccia nella gestione della comunicazione _socket_ per il drone e gestione dei _led RGB_ per un feedback comunicativo.

_Cartella Core_: Contiene il codice della STM32 che viene diviso in due cartella a sua volta _Inc_ e _Src_. La prima contiene gli header file, la seconda contiene appunto i source file. La STM32 svolge da orchestratore e comnuicherà con il modulo host SUB shield mini v2.0 avente chip _MAX4231E_, due display tramite protocollo _I2C_, protocollo _USART_ con la **ESP_32** e procollo _SPI_ con l'host shield. 

# Collegamento PIN tra schede
|----------
|                  
