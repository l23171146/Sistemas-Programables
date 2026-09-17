
# Interrupciones Externas y Buffer Circular con Arduino UNO R4 WiFi

## Descripción

Este proyecto implementa un sistema de detección de eventos mediante un pulsador que simula el sensor de piezas de una banda transportadora. Se utiliza una interrupción externa para detectar cada pulsación, un buffer circular de tamaño fijo para almacenar los eventos y una animación no bloqueante en la matriz de LEDs integrada del Arduino UNO R4 WiFi.

## Objetivos de aprendizaje

* Comprender el funcionamiento de las interrupciones externas en Arduino.
* Utilizar un pulsador como simulación de un sensor de piezas.
* Implementar un buffer circular de tamaño fijo para almacenar eventos.
* Aplicar un filtro de rebote por software.
* Utilizar `millis()` para realizar una animación sin bloquear el programa.
* Comprender el funcionamiento de los contadores de escritura y lectura del buffer.

## Herramientas y material utilizado

* Arduino UNO R4 WiFi
* Arduino IDE
* Matriz de LEDs integrada en el Arduino UNO R4 WiFi
* Protoboard
* Pulsador
* Cables de conexión (jumpers)

## Diagrama

![Diagrama del circuito](Diagrama/Diagrama%20Buffers.png)

![Armado](Diagrama/Armado.jpeg)

[Ver carpeta Diagrama](Diagrama)

## Código

El programa utiliza una interrupción externa en el pin 2 para detectar las pulsaciones del botón. Cada evento se almacena en un buffer circular de 16 posiciones, mientras la matriz de LEDs continúa ejecutando su animación mediante `millis()` sin detener el programa.

[Ver código Buffers.ino](Codigo/Buffers.ino)

[Ver carpeta Codigo](Codigo)

## Reporte

El reporte explica el funcionamiento de las interrupciones externas, el buffer circular, el filtro de rebote, la animación no bloqueante y el procesamiento de los eventos detectados.

[Ver reporte](Reporte/Reporte.pdf)

[Ver carpeta Reporte](Reporte)

## Resultados

Durante las pruebas, el pulsador generó una interrupción externa al ser presionado, registrando los eventos en el buffer circular. El filtro de rebote de 50 ms permitió evitar registros múltiples ocasionados por una sola pulsación.

Mientras se detectaban y procesaban los eventos, la animación de la matriz de LEDs continuó funcionando sin detenerse. Los contadores `escritos` y `leidos` permitieron controlar los eventos almacenados y el programa también pudo detectar cuando el buffer alcanzaba su capacidad máxima.

## Video

[Ver video](https://youtu.be/R35mJx2twG4)

[Ver carpeta Video](Video)

## Conclusiones

### Gabriel

En esta práctica comprendí cómo las interrupciones externas permiten que el Arduino detecte eventos de manera inmediata mientras realiza otras tareas. También aprendí la utilidad del buffer circular para almacenar temporalmente los eventos del pulsador y procesarlos posteriormente sin detener la animación de la matriz de LEDs.

### Javier

Durante la práctica aprendí la importancia de mantener una rutina de interrupción rápida y sencilla. El uso del filtro de rebote permitió evitar que una sola pulsación del botón fuera registrada varias veces, mientras que los contadores `escritos` y `leidos` facilitaron el control de los eventos almacenados en el buffer.

### Rosa

Esta práctica me permitió comprender cómo realizar varias tareas al mismo tiempo utilizando una programación no bloqueante. La animación de la matriz continúa funcionando mientras el Arduino detecta y procesa las pulsaciones del botón, mostrando cómo las interrupciones, `millis()` y el buffer circular pueden trabajar juntos para responder a eventos sin detener el programa.
