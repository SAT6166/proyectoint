/**
 * Sistema de Gestión Financiera Personal.
 * Registro de ingresos y gastos con GUI en Raylib, persistencia en datos.txt
 * y exportación a CSV. Instrucciones de compilación al final del archivo.
 */

#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

/* ========================================================================
 *  Constantes, temas y tipos
 * ===================================================================== */

#define MAX_TRANSACCIONES 100
#define MAX_TEXTO 50
#define MAX_RUTA 260

#define ANCHO_VENTANA 800
#define ALTO_VENTANA  600

#define ARCHIVO_DATOS  "datos.txt"
#define ARCHIVO_CONFIG "config.txt"
#define NOMBRE_CSV_POR_DEFECTO "historial_financiero.csv"
#define MAX_LINEA 512

#define DURACION_NOTIFICACION 3.0f

#ifdef _WIN32
    #define SEPARADOR_TXT "\\"
#else
    #define SEPARADOR_TXT "/"
#endif

/* Colores de acento, comunes a ambos temas */
#define COLOR_VERDE   (Color){  39, 174,  96, 255 }
#define COLOR_ROJO    (Color){ 214,  69,  65, 255 }
#define COLOR_AZUL    (Color){  41, 128, 185, 255 }
#define COLOR_MORADO  (Color){ 142,  68, 173, 255 }
#define COLOR_GRIS    (Color){ 127, 140, 141, 255 }
#define COLOR_PIZARRA (Color){  52,  73,  94, 255 }
#define COLOR_AMBAR   (Color){ 230, 145,  20, 255 }
#define COLOR_ACERO   (Color){  69,  90, 120, 255 }

/**
 * Paleta de colores por rol de la interfaz.
 * Cambiar de tema consiste en reasignar el puntero global `tema`.
 */
typedef struct {
    Color fondo;
    Color superficie;
    Color superficie_alt;
    Color encabezado;
    Color cabecera_tabla;
    Color texto;
    Color texto_secundario;
    Color borde;
    Color placeholder;
    Color campo;
    Color tooltip;
} Tema;

static const Tema TEMA_CLARO = {
    .fondo            = { 236, 240, 245, 255 },
    .superficie       = { 255, 255, 255, 255 },
    .superficie_alt   = { 245, 247, 250, 255 },
    .encabezado       = {  33,  47,  74, 255 },
    .cabecera_tabla   = { 127, 140, 141, 255 },
    .texto            = {  44,  62,  80, 255 },
    .texto_secundario = { 127, 140, 141, 255 },
    .borde            = { 210, 215, 220, 255 },
    .placeholder      = { 175, 182, 190, 255 },
    .campo            = { 255, 255, 255, 255 },
    .tooltip          = {  33,  47,  74, 255 },
};

static const Tema TEMA_OSCURO = {
    .fondo            = {  22,  25,  32, 255 },
    .superficie       = {  34,  38,  48, 255 },
    .superficie_alt   = {  42,  47,  59, 255 },
    .encabezado       = {  14,  16,  22, 255 },
    .cabecera_tabla   = {  52,  58,  72, 255 },
    .texto            = { 228, 232, 238, 255 },
    .texto_secundario = { 150, 158, 172, 255 },
    .borde            = {  64,  70,  86, 255 },
    .placeholder      = {  98, 105, 120, 255 },
    .campo            = {  28,  31,  40, 255 },
    .tooltip          = {  60,  66,  84, 255 },
};

static const Tema *tema = &TEMA_CLARO;

/* Desactiva hover y clics de los componentes (modal abierto o cierre en curso) */
static bool ui_bloqueada = false;

typedef enum { INGRESO, GASTO } TipoTransaccion;
typedef enum { PANTALLA_DASHBOARD, PANTALLA_FORMULARIO, PANTALLA_HISTORIAL } PantallaActual;

/** Movimiento financiero individual. La fecha usa el formato "DD/MM/AAAA". */
typedef struct {
    int id;
    TipoTransaccion tipo;
    float monto;
    char categoria[MAX_TEXTO];
    char descripcion[MAX_TEXTO];
    char fecha[11];
} Transaccion;

/** Conjunto de transacciones y sus totales derivados (ver recalcular_balance). */
typedef struct {
    float balance_total;
    float total_ingresos;
    float total_gastos;
    Transaccion transacciones[MAX_TRANSACCIONES];
    int cantidad_transacciones;
} SistemaFinanciero;

typedef enum { FILTRO_TEXTO, FILTRO_NUMERO, FILTRO_FECHA, FILTRO_RUTA } FiltroCampo;

/**
 * Campo de texto editable con filtro de caracteres.
 * Una etiqueta vacía indica que el campo se dibuja sin etiqueta.
 */
typedef struct {
    char etiqueta[MAX_TEXTO];
    char ayuda[MAX_TEXTO];
    char texto[MAX_RUTA];
    int longitud_maxima;
    FiltroCampo filtro;
    Rectangle rect;
} CampoTexto;

enum { CAMPO_MONTO, CAMPO_CATEGORIA, CAMPO_DESCRIPCION, CAMPO_FECHA, NUM_CAMPOS };

/** Estado del formulario de nuevo registro. campo_activo = -1 si ninguno. */
typedef struct {
    TipoTransaccion tipo;
    CampoTexto campos[NUM_CAMPOS];
    int campo_activo;
    char mensaje_error[100];
} EstadoFormulario;

/** Estado del diálogo modal para elegir la ruta del CSV. */
typedef struct {
    bool abierto;
    CampoTexto ruta;
    char error[160];
} DialogoExportar;

typedef enum { NOTIF_EXITO, NOTIF_INFO, NOTIF_ERROR } TipoNotificacion;

/** Notificación temporal ("toast"). Visible mientras tiempo_restante > 0. */
typedef struct {
    TipoNotificacion tipo;
    char titulo[80];
    char mensaje[MAX_RUTA + 40];
    float tiempo_restante;
} Notificacion;

/** Tooltip que sigue al cursor. Se reinicia en cada frame. */
typedef struct {
    bool visible;
    Color color;
    char titulo[80];
    char detalle[120];
} Tooltip;

/** Estado global de la interfaz: pantalla activa, preferencias y capas superpuestas. */
typedef struct {
    PantallaActual pantalla;
    bool salir;
    bool cerrando;
    float tiempo_cierre;
    bool modo_oscuro;
    char ultima_ruta_csv[MAX_RUTA];
    Notificacion notificacion;
    Tooltip tooltip;
    EstadoFormulario formulario;
    DialogoExportar dialogo_exportar;
    float scroll_historial;
    bool recientes_primero;
} EstadoApp;

void cargar_datos(SistemaFinanciero *sistema);
bool guardar_datos(const SistemaFinanciero *sistema);
void recalcular_balance(SistemaFinanciero *sistema);
bool agregar_transaccion(SistemaFinanciero *sistema, TipoTransaccion tipo, float monto,
                         const char *cat, const char *desc, const char *fecha);
bool exportar_csv(const SistemaFinanciero *sistema, const char *ruta);


/* ========================================================================
 *  Backend: lógica de negocio y archivos
 * ===================================================================== */

/**
 * Copia una cadena sin exceder el buffer destino y siempre termina en '\0'.
 * @param destino Buffer de salida.
 * @param origen  Cadena a copiar; NULL se trata como cadena vacía.
 * @param tam     Tamaño total de `destino` en bytes.
 */
static void copiar_texto_seguro(char *destino, const char *origen, size_t tam)
{
    if (tam == 0) return;
    if (origen == NULL) origen = "";

    size_t len = 0;
    while (len < tam - 1 && origen[len] != '\0') len++;

    memmove(destino, origen, len);
    destino[len] = '\0';
}

/**
 * Elimina los espacios al inicio y al final de la cadena, en el mismo buffer.
 * @param s Cadena a modificar.
 */
static void recortar_espacios(char *s)
{
    size_t inicio = 0;
    size_t len = strlen(s);

    while (s[inicio] == ' ') inicio++;
    while (len > inicio && s[len - 1] == ' ') len--;

    memmove(s, s + inicio, len - inicio);
    s[len - inicio] = '\0';
}

/**
 * Reemplaza por espacios los caracteres que romperían el formato de datos.txt.
 * @param s Cadena a modificar.
 */
static void limpiar_separadores(char *s)
{
    for (; *s != '\0'; s++) {
        if (*s == '|' || *s == '\n' || *s == '\r') *s = ' ';
    }
}

/**
 * Deja el sistema vacío, con totales en cero y buffers limpios.
 * @param sistema Sistema a reiniciar.
 */
static void inicializar_sistema(SistemaFinanciero *sistema)
{
    memset(sistema, 0, sizeof(*sistema));
}

/**
 * Recalcula ingresos, gastos y balance a partir de todas las transacciones.
 * @param sistema Sistema a actualizar.
 */
void recalcular_balance(SistemaFinanciero *sistema)
{
    sistema->total_ingresos = 0.0f;
    sistema->total_gastos = 0.0f;

    for (int i = 0; i < sistema->cantidad_transacciones; i++) {
        const Transaccion *t = &sistema->transacciones[i];
        if (t->tipo == INGRESO) {
            sistema->total_ingresos += t->monto;
        } else {
            sistema->total_gastos += t->monto;
        }
    }

    sistema->balance_total = sistema->total_ingresos - sistema->total_gastos;
}

/**
 * Carga datos.txt (formato `id|tipo|monto|categoria|descripcion|fecha`).
 * Ignora comentarios '#' y líneas inválidas; si el archivo no existe, deja el sistema vacío.
 * @param sistema Sistema donde se cargan los datos.
 */
void cargar_datos(SistemaFinanciero *sistema)
{
    inicializar_sistema(sistema);

    FILE *archivo = fopen(ARCHIVO_DATOS, "r");
    if (archivo == NULL) {
        printf("[INFO] No se encontro '%s'. Se inicia con datos vacios.\n", ARCHIVO_DATOS);
        return;
    }

    char linea[MAX_LINEA];
    int numero_linea = 0;

    while (fgets(linea, sizeof(linea), archivo) != NULL) {
        numero_linea++;
        linea[strcspn(linea, "\r\n")] = '\0';

        if (linea[0] == '#' || linea[0] == '\0') continue;

        if (sistema->cantidad_transacciones >= MAX_TRANSACCIONES) {
            printf("[AVISO] Se alcanzo el limite de %d transacciones; el resto se ignora.\n",
                   MAX_TRANSACCIONES);
            break;
        }

        Transaccion t;
        memset(&t, 0, sizeof(t));
        int tipo_leido = 0;

        /* %49[^|] limita cada texto a MAX_TEXTO - 1 caracteres */
        int leidos = sscanf(linea, "%d|%d|%f|%49[^|]|%49[^|]|%10[^|]",
                            &t.id, &tipo_leido, &t.monto,
                            t.categoria, t.descripcion, t.fecha);

        if (leidos != 6 || (tipo_leido != INGRESO && tipo_leido != GASTO) || t.monto <= 0.0f) {
            printf("[AVISO] Linea %d de '%s' tiene formato invalido; se ignora.\n",
                   numero_linea, ARCHIVO_DATOS);
            continue;
        }

        t.tipo = (TipoTransaccion)tipo_leido;
        sistema->transacciones[sistema->cantidad_transacciones] = t;
        sistema->cantidad_transacciones++;
    }

    fclose(archivo);

    /* Los totales no se leen del archivo para que no puedan quedar desincronizados */
    recalcular_balance(sistema);
    printf("[INFO] Se cargaron %d transacciones desde '%s'.\n",
           sistema->cantidad_transacciones, ARCHIVO_DATOS);
}

/**
 * Sobrescribe datos.txt con un resumen comentado y todas las transacciones.
 * @param sistema Sistema a guardar.
 * @return true si el archivo se escribió correctamente.
 */
bool guardar_datos(const SistemaFinanciero *sistema)
{
    FILE *archivo = fopen(ARCHIVO_DATOS, "w");
    if (archivo == NULL) {
        perror("[ERROR] No se pudo abrir datos.txt para escribir");
        return false;
    }

    fprintf(archivo, "# Sistema de Gestion Financiera Personal\n");
    fprintf(archivo, "# Balance: %.2f | Ingresos: %.2f | Gastos: %.2f | Transacciones: %d\n",
            sistema->balance_total, sistema->total_ingresos,
            sistema->total_gastos, sistema->cantidad_transacciones);
    fprintf(archivo, "# Formato: id|tipo(0=INGRESO,1=GASTO)|monto|categoria|descripcion|fecha\n");

    for (int i = 0; i < sistema->cantidad_transacciones; i++) {
        const Transaccion *t = &sistema->transacciones[i];
        fprintf(archivo, "%d|%d|%.2f|%s|%s|%s\n",
                t->id, (int)t->tipo, t->monto, t->categoria, t->descripcion, t->fecha);
    }

    fclose(archivo);
    printf("[INFO] Datos guardados en '%s' (%d transacciones).\n",
           ARCHIVO_DATOS, sistema->cantidad_transacciones);
    return true;
}

/**
 * Añade una transacción con ID = mayor ID existente + 1.
 * Normaliza los textos y asigna valores por defecto a categoría y descripción vacías.
 * @param sistema Sistema destino.
 * @param tipo    INGRESO o GASTO.
 * @param monto   Importe, debe ser > 0.
 * @param cat     Categoría.
 * @param desc    Descripción.
 * @param fecha   Fecha en formato DD/MM/AAAA.
 * @return false si no hay espacio o el monto no es válido.
 */
bool agregar_transaccion(SistemaFinanciero *sistema, TipoTransaccion tipo, float monto,
                         const char *cat, const char *desc, const char *fecha)
{
    if (sistema->cantidad_transacciones >= MAX_TRANSACCIONES) return false;
    if (monto <= 0.0f) return false;

    int siguiente_id = 1;
    for (int i = 0; i < sistema->cantidad_transacciones; i++) {
        if (sistema->transacciones[i].id >= siguiente_id) {
            siguiente_id = sistema->transacciones[i].id + 1;
        }
    }

    Transaccion *t = &sistema->transacciones[sistema->cantidad_transacciones];
    memset(t, 0, sizeof(*t));

    t->id = siguiente_id;
    t->tipo = tipo;
    t->monto = monto;
    copiar_texto_seguro(t->categoria, cat, sizeof(t->categoria));
    copiar_texto_seguro(t->descripcion, desc, sizeof(t->descripcion));
    copiar_texto_seguro(t->fecha, fecha, sizeof(t->fecha));

    recortar_espacios(t->categoria);
    recortar_espacios(t->descripcion);
    limpiar_separadores(t->categoria);
    limpiar_separadores(t->descripcion);
    limpiar_separadores(t->fecha);

    /* Un campo vacío haría fallar el sscanf de cargar_datos */
    if (t->categoria[0] == '\0')   copiar_texto_seguro(t->categoria, "General", sizeof(t->categoria));
    if (t->descripcion[0] == '\0') copiar_texto_seguro(t->descripcion, "Sin descripcion", sizeof(t->descripcion));

    sistema->cantidad_transacciones++;
    return true;
}

/**
 * Escribe un campo CSV entre comillas, duplicando las comillas internas.
 * @param archivo Archivo abierto en escritura.
 * @param texto   Contenido del campo.
 */
static void escribir_campo_csv(FILE *archivo, const char *texto)
{
    fputc('"', archivo);
    for (const char *c = texto; *c != '\0'; c++) {
        if (*c == '"') fputc('"', archivo);
        fputc(*c, archivo);
    }
    fputc('"', archivo);
}

/**
 * Genera o sobrescribe un CSV con columnas ID,Tipo,Monto,Categoria,Descripcion,Fecha.
 * @param sistema Sistema a exportar.
 * @param ruta    Ruta completa del archivo de salida.
 * @return true si el archivo se creó correctamente.
 */
bool exportar_csv(const SistemaFinanciero *sistema, const char *ruta)
{
    FILE *archivo = fopen(ruta, "w");
    if (archivo == NULL) {
        perror("[ERROR] No se pudo crear el archivo CSV");
        return false;
    }

    fprintf(archivo, "ID,Tipo,Monto,Categoria,Descripcion,Fecha\n");

    for (int i = 0; i < sistema->cantidad_transacciones; i++) {
        const Transaccion *t = &sistema->transacciones[i];
        fprintf(archivo, "%d,%s,%.2f,", t->id, (t->tipo == INGRESO) ? "INGRESO" : "GASTO", t->monto);
        escribir_campo_csv(archivo, t->categoria);
        fputc(',', archivo);
        escribir_campo_csv(archivo, t->descripcion);
        fprintf(archivo, ",%s\n", t->fecha);
    }

    fclose(archivo);
    printf("[INFO] Historial exportado a '%s'.\n", ruta);
    return true;
}

/**
 * Normaliza la ruta del CSV: añade el nombre por defecto si es carpeta y ".csv" si falta.
 * Verifica que la carpeta contenedora exista.
 * @param entrada   Ruta escrita por el usuario.
 * @param salida    Buffer para la ruta final.
 * @param tam       Tamaño de `salida`.
 * @param error     Buffer para el motivo del fallo.
 * @param tam_error Tamaño de `error`.
 * @return true si la ruta es válida.
 */
static bool preparar_ruta_csv(const char *entrada, char *salida, size_t tam,
                              char *error, size_t tam_error)
{
    char ruta[MAX_RUTA];
    copiar_texto_seguro(ruta, entrada, sizeof(ruta));
    recortar_espacios(ruta);

    if (ruta[0] == '\0') {
        copiar_texto_seguro(error, "Escribe la ruta donde guardar el archivo", tam_error);
        return false;
    }

    size_t len = strlen(ruta);
    bool termina_en_separador = (ruta[len - 1] == '\\' || ruta[len - 1] == '/');

    if (termina_en_separador || DirectoryExists(ruta)) {
        size_t extra = strlen(NOMBRE_CSV_POR_DEFECTO) + (termina_en_separador ? 0 : 1);
        if (len + extra >= sizeof(ruta)) {
            copiar_texto_seguro(error, "La ruta es demasiado larga", tam_error);
            return false;
        }
        if (!termina_en_separador) strcat(ruta, SEPARADOR_TXT);
        strcat(ruta, NOMBRE_CSV_POR_DEFECTO);
    } else if (!IsFileExtension(ruta, ".csv")) {
        if (len + 4 >= sizeof(ruta)) {
            copiar_texto_seguro(error, "La ruta es demasiado larga", tam_error);
            return false;
        }
        strcat(ruta, ".csv");
    }

    const char *carpeta = GetDirectoryPath(ruta);
    if (carpeta[0] != '\0' && !DirectoryExists(carpeta)) {
        snprintf(error, tam_error, "La carpeta no existe: %s", carpeta);
        return false;
    }

    copiar_texto_seguro(salida, ruta, tam);
    return true;
}

/**
 * Busca una carpeta dentro del perfil del usuario probando varios nombres
 * (los nombres cambian según el idioma y si OneDrive redirige la carpeta).
 * @param candidatos Rutas relativas al perfil, en orden de preferencia.
 * @param cantidad   Número de candidatos.
 * @param destino    Buffer para la ruta encontrada.
 * @param tam        Tamaño de `destino`.
 * @return true si alguna carpeta existe.
 */
static bool buscar_carpeta_usuario(const char *const candidatos[], int cantidad,
                                   char *destino, size_t tam)
{
    const char *base = getenv("USERPROFILE");
    if (base == NULL || base[0] == '\0') base = getenv("HOME");
    if (base == NULL || base[0] == '\0') return false;

    for (int i = 0; i < cantidad; i++) {
        snprintf(destino, tam, "%s%s%s", base, SEPARADOR_TXT, candidatos[i]);
        if (DirectoryExists(destino)) return true;
    }
    return false;
}

/**
 * Lee el tema y la última ruta del CSV desde config.txt, si existe.
 * @param app Estado donde se aplican las preferencias.
 */
static void cargar_config(EstadoApp *app)
{
    FILE *archivo = fopen(ARCHIVO_CONFIG, "r");
    if (archivo == NULL) return;

    char linea[MAX_LINEA];
    while (fgets(linea, sizeof(linea), archivo) != NULL) {
        linea[strcspn(linea, "\r\n")] = '\0';

        if (strncmp(linea, "tema=", 5) == 0) {
            app->modo_oscuro = (strcmp(linea + 5, "oscuro") == 0);
        } else if (strncmp(linea, "ruta_csv=", 9) == 0) {
            copiar_texto_seguro(app->ultima_ruta_csv, linea + 9, sizeof(app->ultima_ruta_csv));
        }
    }
    fclose(archivo);
}

/**
 * Guarda el tema y la última ruta del CSV en config.txt.
 * @param app Estado con las preferencias actuales.
 */
static void guardar_config(const EstadoApp *app)
{
    FILE *archivo = fopen(ARCHIVO_CONFIG, "w");
    if (archivo == NULL) {
        perror("[ERROR] No se pudo guardar config.txt");
        return;
    }
    fprintf(archivo, "tema=%s\n", app->modo_oscuro ? "oscuro" : "claro");
    fprintf(archivo, "ruta_csv=%s\n", app->ultima_ruta_csv);
    fclose(archivo);
}

/**
 * Convierte un texto a monto y verifica que esté en el rango (0, 999999999].
 * @param texto     Texto a convertir.
 * @param resultado Valor convertido, solo se escribe si es válido.
 * @return true si el texto es un monto válido.
 */
static bool validar_monto(const char *texto, float *resultado)
{
    if (texto[0] == '\0') return false;

    char *fin = NULL;
    float valor = strtof(texto, &fin);

    /* Rechaza entradas parciales como "12abc" o "." */
    if (fin == texto || *fin != '\0') return false;
    if (!(valor > 0.0f) || valor > 999999999.0f) return false;

    *resultado = valor;
    return true;
}

/**
 * @param anio Año a evaluar.
 * @return true si el año es bisiesto.
 */
static bool es_bisiesto(int anio)
{
    return (anio % 4 == 0 && anio % 100 != 0) || (anio % 400 == 0);
}

/**
 * Valida el formato DD/MM/AAAA y que la fecha exista en el calendario.
 * @param fecha Texto a validar.
 * @return true si la fecha es válida.
 */
static bool validar_fecha(const char *fecha)
{
    int dia, mes, anio;
    static const int dias_por_mes[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

    if (strlen(fecha) != 10 || fecha[2] != '/' || fecha[5] != '/') return false;
    if (sscanf(fecha, "%2d/%2d/%4d", &dia, &mes, &anio) != 3) return false;
    if (anio < 1900 || anio > 2100 || mes < 1 || mes > 12) return false;

    int max_dia = dias_por_mes[mes - 1];
    if (mes == 2 && es_bisiesto(anio)) max_dia = 29;

    return dia >= 1 && dia <= max_dia;
}

/**
 * Escribe la fecha local actual en formato DD/MM/AAAA.
 * @param destino Buffer de salida (mínimo 11 bytes).
 * @param tam     Tamaño de `destino`.
 */
static void obtener_fecha_actual(char *destino, size_t tam)
{
    time_t ahora = time(NULL);
    struct tm *local = localtime(&ahora);
    if (local == NULL || strftime(destino, tam, "%d/%m/%Y", local) == 0) {
        copiar_texto_seguro(destino, "01/01/2026", tam);
    }
}

/**
 * Formatea un importe como "$1234.50" o "-$20.00".
 * @param destino Buffer de salida.
 * @param tam     Tamaño de `destino`.
 * @param valor   Importe a formatear.
 */
static void formatear_monto(char *destino, size_t tam, float valor)
{
    if (valor < 0.0f) snprintf(destino, tam, "-$%.2f", -valor);
    else              snprintf(destino, tam, "$%.2f", valor);
}


/* ========================================================================
 *  Componentes de interfaz
 * ===================================================================== */

/**
 * Activa el tema claro u oscuro.
 * @param oscuro true para el tema oscuro.
 */
static void aplicar_tema(bool oscuro)
{
    tema = oscuro ? &TEMA_OSCURO : &TEMA_CLARO;
}

/**
 * Mezcla un color con blanco.
 * @param c      Color base.
 * @param factor 0.0 = sin cambio, 1.0 = blanco.
 * @return Color resultante.
 */
static Color aclarar(Color c, float factor)
{
    Color r;
    r.r = (unsigned char)(c.r + (255 - c.r) * factor);
    r.g = (unsigned char)(c.g + (255 - c.g) * factor);
    r.b = (unsigned char)(c.b + (255 - c.b) * factor);
    r.a = c.a;
    return r;
}

/**
 * Mezcla un color con negro.
 * @param c      Color base.
 * @param factor 0.0 = sin cambio, 1.0 = negro.
 * @return Color resultante.
 */
static Color oscurecer(Color c, float factor)
{
    Color r;
    r.r = (unsigned char)(c.r * (1.0f - factor));
    r.g = (unsigned char)(c.g * (1.0f - factor));
    r.b = (unsigned char)(c.b * (1.0f - factor));
    r.a = c.a;
    return r;
}

/**
 * @param r Área a comprobar.
 * @return true si el cursor está sobre `r` y la interfaz no está bloqueada.
 */
static bool mouse_encima(Rectangle r)
{
    return !ui_bloqueada && CheckCollisionPointRec(GetMousePosition(), r);
}

/**
 * Dibuja un texto centrado dentro de un rectángulo.
 * @param texto Texto a dibujar.
 * @param r     Área contenedora.
 * @param tam   Tamaño de fuente.
 * @param color Color del texto.
 */
static void dibujar_texto_centrado(const char *texto, Rectangle r, int tam, Color color)
{
    int ancho = MeasureText(texto, tam);
    DrawText(texto, (int)(r.x + (r.width - ancho) / 2), (int)(r.y + (r.height - tam) / 2), tam, color);
}

/**
 * Dibuja un texto truncándolo por el final con "..." si supera el ancho máximo.
 * @param texto     Texto a dibujar (no se modifica).
 * @param x, y      Posición.
 * @param tam       Tamaño de fuente.
 * @param ancho_max Ancho disponible en píxeles.
 * @param color     Color del texto.
 */
static void dibujar_texto_recortado(const char *texto, int x, int y, int tam, int ancho_max, Color color)
{
    char buffer[MAX_RUTA + 4];
    copiar_texto_seguro(buffer, texto, MAX_RUTA);

    size_t len = strlen(buffer);
    while (len > 0 && MeasureText(buffer, tam) > ancho_max) {
        len--;
        buffer[len] = '\0';
        memcpy(buffer + len, "...", 4);
    }
    DrawText(buffer, x, y, tam, color);
}

/**
 * Igual que dibujar_texto_recortado, pero trunca por el inicio ("...\carpeta\archivo.csv").
 * Útil para rutas, donde la parte relevante es el final.
 * @param texto     Texto a dibujar.
 * @param x, y      Posición.
 * @param tam       Tamaño de fuente.
 * @param ancho_max Ancho disponible en píxeles.
 * @param color     Color del texto.
 */
static void dibujar_texto_recortado_inicio(const char *texto, int x, int y, int tam, int ancho_max, Color color)
{
    if (MeasureText(texto, tam) <= ancho_max) {
        DrawText(texto, x, y, tam, color);
        return;
    }

    char buffer[MAX_RUTA + 48] = "...";
    const char *visible = texto;
    while (*visible != '\0') {
        snprintf(buffer, sizeof(buffer), "...%s", visible);
        if (MeasureText(buffer, tam) <= ancho_max) break;
        visible++;
    }
    DrawText(buffer, x, y, tam, color);
}

/**
 * Botón con efecto hover y estado presionado.
 * @param r          Posición y tamaño.
 * @param texto      Etiqueta.
 * @param color_base Color de fondo.
 * @return true en el frame en que se hace clic.
 */
static bool dibujar_boton(Rectangle r, const char *texto, Color color_base)
{
    bool encima = mouse_encima(r);

    Color color = color_base;
    if (encima) {
        color = IsMouseButtonDown(MOUSE_BUTTON_LEFT) ? oscurecer(color_base, 0.15f)
                                                     : aclarar(color_base, 0.20f);
    }

    DrawRectangleRounded((Rectangle){ r.x + 2, r.y + 3, r.width, r.height }, 0.3f, 8, Fade(BLACK, 0.12f));
    DrawRectangleRounded(r, 0.3f, 8, color);
    dibujar_texto_centrado(texto, r, 20, WHITE);

    return encima && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

/**
 * Botón de selección: relleno con `color` si está seleccionado, contorneado si no.
 * @param r            Posición y tamaño.
 * @param texto        Etiqueta.
 * @param color        Color de acento.
 * @param seleccionado Estado actual.
 * @return true en el frame en que se hace clic.
 */
static bool dibujar_toggle(Rectangle r, const char *texto, Color color, bool seleccionado)
{
    bool encima = mouse_encima(r);

    if (seleccionado) {
        DrawRectangleRounded(r, 0.3f, 8, encima ? aclarar(color, 0.15f) : color);
        dibujar_texto_centrado(texto, r, 20, WHITE);
    } else {
        /* Borde simulado: rectángulo exterior + interior 2 px más pequeño */
        DrawRectangleRounded(r, 0.3f, 8, encima ? color : tema->borde);
        Rectangle interior = { r.x + 2, r.y + 2, r.width - 4, r.height - 4 };
        DrawRectangleRounded(interior, 0.3f, 8, encima ? tema->superficie_alt : tema->superficie);
        dibujar_texto_centrado(texto, r, 20, encima ? color : tema->texto_secundario);
    }

    return encima && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

/**
 * Inicializa un campo de texto vacío.
 * @param c               Campo a configurar.
 * @param etiqueta        Texto a la izquierda; "" para omitirla.
 * @param ayuda           Texto de ejemplo cuando el campo está vacío.
 * @param longitud_maxima Máximo de caracteres; se limita al tamaño del buffer.
 * @param filtro          Caracteres admitidos.
 * @param rect            Posición y tamaño de la caja.
 */
static void configurar_campo(CampoTexto *c, const char *etiqueta, const char *ayuda,
                             int longitud_maxima, FiltroCampo filtro, Rectangle rect)
{
    memset(c, 0, sizeof(*c));
    copiar_texto_seguro(c->etiqueta, etiqueta, sizeof(c->etiqueta));
    copiar_texto_seguro(c->ayuda, ayuda, sizeof(c->ayuda));

    if (longitud_maxima > (int)sizeof(c->texto) - 1) longitud_maxima = (int)sizeof(c->texto) - 1;
    c->longitud_maxima = longitud_maxima;
    c->filtro = filtro;
    c->rect = rect;
}

/**
 * Decide si un carácter es admisible según el filtro del campo.
 * Solo se acepta ASCII imprimible, para que borrar un byte borre un carácter.
 * @param c        Campo de destino.
 * @param caracter Código del carácter.
 * @return true si el carácter puede añadirse.
 */
static bool caracter_permitido(const CampoTexto *c, int caracter)
{
    if (caracter < 32 || caracter > 126) return false;

    switch (c->filtro) {
        case FILTRO_NUMERO:
            if (caracter >= '0' && caracter <= '9') return true;
            return caracter == '.' && strchr(c->texto, '.') == NULL;
        case FILTRO_FECHA:
            return (caracter >= '0' && caracter <= '9') || caracter == '/';
        case FILTRO_RUTA:
            return strchr("\"<>|*?", caracter) == NULL;   /* prohibidos en rutas de Windows */
        case FILTRO_TEXTO:
        default:
            return caracter != '|' && caracter != '"';    /* separador de datos.txt y comillas CSV */
    }
}

/**
 * Añade un carácter al final del texto si no se alcanzó la longitud máxima.
 * @param c        Campo a modificar.
 * @param caracter Carácter a añadir.
 */
static void agregar_caracter(CampoTexto *c, char caracter)
{
    size_t len = strlen(c->texto);
    if ((int)len < c->longitud_maxima) {
        c->texto[len] = caracter;
        c->texto[len + 1] = '\0';
    }
}

/**
 * Elimina el último carácter del texto.
 * @param c Campo a modificar.
 */
static void borrar_caracter(CampoTexto *c)
{
    size_t len = strlen(c->texto);
    if (len > 0) c->texto[len - 1] = '\0';
}

/**
 * Filtra y añade un carácter. Convierte ',' en '.' en montos y, al teclear
 * una fecha, inserta '/' automáticamente tras el día y el mes.
 * @param c              Campo a modificar.
 * @param caracter       Código del carácter.
 * @param escrito_a_mano false si proviene del portapapeles (sin auto '/').
 */
static void insertar_caracter(CampoTexto *c, int caracter, bool escrito_a_mano)
{
    if (c->filtro == FILTRO_NUMERO && caracter == ',') caracter = '.';
    if (!caracter_permitido(c, caracter)) return;

    size_t len = strlen(c->texto);
    if (escrito_a_mano && c->filtro == FILTRO_FECHA && caracter != '/' && (len == 2 || len == 5)) {
        agregar_caracter(c, '/');
    }
    agregar_caracter(c, (char)caracter);
}

/**
 * Procesa la entrada de teclado del campo activo: caracteres, Ctrl+V y
 * Backspace (con repetición al mantenerlo pulsado).
 * @param c Campo activo.
 */
static void actualizar_campo_texto(CampoTexto *c)
{
    static float temporizador_borrado = 0.0f;

    int caracter = GetCharPressed();
    while (caracter > 0) {
        insertar_caracter(c, caracter, true);
        caracter = GetCharPressed();
    }

    bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    if (ctrl && IsKeyPressed(KEY_V)) {
        const char *portapapeles = GetClipboardText();
        if (portapapeles != NULL) {
            for (const char *p = portapapeles; *p != '\0'; p++) {
                insertar_caracter(c, (unsigned char)*p, false);
            }
        }
    }

    if (IsKeyPressed(KEY_BACKSPACE)) {
        borrar_caracter(c);
        temporizador_borrado = 0.45f;          /* retardo antes de la repetición */
    } else if (IsKeyDown(KEY_BACKSPACE)) {
        temporizador_borrado -= GetFrameTime();
        if (temporizador_borrado <= 0.0f) {
            borrar_caracter(c);
            temporizador_borrado = 0.05f;      /* intervalo de repetición */
        }
    }
}

/**
 * Dibuja etiqueta, caja, texto (o ayuda si está vacío) y cursor parpadeante.
 * Si el texto no cabe, se muestra solo su parte final.
 * @param c      Campo a dibujar.
 * @param activo true si recibe el teclado.
 */
static void dibujar_campo_texto(const CampoTexto *c, bool activo)
{
    const int tam = 20;
    Rectangle r = c->rect;

    if (c->etiqueta[0] != '\0') {
        DrawText(c->etiqueta, (int)r.x - 170, (int)(r.y + (r.height - tam) / 2), tam, tema->texto);
    }

    DrawRectangleRounded(r, 0.2f, 8, activo ? COLOR_AZUL : tema->borde);
    Rectangle interior = { r.x + 2, r.y + 2, r.width - 4, r.height - 4 };
    DrawRectangleRounded(interior, 0.2f, 8, tema->campo);

    int texto_x = (int)r.x + 12;
    int texto_y = (int)(r.y + (r.height - tam) / 2);
    int ancho_disponible = (int)r.width - 30;

    if (c->texto[0] == '\0' && !activo) {
        DrawText(c->ayuda, texto_x, texto_y, tam, tema->placeholder);
        return;
    }

    const char *visible = c->texto;
    while (*visible != '\0' && MeasureText(visible, tam) > ancho_disponible) visible++;
    DrawText(visible, texto_x, texto_y, tam, tema->texto);

    if (activo && ((int)(GetTime() * 2.0) % 2) == 0) {
        int cursor_x = texto_x + MeasureText(visible, tam) + 2;
        DrawRectangle(cursor_x, texto_y - 2, 2, tam + 4, tema->texto);
    }
}

/**
 * Muestra una notificación, reemplazando la actual.
 * @param app     Estado de la aplicación.
 * @param tipo    Éxito, información o error (define el color y el icono).
 * @param titulo  Primera línea.
 * @param mensaje Segunda línea; "" para una notificación de una sola línea.
 */
static void notificar(EstadoApp *app, TipoNotificacion tipo, const char *titulo, const char *mensaje)
{
    Notificacion *n = &app->notificacion;
    n->tipo = tipo;
    copiar_texto_seguro(n->titulo, titulo, sizeof(n->titulo));
    copiar_texto_seguro(n->mensaje, mensaje, sizeof(n->mensaje));
    n->tiempo_restante = DURACION_NOTIFICACION;
}

/**
 * Dibuja la notificación en la esquina inferior derecha: entra deslizándose,
 * se desvanece al final y muestra una barra con el tiempo restante.
 * @param app Estado de la aplicación.
 */
static void dibujar_notificacion(const EstadoApp *app)
{
    const Notificacion *n = &app->notificacion;
    if (n->tiempo_restante <= 0.0f) return;

    Color color = (n->tipo == NOTIF_EXITO) ? COLOR_VERDE : (n->tipo == NOTIF_ERROR) ? COLOR_ROJO : COLOR_AZUL;

    float transcurrido = DURACION_NOTIFICACION - n->tiempo_restante;
    float entrada = (transcurrido < 0.25f) ? transcurrido / 0.25f : 1.0f;
    float alfa = (n->tiempo_restante < 0.5f) ? n->tiempo_restante / 0.5f : 1.0f;

    bool tiene_mensaje = (n->mensaje[0] != '\0');
    int ancho = MeasureText(n->titulo, 20);
    if (tiene_mensaje && MeasureText(n->mensaje, 18) > ancho) ancho = MeasureText(n->mensaje, 18);
    ancho += 64;
    if (ancho < 260) ancho = 260;
    if (ancho > 460) ancho = 460;
    int alto = tiene_mensaje ? 64 : 44;

    float x = ANCHO_VENTANA - ancho - 16 + (1.0f - entrada) * (ancho + 16);
    float y = ALTO_VENTANA - alto - 12;
    Rectangle r = { x, y, (float)ancho, (float)alto };

    DrawRectangleRounded((Rectangle){ x + 2, y + 4, r.width, r.height }, 0.25f, 8, Fade(BLACK, 0.25f * alfa));
    DrawRectangleRounded(r, 0.25f, 8, Fade(tema->superficie, alfa));
    DrawRectangleRounded((Rectangle){ x, y, 6, r.height }, 1.0f, 4, Fade(color, alfa));

    Vector2 centro = { x + 28, y + alto / 2.0f };
    DrawCircleV(centro, 12, Fade(color, alfa));
    if (n->tipo == NOTIF_EXITO) {
        DrawLineEx((Vector2){ centro.x - 6, centro.y }, (Vector2){ centro.x - 2, centro.y + 4 }, 2.5f, Fade(WHITE, alfa));
        DrawLineEx((Vector2){ centro.x - 2, centro.y + 4 }, (Vector2){ centro.x + 6, centro.y - 4 }, 2.5f, Fade(WHITE, alfa));
    } else {
        const char *simbolo = (n->tipo == NOTIF_ERROR) ? "!" : "i";
        dibujar_texto_centrado(simbolo, (Rectangle){ centro.x - 12, centro.y - 12, 24, 24 }, 20, Fade(WHITE, alfa));
    }

    int texto_x = (int)x + 50;
    int ancho_texto = ancho - 62;
    if (tiene_mensaje) {
        dibujar_texto_recortado(n->titulo, texto_x, (int)y + 10, 20, ancho_texto, Fade(color, alfa));
        dibujar_texto_recortado_inicio(n->mensaje, texto_x, (int)y + 36, 18, ancho_texto, Fade(tema->texto, alfa));
    } else {
        dibujar_texto_recortado(n->titulo, texto_x, (int)y + 12, 20, ancho_texto, Fade(color, alfa));
    }

    float progreso = n->tiempo_restante / DURACION_NOTIFICACION;
    DrawRectangle((int)x + 10, (int)(y + alto - 4), (int)((ancho - 20) * progreso), 2, Fade(color, alfa * 0.7f));
}

/**
 * Solicita un tooltip para el frame actual; se dibuja al final, sobre todo lo demás.
 * @param app     Estado de la aplicación.
 * @param color   Color del indicador.
 * @param titulo  Primera línea.
 * @param detalle Segunda línea.
 */
static void mostrar_tooltip(EstadoApp *app, Color color, const char *titulo, const char *detalle)
{
    app->tooltip.visible = true;
    app->tooltip.color = color;
    copiar_texto_seguro(app->tooltip.titulo, titulo, sizeof(app->tooltip.titulo));
    copiar_texto_seguro(app->tooltip.detalle, detalle, sizeof(app->tooltip.detalle));
}

/**
 * Dibuja el tooltip junto al cursor, reposicionándolo para que no salga de la ventana.
 * @param t Tooltip a dibujar.
 */
static void dibujar_tooltip(const Tooltip *t)
{
    if (!t->visible) return;

    Vector2 mouse = GetMousePosition();
    int ancho = MeasureText(t->titulo, 18);
    if (MeasureText(t->detalle, 16) > ancho) ancho = MeasureText(t->detalle, 16);
    ancho += 34;
    int alto = 52;

    float x = mouse.x + 14;
    float y = mouse.y + 20;
    if (x + ancho > ANCHO_VENTANA - 5) x = mouse.x - ancho - 10;
    if (y + alto > ALTO_VENTANA - 5)   y = mouse.y - alto - 10;
    if (x < 5) x = 5;

    DrawRectangleRounded((Rectangle){ x + 2, y + 3, (float)ancho, (float)alto }, 0.25f, 8, Fade(BLACK, 0.25f));
    DrawRectangleRounded((Rectangle){ x, y, (float)ancho, (float)alto }, 0.25f, 8, tema->tooltip);
    DrawRectangle((int)x + 12, (int)y + 13, 10, 10, t->color);
    DrawText(t->titulo, (int)x + 28, (int)y + 9, 18, WHITE);
    DrawText(t->detalle, (int)x + 12, (int)y + 31, 16, (Color){ 210, 215, 222, 255 });
}

/**
 * Dibuja la barra superior con el título y el botón de cambio de tema.
 * @param titulo Título de la pantalla.
 * @param app    Estado de la aplicación (se modifica al cambiar de tema).
 */
static void dibujar_encabezado(const char *titulo, EstadoApp *app)
{
    DrawRectangle(0, 0, ANCHO_VENTANA, 70, tema->encabezado);
    DrawText(titulo, 30, 22, 28, WHITE);

    const char *texto_boton = app->modo_oscuro ? "Modo claro" : "Modo oscuro";
    Color color_boton = app->modo_oscuro ? COLOR_AMBAR : COLOR_ACERO;

    if (dibujar_boton((Rectangle){ 640, 15, 140, 40 }, texto_boton, color_boton)) {
        app->modo_oscuro = !app->modo_oscuro;
        aplicar_tema(app->modo_oscuro);
        guardar_config(app);
        notificar(app, NOTIF_INFO,
                  app->modo_oscuro ? "Modo oscuro activado" : "Modo claro activado",
                  "Tu preferencia se guardó en config.txt");
    }
}


/* ========================================================================
 *  Pantallas
 * ===================================================================== */

/**
 * Restablece el formulario: campos vacíos, tipo GASTO y fecha de hoy.
 * @param f Formulario a reiniciar.
 */
static void reiniciar_formulario(EstadoFormulario *f)
{
    memset(f, 0, sizeof(*f));
    f->tipo = GASTO;

    const float x = 260, ancho = 450, alto = 42;
    configurar_campo(&f->campos[CAMPO_MONTO],       "Monto ($):",   "Ej: 1500.50",        12, FILTRO_NUMERO, (Rectangle){ x, 160, ancho, alto });
    configurar_campo(&f->campos[CAMPO_CATEGORIA],   "Categoría:",   "Ej: Comida, Sueldo", 30, FILTRO_TEXTO,  (Rectangle){ x, 220, ancho, alto });
    configurar_campo(&f->campos[CAMPO_DESCRIPCION], "Descripción:", "Ej: Compra semanal", MAX_TEXTO - 1, FILTRO_TEXTO, (Rectangle){ x, 280, ancho, alto });
    configurar_campo(&f->campos[CAMPO_FECHA],       "Fecha:",       "DD/MM/AAAA",         10, FILTRO_FECHA,  (Rectangle){ x, 340, ancho, alto });

    obtener_fecha_actual(f->campos[CAMPO_FECHA].texto, sizeof(f->campos[CAMPO_FECHA].texto));
    f->campo_activo = CAMPO_MONTO;
}

/**
 * Tarjeta de resumen del Dashboard. Reduce la fuente si el importe no cabe.
 * @param r      Posición y tamaño.
 * @param titulo Título de la tarjeta.
 * @param valor  Importe a mostrar.
 * @param color  Color del importe y del acento.
 */
static void dibujar_tarjeta(Rectangle r, const char *titulo, float valor, Color color)
{
    DrawRectangleRounded((Rectangle){ r.x + 3, r.y + 4, r.width, r.height }, 0.15f, 8, Fade(BLACK, 0.08f));
    DrawRectangleRounded(r, 0.15f, 8, tema->superficie);
    DrawRectangle((int)r.x + 20, (int)r.y + 16, 50, 5, color);
    DrawText(titulo, (int)r.x + 20, (int)r.y + 32, 20, tema->texto_secundario);

    char texto[40];
    formatear_monto(texto, sizeof(texto), valor);

    int tam = 34;
    while (tam > 14 && MeasureText(texto, tam) > (int)r.width - 40) tam -= 2;
    DrawText(texto, (int)r.x + 20, (int)r.y + 75, tam, color);
}

/**
 * Barra de proporción ingresos (verde) / gastos (rojo).
 * Al pasar el cursor resalta el segmento y muestra monto, porcentaje y nº de registros.
 * @param sistema Datos a representar.
 * @param app     Estado de la aplicación (recibe el tooltip).
 */
static void dibujar_barra_proporcion(const SistemaFinanciero *sistema, EstadoApp *app)
{
    Rectangle barra = { 35, 270, 730, 22 };
    Rectangle zona = { barra.x, barra.y - 5, barra.width, barra.height + 10 };   /* margen para facilitar el hover */

    DrawText("Ingresos vs Gastos", 35, 245, 18, tema->texto);
    const char *pista = "Pasa el cursor por la barra para ver detalles";
    DrawText(pista, 765 - MeasureText(pista, 14), 249, 14, tema->texto_secundario);

    int num_ingresos = 0, num_gastos = 0;
    for (int i = 0; i < sistema->cantidad_transacciones; i++) {
        if (sistema->transacciones[i].tipo == INGRESO) num_ingresos++;
        else num_gastos++;
    }

    Vector2 mouse = GetMousePosition();
    bool encima = mouse_encima(zona);
    float total = sistema->total_ingresos + sistema->total_gastos;

    if (total <= 0.0f) {
        DrawRectangleRec(barra, tema->borde);
        if (encima) {
            mostrar_tooltip(app, COLOR_GRIS, "Sin movimientos",
                            "Registra un ingreso o un gasto para ver la proporción");
        }
        return;
    }

    float porcentaje_ingresos = 100.0f * sistema->total_ingresos / total;
    float porcentaje_gastos = 100.0f - porcentaje_ingresos;
    float ancho_ingresos = barra.width * (sistema->total_ingresos / total);

    Rectangle parte_ingresos = { barra.x, barra.y, ancho_ingresos, barra.height };
    Rectangle parte_gastos = { barra.x + ancho_ingresos, barra.y, barra.width - ancho_ingresos, barra.height };

    bool sobre_ingresos = encima && mouse.x < barra.x + ancho_ingresos;
    bool sobre_gastos = encima && !sobre_ingresos;

    DrawRectangleRec(parte_ingresos, sobre_ingresos ? aclarar(COLOR_VERDE, 0.25f) : COLOR_VERDE);
    DrawRectangleRec(parte_gastos, sobre_gastos ? aclarar(COLOR_ROJO, 0.25f) : COLOR_ROJO);
    if (sobre_ingresos) DrawRectangleLinesEx(parte_ingresos, 2, tema->texto);
    if (sobre_gastos)   DrawRectangleLinesEx(parte_gastos, 2, tema->texto);

    char texto[64];
    snprintf(texto, sizeof(texto), "%.1f%% ingresos", porcentaje_ingresos);
    DrawText(texto, 35, 297, 14, COLOR_VERDE);
    snprintf(texto, sizeof(texto), "%.1f%% gastos", porcentaje_gastos);
    DrawText(texto, 765 - MeasureText(texto, 14), 297, 14, COLOR_ROJO);

    char titulo[80], monto[40], detalle[120];
    if (sobre_ingresos) {
        formatear_monto(monto, sizeof(monto), sistema->total_ingresos);
        snprintf(titulo, sizeof(titulo), "Ingresos: %s", monto);
        snprintf(detalle, sizeof(detalle), "%.1f%% del total movido (%d registro%s)",
                 porcentaje_ingresos, num_ingresos, num_ingresos == 1 ? "" : "s");
        mostrar_tooltip(app, COLOR_VERDE, titulo, detalle);
    } else if (sobre_gastos) {
        formatear_monto(monto, sizeof(monto), sistema->total_gastos);
        snprintf(titulo, sizeof(titulo), "Gastos: %s", monto);
        snprintf(detalle, sizeof(detalle), "%.1f%% del total movido (%d registro%s)",
                 porcentaje_gastos, num_gastos, num_gastos == 1 ? "" : "s");
        mostrar_tooltip(app, COLOR_ROJO, titulo, detalle);
    }
}

/**
 * Abre el diálogo de exportación con la última ruta usada o, si no hay,
 * con el directorio de trabajo y el nombre por defecto.
 * @param app Estado de la aplicación.
 */
static void abrir_dialogo_exportar(EstadoApp *app)
{
    DialogoExportar *d = &app->dialogo_exportar;
    memset(d, 0, sizeof(*d));
    configurar_campo(&d->ruta, "", "Ej: C:\\Users\\yo\\Desktop\\finanzas.csv", MAX_RUTA - 1,
                     FILTRO_RUTA, (Rectangle){ 110, 232, 580, 42 });

    if (app->ultima_ruta_csv[0] != '\0') {
        copiar_texto_seguro(d->ruta.texto, app->ultima_ruta_csv, sizeof(d->ruta.texto));
    } else {
        snprintf(d->ruta.texto, sizeof(d->ruta.texto), "%s%s%s",
                 GetWorkingDirectory(), SEPARADOR_TXT, NOMBRE_CSV_POR_DEFECTO);
    }
    d->abierto = true;
}

/**
 * Sustituye la carpeta de la ruta actual conservando el nombre de archivo.
 * @param d       Diálogo de exportación.
 * @param carpeta Nueva carpeta.
 */
static void usar_carpeta_en_ruta(DialogoExportar *d, const char *carpeta)
{
    /* GetFileName apunta dentro de d->ruta.texto: se copia antes de sobrescribirlo */
    char nombre[MAX_RUTA];
    copiar_texto_seguro(nombre, GetFileName(d->ruta.texto), sizeof(nombre));
    if (nombre[0] == '\0' || !IsFileExtension(nombre, ".csv")) {
        copiar_texto_seguro(nombre, NOMBRE_CSV_POR_DEFECTO, sizeof(nombre));
    }

    char nueva[MAX_RUTA * 2 + 2];
    snprintf(nueva, sizeof(nueva), "%s%s%s", carpeta, SEPARADOR_TXT, nombre);
    copiar_texto_seguro(d->ruta.texto, nueva, sizeof(d->ruta.texto));
    d->error[0] = '\0';
}

/**
 * Aplica un acceso rápido de carpeta y notifica el resultado.
 * @param app            Estado de la aplicación.
 * @param nombre_visible Nombre mostrado en la notificación.
 * @param candidatos     Rutas posibles relativas al perfil (ver buscar_carpeta_usuario).
 * @param cantidad       Número de candidatos.
 */
static void acceso_rapido_carpeta(EstadoApp *app, const char *nombre_visible,
                                  const char *const candidatos[], int cantidad)
{
    char carpeta[MAX_RUTA];
    if (buscar_carpeta_usuario(candidatos, cantidad, carpeta, sizeof(carpeta))) {
        usar_carpeta_en_ruta(&app->dialogo_exportar, carpeta);
        char titulo[80];
        snprintf(titulo, sizeof(titulo), "Carpeta: %s", nombre_visible);
        notificar(app, NOTIF_INFO, titulo, carpeta);
    } else {
        char titulo[80];
        snprintf(titulo, sizeof(titulo), "No se encontró %s", nombre_visible);
        notificar(app, NOTIF_ERROR, titulo, "Escribe la ruta manualmente");
    }
}

/**
 * Valida la ruta del diálogo y exporta. Si tiene éxito, recuerda la ruta y cierra el diálogo.
 * @param sistema Datos a exportar.
 * @param app     Estado de la aplicación.
 */
static void confirmar_exportacion(const SistemaFinanciero *sistema, EstadoApp *app)
{
    DialogoExportar *d = &app->dialogo_exportar;
    char ruta_final[MAX_RUTA];

    if (!preparar_ruta_csv(d->ruta.texto, ruta_final, sizeof(ruta_final), d->error, sizeof(d->error))) {
        notificar(app, NOTIF_ERROR, "Ruta no válida", d->error);
        return;
    }

    if (!exportar_csv(sistema, ruta_final)) {
        copiar_texto_seguro(d->error, "No se pudo crear el archivo. ¿Tienes permisos en esa carpeta?", sizeof(d->error));
        notificar(app, NOTIF_ERROR, "Error al exportar", ruta_final);
        return;
    }

    copiar_texto_seguro(app->ultima_ruta_csv, ruta_final, sizeof(app->ultima_ruta_csv));
    guardar_config(app);
    d->abierto = false;
    notificar(app, NOTIF_EXITO, "¡CSV Exportado con éxito!", ruta_final);
}

/**
 * Diálogo modal de exportación: campo de ruta, accesos rápidos y botones Exportar/Cancelar.
 * @param sistema Datos a exportar.
 * @param app     Estado de la aplicación.
 */
static void ventana_exportar(const SistemaFinanciero *sistema, EstadoApp *app)
{
    /* Orden de preferencia: primero las carpetas redirigidas por OneDrive */
    static const char *const ESCRITORIO[] = {
        "OneDrive" SEPARADOR_TXT "Desktop", "OneDrive" SEPARADOR_TXT "Escritorio", "Desktop", "Escritorio"
    };
    static const char *const DOCUMENTOS[] = {
        "OneDrive" SEPARADOR_TXT "Documents", "OneDrive" SEPARADOR_TXT "Documentos", "Documents", "Documentos"
    };

    DialogoExportar *d = &app->dialogo_exportar;

    DrawRectangle(0, 0, ANCHO_VENTANA, ALTO_VENTANA, Fade(BLACK, 0.55f));

    Rectangle panel = { 80, 130, 640, 345 };
    DrawRectangleRounded((Rectangle){ panel.x + 3, panel.y + 5, panel.width, panel.height }, 0.06f, 8, Fade(BLACK, 0.3f));
    DrawRectangleRounded(panel, 0.06f, 8, tema->superficie);

    DrawText("Exportar historial a CSV", 110, 150, 24, tema->texto);
    char subtitulo[80];
    snprintf(subtitulo, sizeof(subtitulo), "Se exportarán %d transacciones.", sistema->cantidad_transacciones);
    DrawText(subtitulo, 110, 182, 18, tema->texto_secundario);
    DrawText("Ruta del archivo (carpeta o archivo .csv):", 110, 210, 18, tema->texto);

    actualizar_campo_texto(&d->ruta);
    dibujar_campo_texto(&d->ruta, true);

    if (dibujar_boton((Rectangle){ 110, 290, 170, 36 }, "Escritorio", COLOR_ACERO)) {
        acceso_rapido_carpeta(app, "Escritorio", ESCRITORIO, 4);
    }
    if (dibujar_boton((Rectangle){ 290, 290, 170, 36 }, "Documentos", COLOR_ACERO)) {
        acceso_rapido_carpeta(app, "Documentos", DOCUMENTOS, 4);
    }
    if (dibujar_boton((Rectangle){ 470, 290, 220, 36 }, "Carpeta actual", COLOR_ACERO)) {
        usar_carpeta_en_ruta(d, GetWorkingDirectory());
        notificar(app, NOTIF_INFO, "Carpeta: actual", GetWorkingDirectory());
    }

    DrawText("Ctrl+V para pegar una ruta. Si el archivo existe, se sobrescribirá.", 110, 340, 16, tema->texto_secundario);

    if (d->error[0] != '\0') {
        dibujar_texto_recortado(d->error, 110, 366, 18, 580, COLOR_ROJO);
    }

    bool pulso_exportar = dibujar_boton((Rectangle){ 200, 405, 190, 46 }, "Exportar", COLOR_VERDE);
    bool pulso_cancelar = dibujar_boton((Rectangle){ 410, 405, 190, 46 }, "Cancelar", COLOR_GRIS);

    if (pulso_exportar || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
        confirmar_exportacion(sistema, app);
    } else if (pulso_cancelar || IsKeyPressed(KEY_ESCAPE)) {
        d->abierto = false;
        notificar(app, NOTIF_INFO, "Exportación cancelada", "No se creó ningún archivo");
    }
}

/**
 * Pantalla principal: tarjetas de resumen, barra de proporción y menú de acciones.
 * @param sistema Datos financieros.
 * @param app     Estado de la aplicación.
 */
static void pantalla_dashboard(SistemaFinanciero *sistema, EstadoApp *app)
{
    dibujar_encabezado("Gestor de Finanzas Personales", app);

    Color color_balance = (sistema->balance_total >= 0.0f) ? COLOR_AZUL : COLOR_ROJO;
    dibujar_tarjeta((Rectangle){  35, 95, 230, 130 }, "Balance Total",   sistema->balance_total,  color_balance);
    dibujar_tarjeta((Rectangle){ 285, 95, 230, 130 }, "Total Ingresos",  sistema->total_ingresos, COLOR_VERDE);
    dibujar_tarjeta((Rectangle){ 535, 95, 230, 130 }, "Total Gastos",    sistema->total_gastos,   COLOR_ROJO);

    dibujar_barra_proporcion(sistema, app);

    if (dibujar_boton((Rectangle){ 50, 330, 340, 60 }, "Nuevo Registro", COLOR_AZUL)) {
        if (sistema->cantidad_transacciones >= MAX_TRANSACCIONES) {
            notificar(app, NOTIF_ERROR, "Límite alcanzado", "Solo se permiten 100 transacciones");
        } else {
            reiniciar_formulario(&app->formulario);
            app->pantalla = PANTALLA_FORMULARIO;
            notificar(app, NOTIF_INFO, "Nuevo registro", "Completa los campos y pulsa Guardar");
        }
    }

    if (dibujar_boton((Rectangle){ 410, 330, 340, 60 }, "Ver Historial", COLOR_MORADO)) {
        app->scroll_historial = 0.0f;
        app->pantalla = PANTALLA_HISTORIAL;
        char detalle[60];
        snprintf(detalle, sizeof(detalle), "%d registro%s encontrado%s", sistema->cantidad_transacciones,
                 sistema->cantidad_transacciones == 1 ? "" : "s", sistema->cantidad_transacciones == 1 ? "" : "s");
        notificar(app, NOTIF_INFO, "Historial", detalle);
    }

    if (dibujar_boton((Rectangle){ 50, 410, 340, 60 }, "Exportar a CSV", COLOR_VERDE)) {
        abrir_dialogo_exportar(app);
    }

    if (dibujar_boton((Rectangle){ 410, 410, 340, 60 }, "Guardar y Salir", COLOR_PIZARRA)) {
        if (guardar_datos(sistema)) {
            guardar_config(app);
            notificar(app, NOTIF_EXITO, "Datos guardados", "¡Hasta pronto! Cerrando la aplicación...");
            app->cerrando = true;
            app->tiempo_cierre = 1.5f;   /* deja ver la notificación antes de cerrar */
        } else {
            notificar(app, NOTIF_ERROR, "No se pudo guardar", "Revisa los permisos de datos.txt");
        }
    }

    char info[80];
    snprintf(info, sizeof(info), "Transacciones registradas: %d / %d",
             sistema->cantidad_transacciones, MAX_TRANSACCIONES);
    DrawText(info, 35, 500, 18, tema->texto_secundario);
}

/**
 * Valida el formulario, registra la transacción, recalcula el balance y guarda en disco.
 * Ante un error lo muestra y enfoca el campo afectado; si tiene éxito, vuelve al Dashboard.
 * @param sistema Sistema destino.
 * @param app     Estado de la aplicación.
 */
static void guardar_formulario(SistemaFinanciero *sistema, EstadoApp *app)
{
    EstadoFormulario *f = &app->formulario;
    float monto = 0.0f;

    if (!validar_monto(f->campos[CAMPO_MONTO].texto, &monto)) {
        copiar_texto_seguro(f->mensaje_error, "El monto debe ser un número válido mayor que 0", sizeof(f->mensaje_error));
        f->campo_activo = CAMPO_MONTO;
        notificar(app, NOTIF_ERROR, "Revisa el formulario", f->mensaje_error);
        return;
    }

    if (!validar_fecha(f->campos[CAMPO_FECHA].texto)) {
        copiar_texto_seguro(f->mensaje_error, "Fecha inválida. Use el formato DD/MM/AAAA", sizeof(f->mensaje_error));
        f->campo_activo = CAMPO_FECHA;
        notificar(app, NOTIF_ERROR, "Revisa el formulario", f->mensaje_error);
        return;
    }

    if (!agregar_transaccion(sistema, f->tipo, monto,
                             f->campos[CAMPO_CATEGORIA].texto,
                             f->campos[CAMPO_DESCRIPCION].texto,
                             f->campos[CAMPO_FECHA].texto)) {
        copiar_texto_seguro(f->mensaje_error, "No hay espacio para más transacciones", sizeof(f->mensaje_error));
        notificar(app, NOTIF_ERROR, "No se pudo guardar", f->mensaje_error);
        return;
    }

    recalcular_balance(sistema);
    const Transaccion *nueva = &sistema->transacciones[sistema->cantidad_transacciones - 1];

    char texto_monto[40], detalle[120];
    formatear_monto(texto_monto, sizeof(texto_monto), nueva->monto);
    snprintf(detalle, sizeof(detalle), "%s de %s - %s",
             nueva->tipo == INGRESO ? "Ingreso" : "Gasto", texto_monto, nueva->categoria);

    if (guardar_datos(sistema)) {
        notificar(app, NOTIF_EXITO, "¡Transacción guardada!", detalle);
    } else {
        notificar(app, NOTIF_ERROR, "Guardada solo en memoria", "No se pudo escribir datos.txt");
    }
    app->pantalla = PANTALLA_DASHBOARD;
}

/**
 * Formulario de nuevo registro. Atajos: TAB/Shift+TAB, ENTER (guardar), ESC (cancelar).
 * @param sistema Sistema destino.
 * @param app     Estado de la aplicación.
 */
static void pantalla_formulario(SistemaFinanciero *sistema, EstadoApp *app)
{
    EstadoFormulario *f = &app->formulario;

    dibujar_encabezado("Nuevo Registro", app);

    DrawText("Tipo:", 90, 107, 20, tema->texto);
    if (dibujar_toggle((Rectangle){ 260, 95, 215, 45 }, "INGRESO", COLOR_VERDE, f->tipo == INGRESO) && f->tipo != INGRESO) {
        f->tipo = INGRESO;
        notificar(app, NOTIF_INFO, "Tipo: INGRESO", "El monto se sumará al balance");
    }
    if (dibujar_toggle((Rectangle){ 495, 95, 215, 45 }, "GASTO", COLOR_ROJO, f->tipo == GASTO) && f->tipo != GASTO) {
        f->tipo = GASTO;
        notificar(app, NOTIF_INFO, "Tipo: GASTO", "El monto se restará del balance");
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        for (int i = 0; i < NUM_CAMPOS; i++) {
            if (mouse_encima(f->campos[i].rect)) f->campo_activo = i;
        }
    }

    if (IsKeyPressed(KEY_TAB)) {
        bool atras = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        if (f->campo_activo < 0) f->campo_activo = 0;
        else f->campo_activo = (f->campo_activo + (atras ? NUM_CAMPOS - 1 : 1)) % NUM_CAMPOS;
    }

    if (f->campo_activo >= 0 && f->campo_activo < NUM_CAMPOS) {
        actualizar_campo_texto(&f->campos[f->campo_activo]);
    }

    for (int i = 0; i < NUM_CAMPOS; i++) {
        dibujar_campo_texto(&f->campos[i], i == f->campo_activo);
    }

    DrawText("TAB: siguiente campo   ENTER: guardar   ESC: cancelar   Ctrl+V: pegar", 90, 400, 16, tema->texto_secundario);

    if (f->mensaje_error[0] != '\0') {
        DrawText(f->mensaje_error, 90, 426, 20, COLOR_ROJO);
    }

    bool pulso_guardar = dibujar_boton((Rectangle){ 90, 462, 260, 52 }, "Guardar Transacción", COLOR_VERDE);
    bool pulso_cancelar = dibujar_boton((Rectangle){ 370, 462, 160, 52 }, "Cancelar", COLOR_GRIS);

    if (pulso_guardar || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
        guardar_formulario(sistema, app);
    } else if (pulso_cancelar || IsKeyPressed(KEY_ESCAPE)) {
        app->pantalla = PANTALLA_DASHBOARD;
        notificar(app, NOTIF_INFO, "Registro cancelado", "No se guardó ningún cambio");
    }
}

/**
 * Tabla del historial con scroll (rueda o flechas) y orden configurable.
 * @param sistema Datos a mostrar.
 * @param app     Estado de la aplicación.
 */
static void pantalla_historial(const SistemaFinanciero *sistema, EstadoApp *app)
{
    const int area_y = 110, area_alto = 400, alto_fila = 32, tam = 18;
    const int col_x[6]     = { 15, 60, 150, 280, 440, 672 };
    const int col_ancho[6] = { 40, 85, 125, 150, 225, 110 };
    const char *titulos[6] = { "ID", "Tipo", "Monto", "Categoría", "Descripción", "Fecha" };

    int n = sistema->cantidad_transacciones;

    dibujar_encabezado("Historial", app);

    if (dibujar_toggle((Rectangle){ 420, 15, 205, 40 },
                       app->recientes_primero ? "Recientes primero" : "Antiguos primero",
                       COLOR_AZUL, true)) {
        app->recientes_primero = !app->recientes_primero;
        notificar(app, NOTIF_INFO, "Orden cambiado",
                  app->recientes_primero ? "Ahora se muestran los más recientes primero"
                                         : "Ahora se muestran los más antiguos primero");
    }

    float contenido_alto = (float)(n * alto_fila);
    float scroll_maximo = contenido_alto - area_alto;
    if (scroll_maximo < 0.0f) scroll_maximo = 0.0f;

    app->scroll_historial -= GetMouseWheelMove() * alto_fila * 1.5f;
    if (IsKeyDown(KEY_DOWN)) app->scroll_historial += 400.0f * GetFrameTime();
    if (IsKeyDown(KEY_UP))   app->scroll_historial -= 400.0f * GetFrameTime();
    if (app->scroll_historial < 0.0f) app->scroll_historial = 0.0f;
    if (app->scroll_historial > scroll_maximo) app->scroll_historial = scroll_maximo;

    DrawRectangle(0, 78, ANCHO_VENTANA, 32, tema->cabecera_tabla);
    for (int c = 0; c < 6; c++) DrawText(titulos[c], col_x[c], 85, tam, WHITE);

    if (n == 0) {
        dibujar_texto_centrado("No hay transacciones registradas todavía.",
                               (Rectangle){ 0, (float)area_y, ANCHO_VENTANA, (float)area_alto }, 20, tema->texto_secundario);
    }

    /* Recorta las filas desplazadas para que no invadan la cabecera ni el pie */
    BeginScissorMode(0, area_y, ANCHO_VENTANA, area_alto);
    for (int i = 0; i < n; i++) {
        int y = area_y + i * alto_fila - (int)app->scroll_historial;
        if (y + alto_fila < area_y || y > area_y + area_alto) continue;

        int indice = app->recientes_primero ? (n - 1 - i) : i;
        const Transaccion *t = &sistema->transacciones[indice];
        Color color_tipo = (t->tipo == INGRESO) ? COLOR_VERDE : COLOR_ROJO;
        int texto_y = y + (alto_fila - tam) / 2;

        DrawRectangle(0, y, ANCHO_VENTANA, alto_fila, (i % 2 == 0) ? tema->superficie : tema->superficie_alt);

        char id_texto[16], monto_texto[40];
        snprintf(id_texto, sizeof(id_texto), "%d", t->id);
        formatear_monto(monto_texto + 1, sizeof(monto_texto) - 1, t->monto);
        monto_texto[0] = (t->tipo == INGRESO) ? '+' : '-';

        dibujar_texto_recortado(id_texto, col_x[0], texto_y, tam, col_ancho[0], tema->texto);
        dibujar_texto_recortado(t->tipo == INGRESO ? "INGRESO" : "GASTO", col_x[1], texto_y, tam, col_ancho[1], color_tipo);
        dibujar_texto_recortado(monto_texto, col_x[2], texto_y, tam, col_ancho[2], color_tipo);
        dibujar_texto_recortado(t->categoria, col_x[3], texto_y, tam, col_ancho[3], tema->texto);
        dibujar_texto_recortado(t->descripcion, col_x[4], texto_y, tam, col_ancho[4], tema->texto);
        dibujar_texto_recortado(t->fecha, col_x[5], texto_y, tam, col_ancho[5], tema->texto);
    }
    EndScissorMode();

    if (scroll_maximo > 0.0f) {
        float alto_barra = area_alto * (area_alto / contenido_alto);
        float y_barra = area_y + (app->scroll_historial / scroll_maximo) * (area_alto - alto_barra);
        DrawRectangle(788, area_y, 8, area_alto, tema->borde);
        DrawRectangleRounded((Rectangle){ 788, y_barra, 8, alto_barra }, 1.0f, 6, tema->texto_secundario);
    }

    DrawLine(0, area_y + area_alto, ANCHO_VENTANA, area_y + area_alto, tema->borde);

    char resumen[60];
    snprintf(resumen, sizeof(resumen), "Total: %d registros", n);
    DrawText(resumen, 290, 530, 18, tema->texto_secundario);
    DrawText("Rueda del mouse / flechas: desplazar", 290, 555, 16, tema->texto_secundario);

    if (dibujar_boton((Rectangle){ 20, 528, 250, 50 }, "Volver al Dashboard", COLOR_AZUL) ||
        IsKeyPressed(KEY_ESCAPE)) {
        app->pantalla = PANTALLA_DASHBOARD;
        notificar(app, NOTIF_INFO, "Dashboard", "Volviste al menú principal");
    }
}

/** Capa semitransparente que se muestra mientras la aplicación se cierra. */
static void dibujar_capa_cierre(void)
{
    DrawRectangle(0, 0, ANCHO_VENTANA, ALTO_VENTANA, Fade(BLACK, 0.45f));
    dibujar_texto_centrado("Guardando y cerrando...", (Rectangle){ 0, 0, ANCHO_VENTANA, ALTO_VENTANA - 80 }, 30, WHITE);
}


/* ========================================================================
 *  Programa principal
 * ===================================================================== */

int main(void)
{
    /* static: evita ~12 KB en la pila y garantiza inicialización a cero */
    static SistemaFinanciero sistema;
    static EstadoApp app;

    cargar_datos(&sistema);
    cargar_config(&app);
    aplicar_tema(app.modo_oscuro);

    app.pantalla = PANTALLA_DASHBOARD;
    app.recientes_primero = true;

    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(ANCHO_VENTANA, ALTO_VENTANA, "Sistema de Gestion Financiera Personal");
    SetExitKey(0);   /* ESC se usa para navegar, no para cerrar */
    SetTargetFPS(60);

    if (sistema.cantidad_transacciones > 0) {
        char detalle[80];
        snprintf(detalle, sizeof(detalle), "%d transacciones leídas de %s", sistema.cantidad_transacciones, ARCHIVO_DATOS);
        notificar(&app, NOTIF_EXITO, "Datos cargados", detalle);
    } else {
        notificar(&app, NOTIF_INFO, "¡Bienvenido!", "Aún no hay transacciones. Crea un Nuevo Registro");
    }

    while (!app.salir && !WindowShouldClose()) {
        float dt = GetFrameTime();

        if (app.notificacion.tiempo_restante > 0.0f) app.notificacion.tiempo_restante -= dt;
        if (app.cerrando) {
            app.tiempo_cierre -= dt;
            if (app.tiempo_cierre <= 0.0f) app.salir = true;
        }

        /* El diálogo solo atiende la entrada si ya estaba abierto al iniciar el frame,
         * así el clic que lo abre no activa también uno de sus botones. */
        bool dialogo_abierto = app.dialogo_exportar.abierto;
        app.tooltip.visible = false;

        BeginDrawing();
        ClearBackground(tema->fondo);

        ui_bloqueada = dialogo_abierto || app.cerrando;
        switch (app.pantalla) {
            case PANTALLA_DASHBOARD:  pantalla_dashboard(&sistema, &app);  break;
            case PANTALLA_FORMULARIO: pantalla_formulario(&sistema, &app); break;
            case PANTALLA_HISTORIAL:  pantalla_historial(&sistema, &app);  break;
        }

        ui_bloqueada = app.cerrando;
        if (dialogo_abierto && app.dialogo_exportar.abierto) ventana_exportar(&sistema, &app);
        ui_bloqueada = false;

        if (app.cerrando) dibujar_capa_cierre();
        dibujar_tooltip(&app.tooltip);
        dibujar_notificacion(&app);

        EndDrawing();
    }

    /* Cierre con la X de la ventana: guardar igualmente */
    if (!app.cerrando) {
        guardar_datos(&sistema);
        guardar_config(&app);
    }

    CloseWindow();
    return 0;
}


/* ========================================================================
 *  Compilación
 * ========================================================================
 *
 *  Windows (gcc mingw-w64 de 64 bits, Raylib en .\raylib):
 *      .\ejecutar.bat
 *    o manualmente:
 *      gcc main.c -o financiero.exe -std=c99 -Wall -Iraylib\include -Lraylib\lib -lraylib -lopengl32 -lgdi32 -lwinmm
 *
 *  Linux:
 *      gcc main.c -o financiero -std=c99 -Wall -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
 *
 *  macOS:
 *      gcc main.c -o financiero -std=c99 -Wall $(pkg-config --cflags --libs raylib)
 * ===================================================================== */
