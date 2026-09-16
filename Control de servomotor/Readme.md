
# Control de servomotor por interfaz web

## Descripción

Esta práctica tiene como propósito controlar el ángulo de un servomotor mediante una interfaz web. El ángulo deseado se envía desde una página web hacia un Arduino UNO R4 WiFi, el cual recibe la información y posiciona el servomotor de acuerdo con el valor indicado.

## Objetivos

* Controlar un servomotor mediante una interfaz web.
* Enviar el ángulo deseado desde una página web hacia el Arduino.
* Posicionar el servomotor de acuerdo con el ángulo indicado.
* Comprender la comunicación entre la interfaz web y el Arduino UNO R4 WiFi.
* Observar el funcionamiento del sistema mediante diferentes valores de ángulo.

## Material

* Arduino UNO R4 WiFi.
* Servomotor.
* Cables de conexión.
* Computadora.
* Red WiFi.

## Diagrama

El diagrama muestra la conexión de los componentes utilizados para realizar la práctica.

![Diagrama.jpg](Diagrama/Diagrama.jpg)

![Armado.jpg](Diagrama/Armado.jpg)

[Ver carpeta Diagrama](Diagrama)

## Código

El código contiene la programación utilizada para establecer la conexión WiFi, crear la interfaz web y controlar el ángulo del servomotor.

[Ver código](Codigo/Control%20de%20servomotor%20por%20interfaz%20web.ino)

## Video

El video muestra en vivo el funcionamiento del sistema y el control del servomotor mediante la interfaz web.

[Ver información del video](Video/readme.txt)

[Ver video en YouTube](https://youtube.com/shorts/j2AcaIiO340?feature=share)

## Reporte

El reporte contiene la metodología utilizada, capturas de la interfaz web funcionando, análisis de los resultados y conclusiones individuales de los integrantes del equipo.

[Ver Reporte](Reporte/Reporte.pdf)

## Conclusiones técnicas

La práctica permitió comprender cómo una interfaz web puede utilizarse para controlar un componente físico mediante comunicación inalámbrica. El Arduino UNO R4 WiFi recibe el ángulo enviado desde la página web y utiliza esta información para establecer la posición del servomotor.

También se comprendió la relación entre la programación, la comunicación WiFi y el funcionamiento del hardware, comprobando que una instrucción enviada desde una interfaz puede generar una respuesta en un componente electrónico.
