#ifndef CONEXION_BD_H
#define CONEXION_BD_H

#include <mysql.h>
#include <string>
#include <vector>

using namespace std;

// Clase encargada de hablar con MySQL.
// La usan los DAO para ejecutar consultas.
class ConexionBD {
private:
    // Puntero de conexión que usa la librería de MySQL.
    MYSQL* conexion;

public:
    // Constructor: empieza sin conexión.
    ConexionBD();

    // Destructor: cierra la conexión al terminar.
    ~ConexionBD();

    // Abre la conexión con MySQL.
    bool conectar(const string& host, int puerto, const string& usuario, const string& clave, const string& nombreBD);

    // Ejecuta INSERT, UPDATE o DELETE.
    bool ejecutar(const string& sql);

    // Ejecuta SELECT y devuelve las filas como texto.
    vector<vector<string>> consultar(const string& sql);

    // Evita problemas con comillas en textos ingresados.
    string escapar(const string& texto);

    // Devuelve el último error de MySQL.
    string ultimoError();
};

#endif
