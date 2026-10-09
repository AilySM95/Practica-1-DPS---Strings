#!/usr/bin/env bash
# Pruebas de la Parte II: ejecutan el MISMO procedimiento sobre
# exampleStrings.c (original) y exampleStrings_fixed.c (corregido).
#
# Uso (desde cualquier carpeta):   bash tests/run_tests.sh
# Requiere: gcc con soporte de AddressSanitizer y UndefinedBehaviorSanitizer.
#
# Resultados:  tests/evidence/<caso>.<orig|fixed>.txt   (salida recortada)
#              tests/evidence/SUMMARY.txt                (tabla resumen)
#
# NOTA: el original NO compila en modo estricto (literal crudo R"..."), asi que
# se compila con -std=gnu (extension de GCC) y con los avisos silenciados (-w);
# los avisos del original ya estan documentados en el README (seccion 2).

cd "$(dirname "$0")/.." || exit 1
ROOT=$(pwd)
BUILD="$ROOT/tests/build"
EVID="$ROOT/tests/evidence"
rm -rf "$BUILD" "$EVID"
mkdir -p "$BUILD" "$EVID"

# --- Estandar: C23 si el gcc lo conoce; si no, su nombre antiguo c2x ---------
if echo 'int x;' | gcc -std=c23 -x c -fsyntax-only - 2>/dev/null; then
  STRICT=c23; GNU=gnu23
else
  STRICT=c2x; GNU=gnu2x
fi
echo "gcc: $(gcc --version | head -1)"
echo "Estandares usados: estricto=-std=$STRICT, con extensiones=-std=$GNU"

SAN="-g -O0 -fsanitize=address,undefined -fno-omit-frame-pointer"
# En gcc >= 14 algunas construcciones del original pasan a ser errores por
# defecto; se rebajan a aviso para poder ejecutarlo (en gcc 13 no hacen nada).
NOERR=""
for f in -Wno-error=implicit-function-declaration -Wno-error=return-mismatch -Wno-error=incompatible-pointer-types; do
  # solo se anade la opcion si ESTE gcc la reconoce (gcc 13 no conoce return-mismatch)
  if echo 'int x;' | gcc -x c -fsyntax-only "$f" - 2>/dev/null; then NOERR="$NOERR $f"; fi
done
LEGACY="-w $NOERR"   # -w silencia avisos: SOLO para ejecutar el original, nunca para analizarlo

# Dos binarios por version: __FILE__ sin barra ("exampleStrings.c") y con barra
# ("./exampleStrings.c"), porque el fallo de get_dirname solo ocurre si hay '/'.
gcc -std=$GNU $SAN $LEGACY exampleStrings.c        -o "$BUILD/orig_nosl"  || exit 1
gcc -std=$GNU $SAN $LEGACY ./exampleStrings.c      -o "$BUILD/orig_sl"    || exit 1
gcc -std=$STRICT $SAN -Wall -Wextra -Wpedantic exampleStrings_fixed.c   -o "$BUILD/fixed_nosl" || exit 1
gcc -std=$STRICT $SAN -Wall -Wextra -Wpedantic ./exampleStrings_fixed.c -o "$BUILD/fixed_sl"   || exit 1

export ASAN_OPTIONS="detect_leaks=0:abort_on_error=0:color=never"
export UBSAN_OPTIONS="print_stacktrace=0:color=never"

SUMMARY="$EVID/SUMMARY.txt"
printf '%-34s | %-28s | %-28s\n' "CASO" "ORIGINAL" "CORREGIDO" > "$SUMMARY"

# run <id> <orig|fixed> <binario> <stdin(printf)> [args...]
run() {
  local id=$1 lbl=$2 bin=$3 input=$4; shift 4
  local out="$EVID/$id.$lbl.txt" raw
  raw=$(mktemp)
  printf "$input" | timeout 10 "$bin" "$@" > "$raw" 2>&1
  local code=$?
  {
    echo "# caso: $id  ($lbl)"
    echo "# comando: printf '$input' | $(basename "$bin") $*"
    echo "# codigo de salida: $code"
    echo "# ---- salida (primeras 14 lineas) ----"
    head -n 14 "$raw"
  } > "$out"
  # veredicto de una linea para la tabla resumen
  local v
  if   grep -q "AddressSanitizer: stack-buffer-overflow" "$raw"; then v="ASan: stack-buffer-overflow"
  elif grep -q "AddressSanitizer: stack-buffer-underflow" "$raw"; then v="ASan: stack-buffer-underflow"
  elif grep -q "AddressSanitizer: SEGV" "$raw"; then v="ASan: SEGV (fallo de segmentacion)"
  elif grep -q "AddressSanitizer" "$raw"; then v="ASan: $(grep -m1 -o 'AddressSanitizer: [a-z-]*' "$raw")"
  elif grep -q "runtime error" "$raw"; then v="UBSan: $(grep -m1 -o 'runtime error: .*' "$raw" | cut -c1-40)"
  elif grep -q "stack smashing" "$raw"; then v="stack smashing detected"
  elif [ "$code" -eq 0 ]; then v="sin error (codigo 0)"
  else v="sin error de memoria (codigo $code)"
  fi
  echo "$v" > "$EVID/.$id.$lbl.verdict"
  rm -f "$raw"
}

# case <id> <binario_orig> <binario_fixed> <stdin> [args...]
case_() {
  local id=$1 bo=$2 bf=$3 input=$4; shift 4
  run "$id" orig  "$BUILD/$bo" "$input" "$@"
  run "$id" fixed "$BUILD/$bf" "$input" "$@"
  printf '%-34s | %-28s | %-28s\n' "$id" "$(cat "$EVID/.$id.orig.verdict")" "$(cat "$EVID/.$id.fixed.verdict")" >> "$SUMMARY"
}

LONG24=aaaaaaaaaaaaaaaaaaaaaaaa   # 24 letras: no cabe en key[24] junto con " = "

# T1  argv sin validar (EXP34-C)
case_ T1_sin_argumentos       orig_nosl fixed_nosl ''
# T2  desbordamiento de key[24] con strcpy/strcat (STR31-C, ARR38-C)
case_ T2_argumentos_largos    orig_nosl fixed_nosl 'x\ny\n' "$LONG24" "bbbbbbbbbbbbbb"
# T3  get_dirname modifica un literal cuando la ruta lleva '/' (EXP40-C, STR30-C)
case_ T3_ruta_con_barra       orig_sl   fixed_sl   'x\ny\n' clave valor
# T4  gets sin limite (MSC24-C, STR31-C): 2a linea de 40 letras
case_ T4_gets_linea_larga     orig_nosl fixed_nosl 'x\naaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n' clave valor
# T5  desincronizacion de lecturas: 1a linea larga y respuesta 'n' (ERR33-C, STR31-C)
case_ T5_fgets_linea_larga    orig_nosl fixed_nosl 'abcdefghij\nn\n' clave valor
# T6  caso normal (regresion): debe seguir funcionando igual en ambos
case_ T6_normal_regresion     orig_nosl fixed_nosl 'hola\ny\n' clave valor
# T7  entrada vacia (EOF): response sin inicializar (EXP33-C)
case_ T7_entrada_vacia        orig_nosl fixed_nosl '' clave valor

# T8  gets_example_func: indice (size_t)-1 cuando strlen(buf)==0 (FIO37-C, ARR30-C)
# Se compila cada version SIN su main (-Dmain=...) y se enlaza con test_gets.c
for v in orig fixed; do
  if [ $v = orig ]; then SRC=exampleStrings.c; STD=$GNU; FL="$LEGACY -DORIGINAL"; else SRC=exampleStrings_fixed.c; STD=$STRICT; FL=""; fi
  gcc -std=$STD $SAN $LEGACY -Dmain=programa_principal -c $SRC -o "$BUILD/$v.o" 2>/dev/null
  gcc -std=$GNU $SAN -w $FL tests/test_gets.c "$BUILD/$v.o" -o "$BUILD/gets_$v" 2>/dev/null
done
# entrada = un byte NUL: fgets lo guarda y strlen(buf) vale 0
run T8_entrada_con_NUL orig  "$BUILD/gets_orig"  '\0'
run T8_entrada_con_NUL fixed "$BUILD/gets_fixed" '\0'
printf '%-34s | %-28s | %-28s\n' T8_entrada_con_NUL "$(cat "$EVID/.T8_entrada_con_NUL.orig.verdict")" "$(cat "$EVID/.T8_entrada_con_NUL.fixed.verdict")" >> "$SUMMARY"

# T9  idioma de quitar '\n': borra una letra real si no hay '\n' (FIO37-C)
gcc -std=$STRICT $SAN -Wall -Wextra tests/test_newline.c -o "$BUILD/newline" && "$BUILD/newline" > "$EVID/T9_quitar_salto.txt" 2>&1
printf '%-34s | %s\n' T9_quitar_salto "ver tests/evidence/T9_quitar_salto.txt" >> "$SUMMARY"

# --- T10: Valgrind (analisis dinamico distinto de ASan) ------------------------
# Aisla la lectura de 'response' sin inicializar cuando gets/fgets fallan (EXP33-C):
# con stdin vacio el original lee response[0] sin haberlo escrito nunca.
if command -v valgrind >/dev/null 2>&1; then
  gcc -std=$GNU -g -O0 $LEGACY exampleStrings.c       -o "$BUILD/orig_vg"  2>/dev/null
  gcc -std=$STRICT -g -O0 -Wall exampleStrings_fixed.c -o "$BUILD/fixed_vg"
  for v in orig fixed; do
    { echo "# caso: T10_valgrind_stdin_vacio ($v)   comando: valgrind ./${v}_vg clave valor < /dev/null"
      valgrind --track-origins=yes -q "$BUILD/${v}_vg" clave valor < /dev/null 2>&1 \
        | grep -E "uninitialised|Invalid (read|write)|Process terminating|Address 0x|at 0x|by 0x" | cut -c1-120 | head -n 14
      echo "# (sin lineas ==pid== = valgrind no detecto nada)"; } > "$EVID/T10_valgrind_stdin_vacio.$v.txt"
  done
  printf '%-34s | %-28s | %-28s\n' T10_valgrind_stdin_vacio \
    "$(grep -c 'uninitialised' "$EVID/T10_valgrind_stdin_vacio.orig.txt") avisos 'uninitialised'" \
    "$(grep -c 'uninitialised' "$EVID/T10_valgrind_stdin_vacio.fixed.txt") avisos 'uninitialised'" >> "$SUMMARY"
else
  echo "(valgrind no instalado: se omite T10)"
fi

# --- Analisis estatico ---------------------------------------------------------
{
  echo "== gcc -fanalyzer, original (-std=$GNU; SIN -w para no ocultar avisos) =="
  gcc -std=$GNU -fanalyzer -Wall -Wextra $NOERR -c exampleStrings.c -o /dev/null 2>&1 | grep -E "Wanalyzer" | head -n 12
  echo "(lineas con Wanalyzer arriba; ninguna = el analizador no detecto nada)"
  echo
  echo "== gcc -fanalyzer, corregido (-std=$STRICT) =="
  gcc -std=$STRICT -fanalyzer -Wall -Wextra -Wpedantic -c exampleStrings_fixed.c -o /dev/null 2>&1 | grep -E "warning|error" | head -n 12
  echo "(ninguna linea = sin avisos)"
  if command -v clang-tidy >/dev/null 2>&1; then
    CT='-*,cert-*,bugprone-not-null-terminated-result,clang-analyzer-security.*'
    echo
    echo "== clang-tidy (cert-*, clang-analyzer-security.*), original (-std=$GNU) =="
    clang-tidy exampleStrings.c -checks="$CT" -- -std=$GNU 2>&1 | grep -E "warning:|error:" | sed 's|^.*/||' | cut -c1-150 | head -n 12
    echo
    echo "== clang-tidy (cert-*, clang-analyzer-security.*), corregido (-std=$STRICT) =="
    clang-tidy exampleStrings_fixed.c -checks="$CT" -- -std=$STRICT 2>&1 | grep -E "warning:|error:" | sed 's|^.*/||' | cut -c1-150 | head -n 12
    echo "(ninguna linea = sin avisos)"
  fi
} > "$EVID/ANALISIS_ESTATICO.txt"

rm -f "$EVID"/.*.verdict
echo; cat "$SUMMARY"; echo; echo "Evidencias en: tests/evidence/"
