#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define NPROCS 10 

long long
work_loop() {
    volatile long long i;
    long long j = 0; 

    for (i = 0; i < 1000000000; i++) {
        j += (i % 2) + 1; 
    }
    return j;
}

int
main(int argc, char *argv[])
{
    int i;
    int pid;

    printf("Iniciando prueba de Lottery Scheduler con %d procesos...\n", NPROCS);

    for (i = 0; i < NPROCS; i++) {
        pid = fork();
        
        if (pid < 0) { 
            printf("fork falló\n");
            exit(-1);
        } 
        
        if (pid == 0) { 
            
            int tickets = 50 * (i + 1);
            
            // Llamar a la syscall settickets
            if (settickets(tickets) < 0) {
                printf("Error: settickets no pudo asignar %d tickets\n", tickets);
                exit(-1);
            }

            // Ejecutar y almacenar el resultado
            long long work_result = work_loop();

            // Obtener el resultado de la contabilidad
            int slices = get_slices();

            // Imprimir el resultado
            printf("Hijo %d (PID %d): %d tickets -> %d slices (sum=%d)\n", i, getpid(), tickets, slices, (int)work_result);

            exit(0); 
        }
    }

    for (i = 0; i < NPROCS; i++) {
        wait(0);
    }
    
    printf("Prueba completada.\n");
    exit(0);
}
