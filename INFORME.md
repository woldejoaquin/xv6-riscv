# Informe de Instalación y Ejecución de xv6

**Integrantes:** Joaquín Wolde, Agustín de la Vega

---

### Introducción

Este documento describe el procedimiento realizado para instalar y ejecutar el sistema operativo educativo **xv6**. El objetivo fue familiarizarnos con la compilación y el funcionamiento de un sistema operativo simple. Para ello, se utilizó el **Subsistema de Windows para Linux (WSL)** con **Ubuntu** como entorno de desarrollo y el emulador **QEMU** para la virtualización.

---

### Pasos de la Instalación

1.  **Configuración del Entorno:** Se instaló WSL con la distribución Ubuntu en Windows para obtener una terminal de tipo Unix, esencial para la compilación.

2.  **Obtención del Código Fuente:** Se clonó el repositorio oficial de xv6 y, para mantener un flujo de trabajo organizado, se creó una nueva rama con los nombres de los integrantes del grupo.

3.  **Instalación de Dependencias:** Se instalaron las herramientas de compilación necesarias (`make`, `gcc`, `bc`) y el emulador QEMU, que es fundamental para ejecutar el sistema operativo.

4.  **Compilación y Ejecución:** Utilizando el comando `make`, se compiló el código fuente de xv6 para generar la imagen del sistema. Posteriormente, se inició el sistema operativo en el emulador con el comando `make qemu`.

---

### Problema Encontrado y Solución

Durante el primer intento de ejecución, nos encontramos con un **error de incompatibilidad de versión**. El sistema requería una versión de **QEMU 7.2 o superior**, pero la que teníamos instalada era más antigua.

Para solucionarlo, **actualizamos los repositorios de paquetes de nuestro sistema Ubuntu** y reinstalamos QEMU. Este proceso nos permitió obtener la versión más reciente del emulador (versión 9), la cual cumplía sobradamente con el requisito. Tras la actualización, xv6 se ejecutó sin inconvenientes.

---

### Verificación del Funcionamiento

Confirmamos que la instalación fue exitosa de dos maneras:

* **Arranque del Sistema:** Observamos en la terminal los mensajes de arranque del kernel de xv6, finalizando con la aparición del `prompt` de la línea de comandos, lo que indicaba que el sistema se había cargado correctamente.

* **Ejecución de Comandos:** Probamos comandos básicos de la shell de xv6 como `ls` y `echo`. Todos respondieron como se esperaba, demostrando que el sistema operativo era funcional y capaz de gestionar tareas simples.
