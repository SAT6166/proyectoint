# Sistema de Gestión Financiera Personal

Una pequeña aplicación de escritorio para llevar la cuenta de tu dinero: anotas lo que entra y lo que sale, y ella te dice cuál es tu balance. Está escrita en C, con [Raylib](https://www.raylib.com) para la parte gráfica, y nació como Proyecto Integrador de programación.

Todo el programa está en un solo archivo, [`main.c`](main.c).

## ¿Para qué sirve?

En lo práctico, para saber en qué se te va el dinero. Al abrir la app ves tu balance, cuánto has ganado y cuánto has gastado. Una barra compara ingresos y gastos; si pasas el cursor por encima, te muestra los montos y porcentajes exactos.

Desde ahí puedes:

- **registrar un movimiento nuevo**: eliges si es ingreso o gasto y escribes el monto, la categoría, una descripción y la fecha (que ya viene con la de hoy);
- **revisar el historial completo** en una tabla que puedes desplazar y ordenar;
- **exportarlo a CSV** para abrirlo en Excel, guardándolo donde quieras.

Cada vez que haces algo, una notificación en la esquina te confirma que salió bien, o te explica qué falló. También hay un modo oscuro, y la app recuerda cuál prefieres.

## Cómo ponerlo en marcha

Solo necesitas un compilador de C de 64 bits. En Windows, lo más fácil es instalar WinLibs:

```bash
winget install BrechtSanders.WinLibs.POSIX.UCRT
```

Raylib ya viene incluido en la carpeta `raylib\`. Con eso, para compilar y abrir la aplicación basta con hacer doble clic en `ejecutar.bat`, o escribir en la terminal:

```bash
.\ejecutar.bat
```

Si vas a modificar el código, te conviene más el modo vigilante. El script se queda atento a `main.c` y, cada vez que guardas un cambio, recompila y vuelve a abrir la app por su cuenta:

```bash
.\ejecutar.bat -Vigilar
```

Y si solo quieres compilar, sin abrir nada, tienes `.\compilar.bat`.

### Por qué un script y no `gcc` a secas

Compilar a mano en Windows falla con facilidad. El script se encarga de lo siguiente:

- Si en tu PC hay más de un `gcc`, elige uno de 64 bits. El MinGW antiguo de `C:\MinGW` es de 32 bits y no funciona con Raylib.
- Si falta la carpeta `raylib\`, la descarga.
- Si la app sigue abierta, la cierra antes de reemplazar el ejecutable.
- Si cometes un error en el código, no borra el `financiero.exe` que ya funcionaba.

Si aun así prefieres compilar a mano, este es el comando (con el gcc de 64 bits):

```bash
gcc main.c -o financiero.exe -std=c99 -Wall -Iraylib\include -Lraylib\lib -lraylib -lopengl32 -lgdi32 -lwinmm
```

En **Linux**, instala Raylib con `sudo apt install libraylib-dev` y compila con:

```bash
gcc main.c -o financiero -std=c99 -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
```

En **macOS**, instálalo con `brew install raylib` y compila con:

```bash
gcc main.c -o financiero -std=c99 $(pkg-config --cflags --libs raylib)
```

## Algunos detalles de uso

En el formulario puedes moverte entre campos con `TAB`, guardar con `ENTER` y salir con `ESC`. `Ctrl+V` pega texto.

Al exportar, puedes indicar una carpeta (y el archivo se llamará `historial_financiero.csv`) o un nombre de archivo completo. Si olvidas la extensión `.csv`, la app la añade. Los botones de Escritorio y Documentos te ahorran escribir la ruta, y la próxima vez el diálogo recordará la última que usaste.

En el historial, la rueda del mouse o las flechas desplazan la tabla, y `ESC` te devuelve al inicio.

## Dónde se guardan tus datos

Todo queda en la carpeta del proyecto, en archivos de texto que puedes abrir con cualquier editor:

- `datos.txt` guarda tus transacciones, una por línea y con los campos separados por `|`. Se actualiza cada vez que registras algo, así que no pierdes nada aunque cierres la ventana de golpe.
- `config.txt` recuerda el tema elegido y la última ruta de exportación.

Los totales no se guardan: la app los recalcula cada vez que carga los datos, así que nunca pueden quedar desincronizados. Si alguna línea de `datos.txt` está mal escrita, se ignora y la app sigue funcionando.

## Un vistazo al código

`main.c` se lee de arriba abajo en este orden:

1. las estructuras de datos;
2. la lógica que guarda y calcula, que no sabe nada de la interfaz;
3. los componentes visuales reutilizables: botones, campos de texto, notificaciones;
4. las pantallas;
5. el bucle principal.

Raylib usa una **interfaz en modo inmediato**. No existe un "objeto botón" que viva en memoria: 60 veces por segundo se borra la pantalla y se dibuja todo de nuevo, y un botón es simplemente una función que se dibuja y te avisa si alguien le hizo clic en ese instante. Qué pantalla se dibuja lo decide un `switch` sobre una variable de estado.

Todas las copias de texto pasan por una función que nunca escribe más allá del tamaño del buffer, y las lecturas de archivo llevan límites explícitos.

## Si algo falla

Casi todos los problemas que han aparecido vienen de compilar con el `gcc` equivocado:

- Si ves errores como `undefined reference to 'InitWindow'`, se usó el MinGW de 32 bits.
- Si Windows dice que `financiero.exe` no existe, la compilación falló antes de crearlo.

En ambos casos, usar `ejecutar.bat` lo resuelve.

Si la app arranca pero no ves tus transacciones, probablemente la abriste desde otra carpeta y no encontró `datos.txt`. Ábrela siempre desde el proyecto; el script ya lo hace por ti.

---

La parte gráfica usa [Raylib](https://www.raylib.com) 5.5, de Ramon Santamaria, bajo licencia zlib (ver `raylib/LICENSE`).
