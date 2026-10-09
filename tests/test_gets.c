/* Test de gets_example_func (FIO37-C / ARR30-C).
 * Se enlaza con el original (-DORIGINAL, devuelve void) o con el corregido
 * (devuelve int). Lee de stdin: run_tests.sh le envia un unico byte NUL, de
 * modo que fgets guarda "" y strlen(buf) vale 0.
 *   original : buf[strlen(buf) - 1] -> indice (size_t)-1 -> escritura fuera del array
 *   corregido: buf[strcspn(buf, "\n")] -> buf[0] -> sin problema
 */
#include <stdio.h>

#ifdef ORIGINAL
void gets_example_func(void);
#else
int gets_example_func(void);
#endif

int main(void) {
#ifdef ORIGINAL
  gets_example_func();
  puts("gets_example_func termino (el original es void: no informa de errores)");
#else
  int r = gets_example_func();
  printf("gets_example_func devolvio %d\n", r);
#endif
  return 0;
}
