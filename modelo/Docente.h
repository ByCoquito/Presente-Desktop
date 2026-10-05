#ifndef DOCENTE_H
#define DOCENTE_H

#include <string>

using namespace std;

// Guarda los datos basicos del docente que inicio sesion.
struct Docente {
    int id = 0;
    string nombre;
    string correo;
};

#endif
