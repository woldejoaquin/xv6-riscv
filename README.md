# Informe de Protección de Lectura en xv6

**Integrantes:** Joaquín Wolde, Agustín de la Vega

---

## Introducción

Este documento describe la implementación de un mecanismo de protección de lectura en el sistema operativo xv6. El objetivo fue desarrollar un modelo de memoria de "solo escritura", diseñado para resguardar datos sensibles como claves criptográficas y credenciales. Para ello, se modificaron las estructuras de paginación del kernel, incorporando las llamadas al sistema `mrdprotect` y `munrdprotect` para manipular directamente los permisos en las tablas de páginas

---

## Implementación del Modelo

### 1. Lógica de Manipulación de Memoria (`kernel/vm.c`)

La implementación central se realizó modificando las estructuras de paginación en el archivo `vm.c`. Se utilizó la función `walk` para navegar la tabla de páginas de tres niveles del hardware RISC-V y obtener el puntero a la entrada (PTE) correspondiente a la dirección virtual procesada.

Para modificar los permisos de acceso, se aplicaron operaciones a nivel de bits (bitwise) directamente sobre las entradas de la tabla:

- **Revocar lectura (`mrdprotect`)**: Se aplicó una máscara lógica AND con el complemento del flag de lectura para limpiar el bit, manteniendo intactos el resto de los permisos.
```c
*pte &= ~PTE_R; // Limpia el bit de lectura
```

- **Restaurar lectura (`munrdprotect`)**: Se utilizó una operación lógica OR para reactivar el flag de lectura.
```c
*pte |= PTE_R; // Activa el bit de lectura
```

Un aspecto crítico de la implementación fue la sincronización con el hardware. Tras modificar cualquier PTE, se aseguró la ejecución de la instrucción `sfence.vma()` para invalidar el Translation Lookaside Buffer (TLB), garantizando que la MMU reconozca los nuevos permisos inmediatamente y no utilice traducciones cacheadas obsoletas.

### 2. Interfaz de Llamadas al Sistema (`kernel/sysproc.c`)

Se expusieron las funciones al espacio de usuario a través de `sysproc.c`, donde se capturan los argumentos `addr` y `len`. Estas funciones actúan como una capa de interfaz (wrappers) que invoca la lógica implementada en `vm.c` tras realizar las validaciones iniciales.

### 3. Validaciones de Seguridad

Siguiendo el principio de defensa en profundidad, se implementaron controles rigurosos antes de ejecutar cualquier modificación en la memoria. El sistema retorna un error (`-1`) en los siguientes casos:

- **Alineación**: La dirección `addr` no está alineada al inicio de una página (`PGSIZE`).
- **Longitud**: El argumento `len` es menor o igual a cero.
- **Integridad de Memoria**: Las páginas objetivo no poseen el bit de validez (`PTE_V`) o no pertenecen al espacio de usuario (`PTE_U`), evitando así la corrupción de memoria no mapeada o perteneciente al kernel.

---

## Problemas Encontrados y Soluciones

Durante el desarrollo de la solución, se enfrentaron diversos desafíos relacionados con la estructura interna de XV6 y el entorno de compilación. A continuación, se detallan los más significativos y sus respectivas soluciones:

### 1. Incompatibilidad en la API de Argumentos

Al implementar la captura de parámetros en `sysproc.c`, el compilador arrojó el error `void value not ignored as it ought to be`.

- **Causa**: Se intentó validar el retorno de las funciones `argaddr` y `argint` dentro de sentencias `if`. Se detectó que, en la versión RISC-V de XV6, estas funciones tienen retorno `void` y manejan los errores provocando un `panic` automático, a diferencia de versiones anteriores que retornaban códigos de error.
- **Solución**: Se eliminaron las comprobaciones condicionales explícitas y se realizaron las llamadas a las funciones de argumentos de forma directa, delegando el manejo de excepciones al mecanismo interno del kernel.

### 2. Visibilidad de Símbolos y Errores de Enlace

Durante la fase de enlazado (linking), se presentó el error `undefined reference to 'growproc'`, impidiendo la generación de la imagen del kernel.

- **Causa**: La función `growproc` estaba definida con la palabra clave `static` dentro de `proc.c`, lo que restringía su visibilidad únicamente a ese archivo y la hacía inaccesible desde otros módulos del sistema.
- **Solución**: Se modificó la definición en `proc.c` eliminando el modificador `static` y se agregó su prototipo (`int growproc(int);`) en el archivo de cabecera global `kernel/defs.h`, exponiéndola así al resto del kernel.

### 3. Declaraciones Implícitas y Gestión de Cabeceras

Inicialmente, la compilación falló con advertencias de `implicit declaration of function` al intentar invocar la nueva lógica de memoria desde la interfaz de llamadas al sistema.

- **Causa**: Aunque las funciones se implementaron correctamente en `vm.c`, no se registraron en el archivo de definiciones central, por lo que `sysproc.c` desconocía su existencia.
- **Solución**: Se registraron las firmas de las nuevas funciones (`uvm_rdprotect`, etc.) en `kernel/defs.h` bajo la sección correspondiente a vm, permitiendo que el compilador enlazara correctamente las llamadas entre los distintos subsistemas.

---

### Validación del Funcionamiento

Para verificar la correcta implementación del modelo de protección, se compiló y ejecutó el programa de prueba `rdprotect_test` suministrado en las especificaciones.
**Resultados obtenidos:** Como se evidencia en la captura de pantalla adjunta, el kernel de XV6 logra interceptar el acceso a la memoria protegida.
[![image.png](https://i.postimg.cc/nL6wTcFT/image.png)](https://postimg.cc/hJ1rtn77)

---

## Conclusión

El desarrollo de esta tarea permitió integrar con éxito un mecanismo de seguridad avanzado en el kernel de XV6, logrando la implementación de memoria de "solo escritura". A través de la manipulación directa de los bits de control en la Tabla de Páginas (PTE) y la correcta gestión del TLB, se estableció un modelo robusto que impide la lectura no autorizada de datos sensibles.
