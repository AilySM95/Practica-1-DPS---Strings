/* Test de la tecnica para quitar el '\n' final (FIO37-C).
 * OJO: esto prueba la TECNICA aplicada a un buffer propio, no el codigo original
 * (en el original, buf es local y no se puede observar desde fuera). Se simulan
 * las dos entradas posibles de fgets: con '\n' y sin '\n' (linea sin Enter final).
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
  char buf[16];

  /* ORIGINAL: buf[strlen(buf) - 1] = '\0' */
  strcpy(buf, "hola\n");  buf[strlen(buf) - 1] = '\0';
  printf("original, con \\n   : \"%s\"\n", buf);            /* hola   (bien)    */
  strcpy(buf, "hola");    buf[strlen(buf) - 1] = '\0';
  printf("original, sin \\n   : \"%s\"  <- perdio una letra\n", buf); /* hol  */
  assert(strcmp(buf, "hol") == 0);

  /* CORREGIDO: buf[strcspn(buf, "\n")] = '\0' */
  strcpy(buf, "hola\n");  buf[strcspn(buf, "\n")] = '\0';
  printf("corregido, con \\n : \"%s\"\n", buf);
  assert(strcmp(buf, "hola") == 0);
  strcpy(buf, "hola");    buf[strcspn(buf, "\n")] = '\0';
  printf("corregido, sin \\n : \"%s\"\n", buf);
  assert(strcmp(buf, "hola") == 0);
  strcpy(buf, "");        buf[strcspn(buf, "\n")] = '\0';
  assert(buf[0] == '\0');
  puts("test_newline OK");
  return 0;
}
