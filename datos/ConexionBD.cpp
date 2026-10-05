#include "ConexionBD.h"

// Constructor: inicializa la conexión como vacía.
ConexionBD::ConexionBD() {
    conexion = nullptr;
}

// Destructor: si hay conexión abierta, la cierra.
ConexionBD::~ConexionBD() {
    if (conexion != nullptr) {
        mysql_close(conexion);
    }
}

// Conecta el programa con la base de datos MySQL.
bool ConexionBD::conectar(const string& host, int puerto, const string& usuario, const string& clave, const string& nombreBD) {
    if (conexion != nullptr) {
        mysql_close(conexion);
        conexion = nullptr;
    }
    conexion = mysql_init(nullptr);

    // Si MySQL no se pudo iniciar, falla la conexión.
    if (conexion == nullptr) {
        return false;
    }

    // Evita que una caída de red deje congelada la interfaz demasiado tiempo.
    unsigned int esperaConexion = 8;
    unsigned int esperaLectura = 20;
    unsigned int esperaEscritura = 20;
    mysql_options(conexion, MYSQL_OPT_CONNECT_TIMEOUT, &esperaConexion);
    mysql_options(conexion, MYSQL_OPT_READ_TIMEOUT, &esperaLectura);
    mysql_options(conexion, MYSQL_OPT_WRITE_TIMEOUT, &esperaEscritura);

    // Intenta conectar con los datos recibidos.
    if (!mysql_real_connect(conexion, host.c_str(), usuario.c_str(), clave.c_str(), nombreBD.c_str(), puerto, nullptr, 0)) {
        return false;
    }

    // Usa UTF-8 para aceptar acentos y caracteres en español.
    ejecutar("SET NAMES utf8mb4");
    return true;
}

// Ejecuta una consulta que no devuelve tabla.
bool ConexionBD::ejecutar(const string& sql) {
    return mysql_query(conexion, sql.c_str()) == 0;
}

// Ejecuta una consulta SELECT y devuelve los resultados.
vector<vector<string>> ConexionBD::consultar(const string& sql) {
    vector<vector<string>> datos;

    // Si falla la consulta, devuelve una lista vacía.
    if (mysql_query(conexion, sql.c_str()) != 0) {
        return datos;
    }

    // Guarda el resultado que devolvió MySQL.
    MYSQL_RES* resultado = mysql_store_result(conexion);
    if (resultado == nullptr) {
        return datos;
    }

    // Cantidad de columnas que tiene el SELECT.
    int columnas = mysql_num_fields(resultado);
    MYSQL_ROW fila;

    // Recorre cada fila y la guarda como vector de textos.
    while ((fila = mysql_fetch_row(resultado))) {
        vector<string> actual;

        for (int i = 0; i < columnas; i++) {
            // Si el valor viene vacío, se guarda como texto vacío.
            actual.push_back(fila[i] ? fila[i] : "");
        }

        datos.push_back(actual);
    }

    // Libera memoria usada por MySQL.
    mysql_free_result(resultado);
    return datos;
}

// Prepara un texto para poder usarlo en una consulta SQL.
string ConexionBD::escapar(const string& texto) {
    vector<char> buffer(texto.length() * 2 + 1);
    unsigned long largo = mysql_real_escape_string(conexion, buffer.data(), texto.c_str(), (unsigned long)texto.length());
    return string(buffer.data(), largo);
}

// Devuelve el último error informado por MySQL.
string ConexionBD::ultimoError() {
    if (conexion == nullptr) {
        return "No hay conexión con MySQL.";
    }

    return mysql_error(conexion);
}
