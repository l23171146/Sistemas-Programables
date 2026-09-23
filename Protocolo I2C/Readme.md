
# Comunicación I2C entre 4 Arduinos

## Descripción

En esta práctica se implementó un sistema de comunicación mediante el protocolo I2C utilizando cuatro Arduino UNO R4 WiFi. Un Arduino funciona como maestro y los otros tres como esclavos, cada uno con una dirección diferente.

El maestro se comunica con los esclavos para controlar un LED, controlar un servomotor mediante un potenciómetro y mostrar los valores obtenidos mediante el Monitor Serie.

## Integrantes

* Gabriel
* Javier
* Rosa

## Objetivos

* Comprender el funcionamiento del protocolo de comunicación I2C.
* Implementar la comunicación entre un Arduino maestro y tres esclavos.
* Utilizar direcciones diferentes para identificar cada dispositivo.
* Enviar y recibir datos mediante el bus I2C.
* Controlar un LED y un servomotor mediante comunicación I2C.
* Leer un potenciómetro y utilizar su valor para controlar el servomotor.
* Utilizar `millis()` para realizar consultas periódicas sin bloquear el programa.

## Herramientas y material utilizado

### Hardware

* 4 Arduino UNO R4 WiFi
* 1 protoboard
* 1 LED
* 1 resistencia de 470 Ω
* 1 micro servomotor
* 1 potenciómetro
* Resistencias de 4.7 kΩ para SDA y SCL
* Cables de conexión
* Cables USB

### Software

* Arduino IDE
* Librería `Wire`
* Librería `Servo`
* Monitor Serie

## Diagrama

El sistema está compuesto por un Arduino maestro y tres Arduino esclavos conectados mediante el protocolo I2C.

Las líneas SDA y SCL son compartidas entre los cuatro Arduino y todos utilizan una tierra común. Cada esclavo tiene una dirección diferente: `0x08` para el LED, `0x09` para el servomotor y `0x0A` para el potenciómetro.

![Diagrama de conexión](Diagrama/Diagrama.jpg)
![Armado](Diagrama/Armado.jpg)

[Ver carpeta Diagrama](Diagrama)

## Código

El proyecto contiene cinco archivos de código utilizados para la implementación y las pruebas de la comunicación I2C:

* [MaestroR4.ino](Codigo/MaestroR4.ino) — Programa del **Arduino maestro adaptado para UNO R4 WiFi**, encargado de controlar el LED, recibir el valor del potenciómetro y controlar el servomotor.

* [Master.ino](Codigo/Master.ino) — Programa del **Arduino maestro**, utilizado como versión de referencia para la comunicación I2C y el control de los dispositivos.

* [Esclavo1.ino](Codigo/Esclavo1.ino) — Programa del **Arduino esclavo 1**, encargado de controlar el LED mediante la dirección I2C `0x08`.

* [Esclavo2.ino](Codigo/Esclavo2.ino) — Programa del **Arduino esclavo 2**, encargado de controlar el servomotor mediante la dirección I2C `0x09`.

* [Esclavo3.ino](Codigo/Esclavo3.ino) — Programa del **Arduino esclavo 3**, encargado de leer el potenciómetro y enviar su valor al maestro mediante la dirección I2C `0x0A`.

[Ver carpeta Código](Codigo)


## Reporte

En el reporte se explica el funcionamiento de la comunicación I2C entre los cuatro Arduino, la configuración del maestro y los esclavos, el control del LED, el servomotor y el potenciómetro, así como las consideraciones utilizadas para el Arduino UNO R4 WiFi.

[Ver reporte](Reporte/Reporte.pdf)

## Resultados

Durante la práctica se implementó la comunicación entre un Arduino maestro y tres esclavos mediante el protocolo I2C. Se trabajó con diferentes direcciones para cada dispositivo y se realizaron pruebas de comunicación para controlar los componentes conectados.
![Resultados Terminal](Diagrama/Terminal.jpeg)

## Video

En el siguiente video se muestra el funcionamiento de la comunicación I2C entre los cuatro Arduino y el control de los dispositivos.

[Ver video de la práctica](https://youtu.be/xtoZlY5BY0I)

[Ver carpeta Video](Video)


## Conclusiones

La práctica permitió comprender el funcionamiento de la comunicación I2C utilizando un Arduino como maestro y tres como esclavos. Se aprendió cómo utilizar direcciones diferentes para establecer la comunicación con cada dispositivo mediante las líneas SDA y SCL.

También se comprendió cómo el maestro puede enviar órdenes y solicitar información a diferentes esclavos para controlar componentes como un LED y un servomotor, además de recibir la lectura de un potenciómetro.

En conjunto, la práctica permitió aplicar la comunicación entre múltiples dispositivos y comprender su utilidad para coordinar diferentes componentes mediante un mismo bus de comunicación.
