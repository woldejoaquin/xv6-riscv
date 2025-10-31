# Informe Técnico – Implementación de Lottery Scheduling en xv6-riscv

**Integrantes:** Joaquín Wolde, Agustín de la Vega

---

## 1. Funcionamiento y Lógica de la Implementación

El objetivo de esta implementación fue reemplazar el planificador Round-Robin de XV6 por un sistema de **reparto proporcional** (proportional-share) basado en lotería. A diferencia del Round-Robin tradicional, donde cada proceso recibe la misma cantidad de tiempo de CPU, el planificador de lotería asigna tiempo de CPU de manera probabilística, proporcionalmente al número de "tickets" que posee cada proceso.

### Componentes principales

**Estructura del proceso:**
- Se añadieron dos campos a `struct proc`:
  - `int tickets`: número de tickets asignados al proceso para la lotería
  - `int run_slices`: contador de veces que el proceso ha sido ejecutado

**Llamadas al sistema:**
- `settickets(int n)`: permite a un proceso modificar su número de tickets
- `get_slices(void)`: permite a un proceso consultar su contador de ejecuciones

### Algoritmo de selección

La lógica central se implementó modificando la función `scheduler()` en `kernel/proc.c`. El algoritmo opera en tres pasos por cada ciclo de planificación:

**Paso 1: Calcular el total de tickets**  
El planificador itera sobre todos los procesos en estado `RUNNABLE` y suma sus tickets para obtener `total_tickets`.

**Paso 2: Generar el ticket ganador**  
Se genera un número pseudoaleatorio `winning_ticket` en el rango `[1, total_tickets]` utilizando un generador de congruencia lineal (LCG).

**Paso 3: Encontrar al proceso ganador**  
El planificador itera nuevamente sobre los procesos `RUNNABLE`, acumulando sus tickets. El primer proceso cuya suma acumulada sea mayor o igual a `winning_ticket` es seleccionado como ganador.

**Ejecución:**  
El proceso ganador pasa al estado `RUNNING`, se incrementa su contador `run_slices++`, y se le cede la CPU.

### Garantía de proporcionalidad

Este mecanismo asegura que, estadísticamente y a lo largo del tiempo, un proceso con 500 tickets recibirá aproximadamente 10 veces más tiempo de CPU que un proceso con 50 tickets.

---

## 2. Explicación de las Modificaciones Realizadas

### 2.1 Modificaciones en el kernel

**Archivo `kernel/proc.h`**  
Se extendió la estructura `struct proc` con los nuevos campos:
```c
struct proc {
  int tickets;      // Número de tickets para lottery scheduling
  int run_slices;   // Contador de veces ejecutado
};
```

**Archivo `kernel/proc.c`**  
Se realizaron tres modificaciones principales:

1. **Inicialización en `allocproc()`:**
```c
p->tickets = 100;     // Valor por defecto
p->run_slices = 0;
```

2. **Reimplementación completa de `scheduler()`:**  
Se reemplazó la lógica Round-Robin por el algoritmo de lotería de dos pasadas descrito anteriormente.

3. **Generador de números aleatorios:**  
Se añadió una función `rand()` simple basada en Linear Congruential Generator:
```c
static unsigned long next_rand = 1;

int rand(void) {
  next_rand = next_rand * 1103515245 + 12345;
  return (unsigned int)(next_rand / 65536) % 32768;
}
```

**Archivo `kernel/sysproc.c`**  
Se implementaron las dos nuevas llamadas al sistema:

1. **`sys_settickets()`:**
```c
uint64
sys_settickets(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  
  if(n < 1)
    return -1;
  
  struct proc *p = myproc();
  acquire(&p->lock);
  p->tickets = n;
  release(&p->lock);
  
  return 0;
}
```
Esta función valida que `n >= 1` y protege la escritura con locks para evitar condiciones de carrera.

2. **`sys_get_slices()`:**
```c
uint64
sys_get_slices(void)
{
  return myproc()->run_slices;
}
```

**Archivos `kernel/syscall.h` y `kernel/syscall.c`**  
Se registraron las nuevas llamadas al sistema:

En `syscall.h`:
```c
#define SYS_settickets 22
#define SYS_get_slices 23
```

En `syscall.c`:
```c
extern uint64 sys_settickets(void);
extern uint64 sys_get_slices(void);

static uint64 (*syscalls[])(void) = {
  // ... syscalls existentes ...
  [SYS_settickets] sys_settickets,
  [SYS_get_slices] sys_get_slices,
};
```

### 2.2 Modificaciones en el espacio de usuario

**Archivo `user/usys.pl`**  
Se añadieron las entradas para generar los stubs en ensamblador:
```perl
entry("settickets");
entry("get_slices");
```

**Archivo `user/user.h`**  
Se agregaron los prototipos de las funciones:
```c
int settickets(int);
int get_slices(void);
```

**Archivo `Makefile`**  
Se añadió el programa de prueba a la lista de programas de usuario:
```make
UPROGS=\
  $U/_demo
```

### 2.3 Programa de prueba

Se creó `user/demo.c` para verificar la proporcionalidad del planificador:

```c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

long work_loop(void) {
  long j = 0;
  for (long i = 0; i < 1000000000; i++) {
    j += i % 7;
  }
  return j;
}

int main(void) {
  int pid;
  int tickets_list[10] = {50, 100, 150, 200, 250, 300, 350, 400, 450, 500};
  
  for (int i = 0; i < 10; i++) {
    pid = fork();
    if (pid == 0) {
      // Proceso hijo
      settickets(tickets_list[i]);
      long result = work_loop();
      int slices = get_slices();
      printf("Proceso %d: tickets=%d, slices=%d, work=%d\n", 
             getpid(), tickets_list[i], slices, result);
      exit(0);
    }
  }
  
  // Proceso padre espera a todos los hijos
  for (int i = 0; i < 10; i++) {
    wait(0);
  }
  
  exit(0);
}
```

El programa crea 10 procesos hijos con tickets proporcionales, de 50 a 500 y verifica que los `run_slices` obtenidos sean proporcionales a sus tickets.

---

## 3. Dificultades Encontradas y Soluciones

### 3.1 Error de enlazador (undefined reference)

**Problema:** Al compilar `demo.c`, el enlazador no encontraba las funciones `settickets` y `get_slices`:
```
undefined reference to `settickets'
undefined reference to `get_slices'
```

**Causa:** El archivo `user/usys.pl` mantiene una lista manual de syscalls para generar el código ensamblador. Las nuevas llamadas no estaban registradas en este archivo.

**Solución:** Se añadieron manualmente las entradas `entry("settickets");` y `entry("get_slices");` en `user/usys.pl`. Luego se ejecutó `make clean && make qemu` para regenerar el código.

### 3.2 Resultados de prueba incorrectos (no proporcionales)

**Problema:** El programa `demo` mostraba que todos los procesos recibían aproximadamente la misma cantidad de slices, que eran mas o menos 10, sin importar sus tickets. No se observaba la proporcionalidad esperada.

**Causa:** Se identificaron dos problemas:

1. **Optimización del compilador:** El compilador GCC detectaba que el `work_loop()` original (un bucle `for` vacío) no tenía efectos observables y lo eliminaba por completo del código.

2. **Bucle de trabajo muy corto:** Incluso desactivando optimizaciones, el bucle era tan corto que los procesos terminaban en 1-2 slices sin competir realmente por la CPU.

**Solución:**

1. Se modificó `work_loop()` para realizar un cálculo real:
```c
long work_loop(void) {
  long j = 0;
  for (long i = 0; i < 1000000000; i++) {
    j += i % 7;  // Operación que el compilador no puede eliminar
  }
  return j;
}
```

2. Se incrementó el número de iteraciones a 1,000,000,000 (mil millones) para forzar a los procesos a competir por CPU durante un tiempo significativo.

3. El resultado de `work_loop()` se imprime en `main`, obligando al compilador a ejecutar el bucle completo.

### 3.3 Condición de carrera en sistemas multiprocesador (SMP)

**Problema:** Al ejecutar el sistema con múltiples CPUs (`CPUS := 3` en el Makefile), los resultados volvían a ser planos, sin mostrar proporcionalidad.

**Causa:** La función `rand()` usa una variable estática global (`next_rand`) sin protección de sincronización. Cuando múltiples schedulers (uno por CPU) llaman a `rand()` simultáneamente, corrompen el estado del generador, produciendo números de baja calidad o patrones repetitivos.

**Solución temporal:** Se forzó a QEMU a ejecutarse en un solo núcleo modificando el Makefile:
```make
CPUS := 1
```

**Solución completa (no implementada):** Para un sistema SMP real, sería necesario:
- Proteger `next_rand` con un spinlock
- O usar generadores de números aleatorios por-CPU
- O implementar un generador lock-free

---

## 4. Análisis de Problemas del Lottery Scheduling

Aunque la planificación por lotería es una solución aceptable para el reparto proporcional, presenta varias desventajas teóricas y prácticas que la hacen menos ideal que otras alternativas.

### 4.1 Injusticia a corto plazo (naturaleza probabilística)

**Descripción:**  
Esta es la desventaja más significativa. El sistema es justo estadísticamente y converge a la proporcionalidad a largo plazo, pero no ofrece ninguna garantía a corto plazo.

**Ejemplo:**  
Un proceso con 1 ticket (baja prioridad) podría, por pura suerte, ganar la lotería 5 veces consecutivas, mientras que un proceso con 10,000 tickets (alta prioridad) podría no ser elegido, experimentando inanición temporal (starvation).

**Impacto:**  
Esto hace al Lottery Scheduling completamente inadecuado para sistemas de **tiempo real estricto (Hard Real-Time)**, donde el incumplimiento de un plazo es un fallo catastrófico. Un planificador determinista como Stride Scheduling resuelve este problema garantizando el reparto proporcional incluso en ventanas de tiempo muy cortas.

### 4.2 Inflación de tickets y dificultad de gestión

**Descripción:**  
El valor de un ticket no es absoluto, sino relativo al total de tickets en el sistema. Esto dificulta significativamente la gestión de prioridades.

**Ejemplo:**  
- **Estado inicial:** Proceso A (10 tickets) y B (100 tickets) compiten entre ellos. B obtiene el 90% de la CPU.
- **Nuevo estado:** Entra un proceso C con 1,000,000 tickets. Ahora A y B son efectivamente starved, cada uno recibiendo muy poco porcentaje de la CPU, un 0.001% de la CPU. Su proporción original 1:10 se vuelve irrelevante.

**Impacto:**  
Los procesos (o usuarios) no pueden asignar tickets de manera inteligente sin conocimiento global de todos los demás procesos activos. Este problema se conoce como **"inflación de tickets"** y requiere mecanismos adicionales de control y políticas administrativas.

### 4.3 Complejidad de la generación aleatoria en SMP

**Descripción:**  
Como se descubrió durante la implementación, generar buenos números aleatorios en un kernel no es trivial, especialmente en sistemas multiprocesador.

**Problemas identificados:**

1. **Estado compartido:** El generador requiere mantener un estado (`next_rand`). En un sistema multi-núcleo, este estado global debe ser protegido con locks para evitar condiciones de carrera.

2. **Contención:** Múltiples CPUs esperando por el mismo lock para generar números aleatorios crea contención, degradando significativamente el rendimiento de la planificación.

3. **Calidad del generador:** Un generador pseudoaleatorio de mala calidad puede exhibir patrones o ciclos cortos, llevando a una planificación sesgada e injusta.

**Soluciones posibles:**
- Usar generadores per-CPU con semillas diferentes
- Implementar generadores lock-free
- Usar fuentes de entropía de hardware cuando estén disponibles

### 4.4 Dificultad de análisis y predicción

**Descripción:**  
A diferencia de los planificadores deterministas, el comportamiento del Lottery Scheduling es inherentemente probabilístico, lo que dificulta:

- Predecir el comportamiento del sistema
- Debugging y reproducción de problemas
- Análisis de rendimiento (los resultados varían entre ejecuciones)
- Garantizar SLAs (Service Level Agreements)

**Impacto:**  
Para aplicaciones críticas o de caracter comerciales, la falta de determinismo puede ser inaceptable para estos casos.

---

## Conclusión

Se implementó exitosamente un planificador de lotería (Lottery Scheduling) en xv6-riscv, reemplazando el planificador Round-Robin original. La implementación demuestra el funcionamiento del reparto proporcional probabilístico y permite verificar empíricamente la proporcionalidad estadística del algoritmo.

Durante el desarrollo se identificaron y resolvieron problemas relacionados con la generación de código, optimización del compilador y sincronización en sistemas multiprocesador. El análisis teórico reveló limitaciones importantes del Lottery Scheduling, especialmente en términos de garantías a corto plazo, gestión de tickets y overhead del planificador.

Llegamos a la conlución que Lottery Scheduling funciona bien para ciertos escenarios, alternativas como Stride Scheduling o CFS ofrecen mejores garantías de equidad y menor overhead, haciéndolos más adecuados para sistemas de producción.
