# Control de acceso con RFID RC522

## Descripción

En esta práctica se implementó un sistema de control de acceso utilizando un lector RFID RC522 mediante el protocolo de comunicación SPI.

El sistema permite leer el UID de una tarjeta o llavero RFID, mostrarlo en el Monitor Serie y compararlo con un UID autorizado previamente definido en el programa. Dependiendo del resultado, se muestra un mensaje de acceso permitido o denegado y se controla el encendido de los LEDs.

El sistema utiliza `millis()` para controlar el tiempo de encendido de los LEDs durante 2000 ms sin bloquear la lectura de nuevas tarjetas.

## Objetivos

* Comprender el funcionamiento de la comunicación SPI.
* Conectar y configurar un lector RFID RC522 con Arduino.
* Leer y mostrar el UID de tarjetas y llaveros RFID.
* Comparar un UID leído con un UID autorizado.
* Simular un sistema de control de acceso.
* Utilizar `millis()` para controlar el tiempo sin bloquear el programa.
* Comprobar el funcionamiento de la comunicación SPI mediante el lector RC522.

## Herramientas y material utilizado

### Hardware

* Arduino
* Lector RFID RC522
* Tarjeta RFID
* Llavero RFID
* LED verde
* LED rojo
* Resistencias de 220 Ω
* Cables de conexión
* Protoboard

### Software

* Arduino IDE
* Librería `SPI`
* Librería `MFRC522`
* Monitor Serie

## Diagrama

El lector RFID RC522 se comunica con el Arduino mediante el protocolo SPI. El módulo se alimenta con **3.3 V** y utiliza los pines correspondientes para SCK, MOSI, MISO, SS y RST.

El LED verde indica un acceso permitido, mientras que el LED rojo indica un acceso denegado.

![Diagrama de conexión](Diagrama.jpeg)

[Ver carpeta Diagrama](Diagrama)

## Código

El programa inicializa la comunicación SPI y el lector RFID RC522. Al iniciar, comprueba la comunicación con el lector mediante el registro de versión del módulo.

Cada vez que se acerca una tarjeta o llavero, se obtiene su UID y se muestra en el Monitor Serie. El UID leído se compara con el UID autorizado almacenado en el programa.

Si el UID coincide, se muestra `ACCESO PERMITIDO` y se enciende el LED verde. Si no coincide, se muestra `ACCESO DENEGADO` y se enciende el LED rojo.

El tiempo de encendido de los LEDs se controla mediante `millis()`, permitiendo que el sistema continúe leyendo tarjetas mientras alguno de los LEDs permanece encendido.

[Ver código control_acceso_rfid.ino](Codigo/control_acceso_rfid.ino)

[Ver carpeta Código](Codigo)

## Terminal

En las pruebas realizadas mediante el Monitor Serie se observaron los mensajes correspondientes a la comunicación con el lector, la lectura de los UID y el resultado del control de acceso.

![Prueba en Monitor Serie](Terminal.jpeg)

[Ver carpeta Terminal](Terminal)

## Resultados

### Acceso permitido

Al acercar la tarjeta cuyo UID se encuentra registrado como autorizado, el sistema muestra el mensaje `ACCESO PERMITIDO` y enciende el LED verde durante 2000 ms.

![LED verde](LED_verde.jpeg)

### Acceso denegado

Al acercar una tarjeta o llavero cuyo UID no corresponde al autorizado, el sistema muestra el mensaje `ACCESO DENEGADO` y enciende el LED rojo durante 2000 ms.

![LED rojo](LED_rojo.jpeg)

## Reporte

En el reporte se explica el funcionamiento del protocolo SPI utilizado por el lector RFID RC522, la lectura del UID, el funcionamiento del control de acceso, el uso de `millis()` y las pruebas realizadas durante la práctica.

[Ver reporte](Reporte/Reporte.pdf)

## Video

En el siguiente video se muestra el funcionamiento del sistema de control de acceso mediante el lector RFID RC522.

[Ver video de la práctica](PEGA_AQUI_EL_LINK_DEL_VIDEO)

[Ver carpeta Video](Video)

## Conclusiones

La práctica permitió comprender el funcionamiento de un lector RFID RC522 mediante el protocolo SPI y la forma en que el Arduino puede obtener y procesar el UID de una tarjeta o llavero.

También se comprobó cómo utilizar la información obtenida del lector para simular un sistema de control de acceso, diferenciando entre tarjetas autorizadas y no autorizadas mediante el control de los LEDs.

Finalmente, el uso de `millis()` permitió controlar el tiempo de encendido de los LEDs sin detener la lectura del lector RFID, permitiendo que el sistema continúe respondiendo mientras se encuentra activo el indicador de acceso.
