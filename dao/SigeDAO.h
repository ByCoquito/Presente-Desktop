#ifndef SIGE_DAO_H
#define SIGE_DAO_H

#include "../api/ApiCliente.h"
#include "../datos/ConexionBD.h"
#include "../modelo/Docente.h"

// Centraliza las operaciones de datos realizadas contra S.I.G.E.
class SigeDAO {
private:
    ApiCliente& api;
    ConexionBD db;
    int docenteId = 0;

    RespuestaAPI correcto(const QJsonValue& data);
    RespuestaAPI error(const QString& mensaje, int estadoHttp = 500, const QString& codigo = "");
    QJsonObject cursoJson(const std::vector<std::string>& fila);
    void sincronizarAlumnosPlanilla(int planillaId);
    QJsonObject planillaCompleta(int planillaId);

public:
    SigeDAO(ApiCliente& cliente);

    RespuestaAPI iniciarSesion(const QString& correo, const QString& clave, Docente& docente);
    RespuestaAPI cerrarSesion();

    RespuestaAPI resumenPanel();
    RespuestaAPI listarCursos();
    RespuestaAPI guardarCurso(int cursoId, const QString& anio, const QString& division, int cicloLectivo);
    RespuestaAPI archivarCurso(int cursoId);
    RespuestaAPI listarMaterias();
    RespuestaAPI guardarMateria(int materiaId, const QString& nombre);
    RespuestaAPI archivarMateria(int materiaId);
    RespuestaAPI asignarMateriaCurso(int materiaId, int cursoId);
    RespuestaAPI listarAsignaciones();
    RespuestaAPI listarEstudiantes(int cursoId = 0);
    RespuestaAPI guardarEstudiante(int estudianteId, const QString& nombre, const QString& segundoNombre,
        const QString& apellido, const QString& segundoApellido, const QString& dni, int cursoId);
    RespuestaAPI eliminarEstudiante(int estudianteId);

    RespuestaAPI listarPlanillas();
    RespuestaAPI obtenerPlanilla(int planillaId);
    RespuestaAPI crearPlanilla(const QString& titulo, int asignacionId);
    RespuestaAPI crearColumna(int planillaId, const QString& titulo, double aprobacion, double maximo);
    RespuestaAPI editarColumna(int planillaId, int columnaId, const QString& titulo, double aprobacion, double maximo);
    RespuestaAPI eliminarColumna(int planillaId, int columnaId);
    RespuestaAPI guardarNota(int planillaId, int alumnoId, int columnaId, const QJsonValue& nota, const QString& actualizado);
    RespuestaAPI cambiarEstado(int planillaId, const QString& estado);
    RespuestaAPI archivarPlanilla(int planillaId);
    RespuestaAPI desarchivarPlanilla(int planillaId);

    RespuestaAPI listarSesionesAsistencia();
    RespuestaAPI crearSesionAsistencia(int asignacionId, const QString& fecha, const QString& inicio, const QString& fin);
    RespuestaAPI obtenerSesionAsistencia(int sesionId);
    RespuestaAPI guardarAsistencia(int sesionId, int estudianteId, int estadoId, const QString& observacion);
    RespuestaAPI marcarTodosAsistencia(int sesionId, int estadoId);

    RespuestaAPI contarNotificaciones();
    RespuestaAPI listarNotificaciones();
    RespuestaAPI marcarNotificacionLeida(int notificacionId);
    RespuestaAPI marcarTodasNotificacionesLeidas();

    RespuestaAPI listarUsuariosAdmin();
    RespuestaAPI actualizarUsuarioAdmin(int usuarioId, const QString& nombre, const QString& apellido, const QString& correo, int rolId, bool activo);
};

#endif
