# Informe técnico – Llamada al sistema `getppid` en xv6-riscv
**Integrantes:** Joaquín Wolde, Agustín de la Vega

---

## 1. Funcionamiento de la llamada al sistema

La llamada al sistema `getppid()` permite a un proceso obtener el identificador (PID) de su proceso padre. Para implementarla, fue necesario modificar tanto el kernel como el espacio de usuario, creando una conexión completa entre ambos.

**Componentes principales:**

**En el kernel:**
- Se asigna un número único a la syscall (22) en `syscall.h`
- El archivo `syscall.c` conecta ese número con la función implementadora
- La función `sys_getppid()` en `sysproc.c` accede a la estructura del proceso actual y retorna el PID del padre

**En el espacio de usuario:**
- El prototipo en `user.h` permite que los programas reconozcan la función
- El archivo `usys.S` genera el código ensamblador que prepara los registros y ejecuta la interrupción para transferir el control al kernel

**Flujo de ejecución:**
1. Un programa llama a `getppid()`
2. El esamblador coloca el número 22 en el registro correspondiente y ejecuta la instrucción
3. El control pasa al kernel, donde el dispatcher busca la función asociada al número 22
4. Se ejecuta `sys_getppid()`, que obtiene el proceso actual con `myproc()` y accede al PID del padre
5. El valor retornado se devuelve al programa de usuario

---

## 2. Explicación de las modificaciones realizadas

### 2.1 Modificaciones en el kernel

**Archivo `kernel/syscall.h`**  
Se asignó el número identificador para la nueva syscall:
```c
#define SYS_getppid 22
```

**Archivo `kernel/syscall.c`**  
Se declaró la función implementadora y se agregó al arreglo de syscalls que actúa como coordinador:
```c
extern uint64 sys_getppid(void);

static uint64 (*syscalls[])(void) = {
  /* ... */
  [SYS_getppid] sys_getppid,
};
```

**Archivo `kernel/sysproc.c`**  
Se implementó la lógica de la syscall:
```c
uint64
sys_getppid(void)
{
  struct proc *p = myproc();
  int ppid = -1;

  acquire(&p->lock);
  if (p->parent)
    ppid = p->parent->pid;
  release(&p->lock);

  return ppid;
}
```

La función obtiene el proceso actual, protege el acceso con locks, verifica que exista un padre, y retorna su PID (o -1 si no hay padre, como en el caso del proceso `init`).

### 2.2 Modificaciones en el espacio de usuario

**Archivo `user/user.h`**  
Se agregó el prototipo de la función:
```c
int getppid(void);
```

**Archivo `user/usys.pl`**  
Se añadió una entrada para generar automáticamente la nueva funcionlidad en el ensamblador:
```perl
entry("getppid");
```

### 2.3 Programa de prueba

Se creó `user/ppidtest.c` para verificar el funcionamiento:
```c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(void)
{
  int pid  = getpid();
  int ppid = getppid();

  printf("Mi PID es: %d\n", pid);
  printf("El PID de mi padre es: %d\n", ppid);
  exit(0);
}
```

**Archivo `Makefile`**  
Se agregó el programa a la lista `UPROGS` para que se compile e incluya en la imagen del sistema:
```make
UPROGS=\
  ...
  $U/_ppidtest
```

---

## 3. Dificultades encontradas y cómo se resolvieron

### 3.1 Error de tipos incompatibles en la tabla de syscalls

**Problema:** Al compilar apareció el error:
```
initialization of 'uint64 (*)(void)' from incompatible pointer type
```

**Causa:** El manejador estaba declarado con tipo de retorno `int` en lugar de `uint64`, lo que no coincidía con el tipo esperado por la tabla de syscalls.

**Solución:** Se cambió la firma de la función a `uint64 sys_getppid(void)` tanto en la implementación en `sysproc.c` como en la declaración `extern` en `syscall.c`.

### 3.2 Comando no encontrado al ejecutar ppidtest

**Problema:** Al intentar ejecutar `ppidtest` en xv6 aparecía:
```
ppidtest: command not found
```

**Causa:** El binario no se estaba incluyendo en la imagen del sistema de archivos. La entrada en `UPROGS` del `Makefile` tenía un formato incorrecto o faltaba el prefijo `$U/`.

**Solución:** Se verificó que la entrada siguiera el formato correcto `$U/_ppidtest`, asegurando que no tuviera barra invertida al final si era la última línea. Se ejecutó `make clean && make qemu` para reconstruir completamente el sistema.

### 3.3 Error de permisos al compilar en WSL

**Problema:** El compilador generaba:
```
fatal error: user/ppidtest.c: Permission denied
```

**Causa:** El archivo tenía permisos o propietario incorrecto, posiblemente por haber sido creado desde Windows en un entorno WSL.

**Solución:** Se corrigieron los permisos del archivo:
```bash
sudo chown $USER:$USER user/ppidtest.c
chmod 644 user/ppidtest.c
```
Luego se recompiló con `make clean && make qemu`.

### 3.4 Acceso inseguro al proceso padre

**Problema:** Al acceder directamente a `myproc()->parent->pid` sin validación, existe el riesgo de desreferenciar un puntero NULL (por ejemplo, el proceso `init` no tiene padre).

**Causa:** No se estaba verificando la existencia del padre ni protegiendo el acceso con locks.

**Solución:** Se agregó protección con locks y validación:
- Se usa `acquire(&p->lock)` antes de acceder al padre
- Se verifica que `p->parent` no sea NULL
- Se retorna `-1` si no hay padre
- Se libera el lock con `release(&p->lock)`

---

## Conclusión

Se implementó exitosamente la llamada `getppid()` en xv6, logrando conectar el espacio de usuario con el kernel a través de un coordinador que maneja la llamada.
