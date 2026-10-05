/**
 * exampleStrings_fixed.c  -  Version corregida de exampleStrings.c
 *
 * Entorno de referencia: gcc con el estandar C23.
 *
 * Compilacion (sin errores ni warnings):
 *   gcc -std=c23 -Wall -Wextra -Wpedantic exampleStrings_fixed.c -o exampleStrings_fixed
 *
 * Nota: si el gcc es anterior a la version 14, la opcion -std=c23 no existe
 * y hay que escribir -std=c2x (es el mismo estandar con su nombre antiguo).
 *
 * Los comentarios marcados con [CAMBIO] explican cada modificacion respecto
 * al original, el problema que habia y la regla/recomendacion de SEI CERT C
 * relacionada. Los identificadores CERT son orientativos: hay que contrastar
 * su enunciado en la pagina oficial de SEI CERT C antes de usarlos en el README.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------------- */
/* Cadenas globales                                                          */
/* ------------------------------------------------------------------------- */

/*
 * [SIN CAMBIO DE LOGICA] array1 y array2 ya eran correctos:
 *  - array1 deja los corchetes vacios, asi el compilador calcula el tamano
 *    (7 = 6 letras + '\0'). CERT STR11-C.
 *  - array2 incluye el '\0' final a mano. CERT STR32-C.
 * [CAMBIO] Se anade "static const": son datos de solo lectura y solo se usan
 * en este archivo.
 */
static const char array1[] = "Foo" "bar";
static const char array2[] = { 'F', 'o', 'o', 'b', 'a', 'r', '\0' };

enum { BUFFER_MAX_SIZE = 1024 }; /* Constante simbolica. CERT DCL06-C. */

/*
 * [CAMBIO] El original usaba un literal crudo (raw string) R"foo( ... )foo".
 * Eso es sintaxis de C++11, NO existe en C ni en C23, y no compila con
 * -std=c23. Se sustituye por un literal normal de C con \n.
 * Referencias: MSC00-C (compilar sin warnings) y MSC14-C (no depender de
 * caracteristicas de otra plataforma/lenguaje).
 * [CAMBIO] "static const char *const": el literal es de solo lectura (CERT
 * STR05-C) y ademas el propio puntero no cambia.
 */
static const char *const s1 = "\nHello\nWorld\n";
static const char *const s2 = "\nHello\nWorld\n";

/* ------------------------------------------------------------------------- */
/* Funciones auxiliares nuevas                                               */
/* ------------------------------------------------------------------------- */

/*
 * [NUEVO] read_line: lee UNA linea de forma segura desde stdin.
 *  - Nunca escribe mas de "size" bytes en buf (CERT STR31-C).
 *  - Siempre deja buf como una cadena terminada en '\0' (CERT STR32-C),
 *    incluso si fgets falla (CERT FIO40-C).
 *  - Quita el '\n' final sin suponer que existe (CERT FIO37-C, ARR30-C).
 *  - Si la linea era mas larga que el buffer, descarta el resto de la linea
 *    para que no se mezcle con la siguiente lectura.
 *  - "int c" (y no "char c") para distinguir EOF de un caracter. CERT FIO34-C.
 * Devuelve 0 si leyo algo y -1 si hubo error o fin de entrada. CERT ERR33-C.
 */
static int read_line(char *buf, int size) {
  if (buf == NULL || size < 1) {
    return -1;
  }

  if (fgets(buf, size, stdin) == NULL) {
    buf[0] = '\0'; /* FIO40-C: no dejar contenido indeterminado */
    return -1;
  }

  char *newline = strchr(buf, '\n');
  if (newline != NULL) {
    *newline = '\0';
  } else {
    int c;
    do {
      c = getchar();
    } while (c != '\n' && c != EOF);
  }
  return 0;
}

/*
 * [NUEVO] copy_string: copia src en dst sin pasarse del tamano de dst.
 * snprintf SIEMPRE termina en '\0' (a diferencia de strncpy) y permite saber
 * si hubo truncamiento. Devuelve 0 si cupo todo y -1 si se trunco o hubo error.
 * CERT STR31-C, STR32-C, STR03-C (no truncar sin darse cuenta).
 */
static int copy_string(char *dst, size_t dst_size, const char *src) {
  int n = snprintf(dst, dst_size, "%s", src);
  if (n < 0 || (size_t)n >= dst_size) {
    return -1;
  }
  return 0;
}

/* ------------------------------------------------------------------------- */
/* gets_example_func                                                         */
/* ------------------------------------------------------------------------- */

/*
 * [CAMBIO 1] Era "void" pero hacia "return 1;". Un return con valor en una
 * funcion void no es valido en C (violacion del estandar, el compilador lo
 * rechaza o avisa segun la version). Ahora devuelve int: 0 = bien, 1 = fallo.
 * Referencias: MSC00-C (compilar limpio) y ERR00-C (politica de errores
 * coherente: asi quien llama puede saber si fallo).
 *
 * [CAMBIO 2] buf[strlen(buf) - 1] = '\0' suponia que la ultima letra siempre
 * es '\n'. Si no lo es, borra una letra real; y si strlen(buf) es 0, el indice
 * "0 - 1" es un size_t gigantesco y se escribe fuera del array.
 * Ahora se usa strcspn, que busca el '\n' y, si no existe, no daña nada.
 * CERT FIO37-C y ARR30-C.
 *
 * Esta funcion no se llama desde main (igual que en el original); se deja con
 * enlace externo para poder probarla desde un archivo de tests (Parte II).
 */
int gets_example_func(void) {
  char buf[BUFFER_MAX_SIZE];

  if (fgets(buf, sizeof(buf), stdin) == NULL) {
    return 1;
  }
  buf[strcspn(buf, "\n")] = '\0';
  return 0;
}

/* ------------------------------------------------------------------------- */
/* get_dirname                                                               */
/* ------------------------------------------------------------------------- */

/*
 * [CAMBIO] El original recibia un "const char *", pero escribia '\0' dentro
 * de el para "cortar" la ruta. Eso modifica un objeto const (CERT EXP40-C) y,
 * como en main se llamaba con __FILE__ (un literal de solo lectura), era
 * ademas comportamiento indefinido (CERT STR30-C).
 *
 * Nueva interfaz (Solucion B, refactorizacion): la funcion NO toca la ruta
 * original; copia la parte de la carpeta en un buffer que pasa quien llama,
 * comprobando que cabe (STR31-C) y devolviendo un codigo de error (ERR00-C).
 *
 * El resultado es el MISMO que daba el original (solo cambia como se obtiene):
 *   "carpeta/archivo.c"  ->  "carpeta"
 *   "archivo.c"          ->  "archivo.c"  (sin barra: ruta completa, igual
 *                                          que el original)
 *   "/archivo.c"         ->  ""           (barra al inicio: cadena vacia,
 *                                          igual que el original)
 *
 * Devuelve 0 si todo fue bien y -1 si algun argumento es invalido o la
 * carpeta no cabe en dirname (en ese caso dirname queda como cadena vacia).
 */
int get_dirname(const char *pathname, char *dirname, size_t dirname_size) {
  if (pathname == NULL || dirname == NULL || dirname_size == 0) {
    return -1; /* EXP34-C: no desreferenciar punteros nulos */
  }

  const char *slash = strrchr(pathname, '/');
  size_t length;

  if (slash == NULL) {
    length = strlen(pathname); /* sin barra: se devuelve la ruta completa */
  } else {
    length = (size_t)(slash - pathname); /* todo lo anterior a la ultima barra */
  }

  if (length >= dirname_size) { /* hace falta sitio para el '\0' */
    dirname[0] = '\0';
    return -1;
  }

  memcpy(dirname, pathname, length);
  dirname[length] = '\0';
  return 0;
}

/* ------------------------------------------------------------------------- */
/* get_y_or_n                                                                */
/* ------------------------------------------------------------------------- */

/*
 * [CAMBIO] El original usaba gets(response). gets no permite indicar el
 * tamano, asi que cualquier linea de 8 caracteres o mas desborda response.
 * Por eso fue ELIMINADA en C11 (y no existe en C23): con -std=c23 el
 * compilador ya no la declara y da error.
 * Ahora se usa read_line (basada en fgets) con el tamano real del array.
 * CERT MSC24-C (no usar funciones obsoletas), STR31-C, ERR33-C.
 *
 * [CAMBIO] Si la lectura falla, el original dejaba response sin inicializar
 * y luego leia response[0] (CERT EXP33-C). Ahora read_line deja response[0]
 * en '\0', asi que se toma la opcion por defecto: continuar ("[y]").
 *
 * [CAMBIO] fflush(stdout) para que el mensaje aparezca antes de esperar la
 * respuesta aunque no termine en salto de linea.
 * [CAMBIO] exit(EXIT_SUCCESS) en lugar de exit(0): mismo valor, mas claro.
 */
void get_y_or_n(void) {
  char response[8];

  printf("Continue? [y] n: ");
  fflush(stdout);

  (void)read_line(response, (int)sizeof response); /* si falla: response[0] == '\0' */

  if (response[0] == 'n') {
    exit(EXIT_SUCCESS);
  }
}

/* ------------------------------------------------------------------------- */
/* main                                                                      */
/* ------------------------------------------------------------------------- */

int main(int argc, char *argv[]) {
  char key[24];
  char response[8];
  char array5[] = "01234567890123456"; /* 17 letras + '\0' = 18 bytes */

  /*
   * [CAMBIO] array3 y array4 median 16 bytes, menos que array5 (18). Con
   * strncpy eso dejaba array3 SIN '\0' y despues strlen(array3) leia fuera
   * del array. Ahora los destinos se dimensionan a partir de array5, de modo
   * que siempre cabe el texto completo + '\0' (STR31-C, STR32-C, STR03-C), y
   * ademas se copia con copy_string, que comprueba el tamano.
   */
  char array3[sizeof array5];
  char array4[sizeof array5];

  /*
   * [CAMBIO] ptr_char era "char *" apuntando a un literal y luego se hacia
   * ptr_char[0] = 'N' (modificar un literal = comportamiento indefinido,
   * STR30-C; ademas incumple STR05-C). Ahora es un ARRAY con copia propia del
   * texto, que si se puede modificar.
   */
  char new_string[] = "new string literal";

  /*
   * [CAMBIO] El texto en cirilico "аналитик" tiene 8 letras pero ocupa 16
   * bytes en UTF-8: strlen cuenta BYTES, no letras. Cabe de sobra en 100.
   * size_array1 pasa de int a size_t (strlen devuelve size_t; INT31-C).
   * Se elimina size_array2 (no se usaba). Las dos lineas comentadas del
   * original (arrays de tamano variable con inicializador) tampoco son
   * validas en C, por eso se quitan.
   * Se imprime el tamano para que las variables se usen (sin warnings).
   */
  char analitic3[100] = "аналитик";
  size_t size_array1 = strlen(analitic3);

  /*
   * [CAMBIO] Se pasa la ruta de __FILE__ a la nueva get_dirname, que ya no
   * modifica el literal. Se comprueba el resultado (ERR33-C / ERR00-C).
   */
  char dirname_buf[256];
  if (get_dirname(__FILE__, dirname_buf, sizeof dirname_buf) != 0) {
    fprintf(stderr, "Error: no se pudo obtener la carpeta de la ruta del archivo.\n");
    return EXIT_FAILURE;
  }
  puts(dirname_buf);

  /*
   * [CAMBIO] El original usaba argv[1] y argv[2] sin mirar argc: sin
   * argumentos argv[1] es NULL y strcpy lo desreferencia (EXP34-C). Ahora se
   * valida argc.
   */
  if (argc < 3) {
    fprintf(stderr, "Uso: %s <clave> <valor>\n", argv[0]);
    return EXIT_FAILURE;
  }

  /*
   * [CAMBIO] strcpy + strcat + strcat no comprobaban el tamano de key (24
   * bytes): con argumentos largos se desbordaba (STR31-C, ARR38-C).
   * Ahora un unico snprintf con el tamano real, y si no cabe se rechaza en
   * lugar de truncar en silencio (STR03-C).
   */
  int written = snprintf(key, sizeof key, "%s = %s", argv[1], argv[2]);
  if (written < 0 || (size_t)written >= sizeof key) {
    fprintf(stderr, "Error: los argumentos no caben en key (maximo %zu bytes).\n", sizeof key);
    return EXIT_FAILURE;
  }

  /*
   * [CAMBIO] fgets sin comprobar el resultado (ERR33-C) y sin tratar las
   * lineas largas. Ahora se usa read_line.
   */
  if (read_line(response, (int)sizeof response) != 0) {
    fprintf(stderr, "Aviso: no se pudo leer la entrada; se continua.\n");
  }

  get_y_or_n();

  printf("%s", array1);
  printf("\n");
  printf("%s", array2);
  printf("\n");

  puts(s1);
  printf("\n");
  puts(s2);
  printf("\n");

  printf("Bytes de analitic3: %zu\n", size_array1);

  /*
   * [CAMBIO] strncpy(array3, array5, sizeof(array3)) no ponia '\0' cuando el
   * origen era mas largo que el destino, y strncpy(array4, array3,
   * strlen(array3)) usaba el tamano del ORIGEN y no el del destino.
   * STR32-C, STR31-C, STR03-C, STR07-C. Ahora se usa copy_string con el
   * tamano del DESTINO y se comprueba el resultado.
   */
  if (copy_string(array3, sizeof array3, array5) != 0) {
    fprintf(stderr, "Error: array3 es demasiado pequeno.\n");
    return EXIT_FAILURE;
  }
  if (copy_string(array4, sizeof array4, array3) != 0) {
    fprintf(stderr, "Error: array4 es demasiado pequeno.\n");
    return EXIT_FAILURE;
  }

  array5[0] = 'M';      /* Correcto: array5 es una copia propia y modificable */
  new_string[0] = 'N';  /* Correcto ahora: ya no es un literal de solo lectura */

  /*
   * [CAMBIO] Se imprimen los dos textos modificados. Sin esto, gcc avisa
   * "variable 'new_string' set but not used" (-Wunused-but-set-variable),
   * porque solo se escribia en ella y nunca se leia.
   */
  printf("%s\n%s\n", array5, new_string);

  /*
   * [CAMBIO] Se elimina array3[sizeof(array3)-1] = '\0'. Estaba DESPUES de
   * usar array3 (demasiado tarde) y pisaba la ultima letra. Ya no hace falta:
   * snprintf deja siempre la cadena terminada.
   */

  return EXIT_SUCCESS;
}

