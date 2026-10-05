#include "SigeDAO.h"

#include "../datos/ConfigBD.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>

using namespace std;

// Convierte texto de Qt a texto común de C++.
static string s(const QString& texto) {
    return texto.toStdString();
}

// Convierte texto de C++ a texto de Qt.
static QString q(const string& texto) {
    return QString::fromStdString(texto);
}

// Devuelve un número decimal sin ceros innecesarios.
static QJsonValue numeroJson(const string& texto) {
    if (texto.empty()) return QJsonValue(QJsonValue::Null);
    return QJsonValue(QString::fromStdString(texto).toDouble());
}

// Convierte texto a entero sin cerrar el programa si llega vacío.
static int entero(const string& texto) {
    try {
        if (texto.empty()) return 0;
        return std::stoi(texto);
    } catch (...) {
        return 0;
    }
}

// Devuelve la primera celda de una consulta, o un valor vacío si no hay filas.
static string primeraCelda(const vector<vector<string>>& filas, const string& defecto = "") {
    if (filas.empty() || filas[0].empty()) return defecto;
    return filas[0][0];
}

SigeDAO::SigeDAO(ApiCliente& cliente) : api(cliente) {
}

// Arma una respuesta correcta con el formato de la API.
RespuestaAPI SigeDAO::correcto(const QJsonValue& data) {
    RespuestaAPI respuesta;
    respuesta.correcta = true;
    respuesta.estadoHttp = 200;
    QJsonObject raiz;
    raiz["data"] = data;
    respuesta.json = QJsonDocument(raiz);
    return respuesta;
}

// Arma una respuesta de error para mostrar en pantalla.
RespuestaAPI SigeDAO::error(const QString& mensaje, int estadoHttp, const QString& codigo) {
    RespuestaAPI respuesta;
    respuesta.correcta = false;
    respuesta.estadoHttp = estadoHttp;
    respuesta.codigo = codigo;
    respuesta.mensaje = mensaje;
    return respuesta;
}

// Convierte una fila de curso a JSON.
QJsonObject SigeDAO::cursoJson(const vector<string>& fila) {
    QJsonObject curso;
    curso["id"] = entero(fila[0]);
    curso["anio"] = q(fila[1]);
    curso["division"] = q(fila[2]);
    curso["ciclo_lectivo"] = entero(fila[3]);
    curso["nombre"] = q(fila[1] + " " + fila[2] + " " + fila[3]);
    return curso;
}

// Inicia sesión con la API y abre MySQL para las cargas pesadas.
RespuestaAPI SigeDAO::iniciarSesion(const QString& correo, const QString& clave, Docente& docente) {
    QJsonObject datos;
    datos["email"] = correo;
    datos["password"] = clave;
    datos["device_name"] = "Presente - Escritorio";

    RespuestaAPI respuesta = api.post("/auth/login", datos);
    if (!respuesta.correcta) return respuesta;

    QJsonObject data = respuesta.json.object().value("data").toObject();
    QJsonObject usuario = data.value("user").toObject();
    QString token = data.value("token").toString();

    if (token.isEmpty()) {
        QJsonObject raiz = respuesta.json.object();
        token = raiz.value("token").toString();
        usuario = raiz.value("user").toObject();
    }

    if (token.isEmpty()) {
        respuesta.correcta = false;
        respuesta.mensaje = "S.I.G.E. no devolvió el token de acceso.";
        return respuesta;
    }

    if (!db.conectar(DB_HOST, DB_PUERTO, DB_USUARIO, DB_CLAVE, DB_NOMBRE)) {
        respuesta.correcta = false;
        respuesta.mensaje = "Login correcto, pero no se pudo abrir MySQL.\n" + q(db.ultimoError());
        return respuesta;
    }

    api.guardarToken(token);
    docente.id = usuario.value("id").toInt();
    docenteId = docente.id;

    QString nombre = usuario.value("nombre_completo").toString();
    if (nombre.isEmpty()) nombre = usuario.value("nombre").toString();
    if (nombre.isEmpty()) nombre = usuario.value("name").toString();
    docente.nombre = nombre.toStdString();
    docente.correo = usuario.value("email").toString().toStdString();
    return respuesta;
}

// Cierra la sesión de la API.
RespuestaAPI SigeDAO::cerrarSesion() {
    RespuestaAPI respuesta = api.post("/auth/logout");
    api.borrarToken();
    return respuesta;
}

// Resume los datos principales del panel.
RespuestaAPI SigeDAO::resumenPanel() {
    QJsonObject data;

    vector<vector<string>> resumen = db.consultar(
        "SELECT "
        "(SELECT COUNT(*) FROM cursos WHERE docente_id = " + to_string(docenteId) + " AND archived_at IS NULL), "
        "(SELECT COUNT(*) FROM materias WHERE docente_id = " + to_string(docenteId) + " AND archived_at IS NULL), "
        "(SELECT COUNT(*) FROM estudiantes WHERE docente_id = " + to_string(docenteId) + "), "
        "(SELECT COUNT(*) FROM planillas_notas WHERE docente_id = " + to_string(docenteId) + " AND archived_at IS NULL), "
        "(SELECT COALESCE(ROUND(100 * SUM(CASE WHEN ae.posicion = 1 THEN 1 ELSE 0 END) / NULLIF(COUNT(*),0),1),0) "
            "FROM asistencia_sesiones ass "
            "INNER JOIN materias_cursos amc ON amc.id = ass.materia_curso_id "
            "INNER JOIN cursos ac ON ac.id = amc.curso_id "
            "INNER JOIN estudiantes est ON est.curso_id = ac.id AND est.docente_id = amc.docente_id "
            "LEFT JOIN asistencia_registros ar ON ar.sesion_id = ass.id AND ar.estudiante_id = est.id "
            "LEFT JOIN asistencia_estados ae ON ae.id = ar.estado_id "
            "WHERE amc.docente_id = " + to_string(docenteId) + " AND TIMESTAMP(ass.fecha, ass.hora_inicio) <= NOW()), "
        "(SELECT COUNT(DISTINCT est.id) FROM estudiantes est "
            "INNER JOIN cursos ac ON ac.id = est.curso_id "
            "INNER JOIN materias_cursos amc ON amc.curso_id = ac.id "
            "INNER JOIN asistencia_sesiones ass ON ass.materia_curso_id = amc.id "
            "LEFT JOIN asistencia_registros ar ON ar.sesion_id = ass.id AND ar.estudiante_id = est.id "
            "LEFT JOIN asistencia_estados ae ON ae.id = ar.estado_id "
            "WHERE est.docente_id = " + to_string(docenteId) + " AND amc.docente_id = " + to_string(docenteId)
            + " AND TIMESTAMP(ass.fecha, ass.hora_inicio) <= NOW() AND (ar.id IS NULL OR ae.posicion <> 1 OR ae.posicion IS NULL))"
    );

    if (!resumen.empty()) {
        data["cursos"] = entero(resumen[0][0]);
        data["materias"] = entero(resumen[0][1]);
        data["estudiantes"] = entero(resumen[0][2]);
        data["planillas"] = entero(resumen[0][3]);
        data["asistencia"] = QString::fromStdString(resumen[0][4]).toDouble();
        data["alertas"] = entero(resumen[0][5]);
    }

    QJsonArray clases;
    string clasesSql =
        "SELECT m.nombre, CONCAT(c.anio, ' ', c.division, ' ', c.ciclo_lectivo), "
        "CONCAT(TIME_FORMAT(s.hora_inicio, '%H:%i'), ' - ', TIME_FORMAT(s.hora_fin, '%H:%i')) "
        "FROM asistencia_sesiones s "
        "INNER JOIN materias_cursos mc ON mc.id = s.materia_curso_id "
        "INNER JOIN materias m ON m.id = mc.materia_id "
        "INNER JOIN cursos c ON c.id = mc.curso_id "
        "WHERE mc.docente_id = " + to_string(docenteId) + " AND s.fecha = CURDATE() "
        "ORDER BY s.hora_inicio LIMIT 4";

    for (const vector<string>& fila : db.consultar(clasesSql)) {
        QJsonObject clase;
        clase["materia"] = q(fila[0]);
        clase["curso"] = q(fila[1]);
        clase["horario"] = q(fila[2]);
        clases.append(clase);
    }

    QJsonArray alertas;
    string alertasSql =
        "SELECT CONCAT(e.apellido, IF(e.segundo_apellido IS NULL OR e.segundo_apellido='', '', CONCAT(' ', e.segundo_apellido)), ', ', e.nombre, IF(e.segundo_nombre IS NULL OR e.segundo_nombre='', '', CONCAT(' ', e.segundo_nombre))), "
        "ROUND(100 * SUM(CASE WHEN ae.posicion = 1 THEN 1 ELSE 0 END) / NULLIF(COUNT(s.id),0),1), "
        "SUM(CASE WHEN ar.id IS NULL THEN 1 ELSE 0 END) "
        "FROM estudiantes e "
        "INNER JOIN cursos c ON c.id = e.curso_id "
        "INNER JOIN materias_cursos mc ON mc.curso_id = c.id AND mc.docente_id = e.docente_id "
        "INNER JOIN asistencia_sesiones s ON s.materia_curso_id = mc.id "
        "LEFT JOIN asistencia_registros ar ON ar.sesion_id = s.id AND ar.estudiante_id = e.id "
        "LEFT JOIN asistencia_estados ae ON ae.id = ar.estado_id "
        "WHERE e.docente_id = " + to_string(docenteId) + " AND TIMESTAMP(s.fecha, s.hora_inicio) <= NOW() "
        "GROUP BY e.id, e.apellido, e.segundo_apellido, e.nombre, e.segundo_nombre "
        "HAVING SUM(CASE WHEN ar.id IS NULL THEN 1 ELSE 0 END) > 0 OR ROUND(100 * SUM(CASE WHEN ae.posicion = 1 THEN 1 ELSE 0 END) / NULLIF(COUNT(s.id),0),1) < 75 "
        "ORDER BY 3 DESC, 2 ASC "
        "LIMIT 3";

    for (const vector<string>& fila : db.consultar(alertasSql)) {
        QJsonObject alerta;
        alerta["nombre"] = q(fila[0]);
        alerta["asistencia"] = QString::fromStdString(fila[1]).toDouble();
        alerta["estado"] = entero(fila[2]) > 0 ? "pendiente" : "riesgo";
        alertas.append(alerta);
    }

    data["clases"] = clases;
    data["alertas_lista"] = alertas;
    return correcto(data);
}

// Lista los cursos del docente.
RespuestaAPI SigeDAO::listarCursos() {
    string sql =
        "SELECT id, anio, division, ciclo_lectivo "
        "FROM cursos "
        "WHERE docente_id = " + to_string(docenteId) + " AND archived_at IS NULL "
        "ORDER BY ciclo_lectivo DESC, anio, division";

    QJsonArray data;
    for (const vector<string>& fila : db.consultar(sql)) data.append(cursoJson(fila));
    return correcto(data);
}

// Crea un curso o actualiza uno existente del docente.
RespuestaAPI SigeDAO::guardarCurso(int cursoId, const QString& anio, const QString& division, int cicloLectivo) {
    if (anio.trimmed().isEmpty() || division.trimmed().isEmpty() || cicloLectivo < 2000 || cicloLectivo > 2100) {
        return error("Complete el año, la división y un ciclo lectivo válido.");
    }

    string sql;
    if (cursoId <= 0) {
        sql = "INSERT INTO cursos (docente_id, anio, division, ciclo_lectivo) VALUES ("
            + to_string(docenteId) + ", '" + db.escapar(s(anio.trimmed())) + "', '"
            + db.escapar(s(division.trimmed())) + "', " + to_string(cicloLectivo) + ")";
    } else {
        sql = "UPDATE cursos SET anio = '" + db.escapar(s(anio.trimmed()))
            + "', division = '" + db.escapar(s(division.trimmed()))
            + "', ciclo_lectivo = " + to_string(cicloLectivo)
            + " WHERE id = " + to_string(cursoId) + " AND docente_id = " + to_string(docenteId);
    }

    if (!db.ejecutar(sql)) return error(q(db.ultimoError()));
    return listarCursos();
}

// Archiva un curso sin eliminar su información relacionada.
RespuestaAPI SigeDAO::archivarCurso(int cursoId) {
    string sql = "UPDATE cursos SET archived_at = NOW() WHERE id = " + to_string(cursoId)
        + " AND docente_id = " + to_string(docenteId);
    if (!db.ejecutar(sql)) return error(q(db.ultimoError()));
    return listarCursos();
}

// Lista las materias del docente.
RespuestaAPI SigeDAO::listarMaterias() {
    string sql =
        "SELECT id, nombre "
        "FROM materias "
        "WHERE docente_id = " + to_string(docenteId) + " AND archived_at IS NULL "
        "ORDER BY nombre";

    QJsonArray data;
    for (const vector<string>& fila : db.consultar(sql)) {
        QJsonObject materia;
        materia["id"] = entero(fila[0]);
        materia["nombre"] = q(fila[1]);
        data.append(materia);
    }
    return correcto(data);
}

// Crea una materia o modifica su nombre.
RespuestaAPI SigeDAO::guardarMateria(int materiaId, const QString& nombre) {
    if (nombre.trimmed().isEmpty()) return error("Ingrese el nombre de la materia.");

    string sql;
    if (materiaId <= 0) {
        sql = "INSERT INTO materias (docente_id, nombre) VALUES (" + to_string(docenteId)
            + ", '" + db.escapar(s(nombre.trimmed())) + "')";
    } else {
        sql = "UPDATE materias SET nombre = '" + db.escapar(s(nombre.trimmed()))
            + "' WHERE id = " + to_string(materiaId) + " AND docente_id = " + to_string(docenteId);
    }

    if (!db.ejecutar(sql)) return error(q(db.ultimoError()));
    return listarMaterias();
}

// Archiva una materia sin borrar planillas ni calificaciones.
RespuestaAPI SigeDAO::archivarMateria(int materiaId) {
    string sql = "UPDATE materias SET archived_at = NOW() WHERE id = " + to_string(materiaId)
        + " AND docente_id = " + to_string(docenteId);
    if (!db.ejecutar(sql)) return error(q(db.ultimoError()));
    return listarMaterias();
}

// Vincula una materia con un curso del mismo docente.
RespuestaAPI SigeDAO::asignarMateriaCurso(int materiaId, int cursoId) {
    string validar =
        "SELECT m.id FROM materias m INNER JOIN cursos c ON c.docente_id = m.docente_id "
        "WHERE m.id = " + to_string(materiaId) + " AND c.id = " + to_string(cursoId)
        + " AND m.docente_id = " + to_string(docenteId)
        + " AND m.archived_at IS NULL AND c.archived_at IS NULL LIMIT 1";
    if (db.consultar(validar).empty()) return error("La materia o el curso no pertenecen al docente.");

    string sql = "INSERT INTO materias_cursos (docente_id, materia_id, curso_id) VALUES ("
        + to_string(docenteId) + ", " + to_string(materiaId) + ", " + to_string(cursoId) + ") "
        "ON DUPLICATE KEY UPDATE id = id";
    if (!db.ejecutar(sql)) return error(q(db.ultimoError()));
    return listarAsignaciones();
}

// Lista las materias asignadas a cursos.
RespuestaAPI SigeDAO::listarAsignaciones() {
    string sql =
        "SELECT mc.id, m.id, m.nombre, c.id, c.anio, c.division, c.ciclo_lectivo "
        "FROM materias_cursos mc "
        "INNER JOIN materias m ON m.id = mc.materia_id "
        "INNER JOIN cursos c ON c.id = mc.curso_id "
        "WHERE mc.docente_id = " + to_string(docenteId) + " "
        "AND m.archived_at IS NULL AND c.archived_at IS NULL "
        "ORDER BY m.nombre, c.ciclo_lectivo DESC, c.anio, c.division";

    QJsonArray data;
    for (const vector<string>& fila : db.consultar(sql)) {
        QJsonObject materia;
        materia["id"] = entero(fila[1]);
        materia["nombre"] = q(fila[2]);

        QJsonObject curso;
        curso["id"] = entero(fila[3]);
        curso["anio"] = q(fila[4]);
        curso["division"] = q(fila[5]);
        curso["ciclo_lectivo"] = entero(fila[6]);
        curso["nombre"] = q(fila[4] + " " + fila[5] + " " + fila[6]);

        QJsonObject asignacion;
        asignacion["id"] = entero(fila[0]);
        asignacion["materia"] = materia;
        asignacion["curso"] = curso;
        data.append(asignacion);
    }
    return correcto(data);
}

// Lista estudiantes, con filtro opcional por curso.
RespuestaAPI SigeDAO::listarEstudiantes(int cursoId) {
    string sql =
        "SELECT e.id, e.nombre, IFNULL(e.segundo_nombre,''), e.apellido, IFNULL(e.segundo_apellido,''), "
        "IFNULL(e.dni,''), IFNULL(e.legajo,e.id), IFNULL(c.id,0), IFNULL(c.anio,''), IFNULL(c.division,''), IFNULL(c.ciclo_lectivo,0) "
        "FROM estudiantes e "
        "LEFT JOIN cursos c ON c.id = e.curso_id "
        "WHERE e.docente_id = " + to_string(docenteId) + " ";

    if (cursoId > 0) sql += "AND e.curso_id = " + to_string(cursoId) + " ";
    sql += "ORDER BY e.apellido, e.segundo_apellido, e.nombre, e.segundo_nombre";

    QJsonArray data;
    for (const vector<string>& fila : db.consultar(sql)) {
        QString nombre = (q(fila[3] + " " + fila[4]).trimmed() + ", " + q(fila[1] + " " + fila[2]).trimmed()).trimmed();
        QString curso = q(fila[8] + " " + fila[9] + " " + fila[10]).trimmed();

        QJsonObject estudiante;
        estudiante["id"] = entero(fila[0]);
        estudiante["identificador"] = QString::fromStdString(fila[6]).toInt();
        estudiante["nombre"] = q(fila[1]);
        estudiante["segundo_nombre"] = q(fila[2]);
        estudiante["apellido"] = q(fila[3]);
        estudiante["segundo_apellido"] = q(fila[4]);
        estudiante["nombre_completo"] = nombre;
        estudiante["nombre_listado"] = nombre;
        estudiante["dni"] = q(fila[5]);
        estudiante["curso_id"] = entero(fila[7]);
        estudiante["curso_nombre"] = curso;
        data.append(estudiante);
    }
    return correcto(data);
}


// Crea o actualiza un estudiante del docente.
RespuestaAPI SigeDAO::guardarEstudiante(int estudianteId, const QString& nombre, const QString& segundoNombre,
    const QString& apellido, const QString& segundoApellido, const QString& dni, int cursoId) {
    if (nombre.trimmed().isEmpty() || apellido.trimmed().isEmpty()) {
        return error("Complete el nombre y el apellido.");
    }

    if (cursoId > 0) {
        string validar = "SELECT id FROM cursos WHERE id = " + to_string(cursoId)
            + " AND docente_id = " + to_string(docenteId) + " AND archived_at IS NULL LIMIT 1";
        if (db.consultar(validar).empty()) return error("El curso seleccionado no pertenece al docente.");
    }

    string cursoSql = cursoId > 0 ? to_string(cursoId) : "NULL";
    string dniSql = dni.trimmed().isEmpty() ? "NULL" : "'" + db.escapar(s(dni.trimmed())) + "'";
    string segundoNombreSql = segundoNombre.trimmed().isEmpty() ? "NULL" : "'" + db.escapar(s(segundoNombre.trimmed())) + "'";
    string segundoApellidoSql = segundoApellido.trimmed().isEmpty() ? "NULL" : "'" + db.escapar(s(segundoApellido.trimmed())) + "'";
    string sql;

    if (estudianteId <= 0) {
        sql = "INSERT INTO estudiantes (docente_id, curso_id, nombre, segundo_nombre, apellido, segundo_apellido, dni) VALUES ("
            + to_string(docenteId) + ", " + cursoSql + ", '" + db.escapar(s(nombre.trimmed())) + "', "
            + segundoNombreSql + ", '" + db.escapar(s(apellido.trimmed())) + "', " + segundoApellidoSql + ", " + dniSql + ")";
        if (!db.ejecutar(sql)) return error(q(db.ultimoError()));
        string nuevoId = primeraCelda(db.consultar("SELECT LAST_INSERT_ID()"));
        if (!nuevoId.empty()) db.ejecutar("UPDATE estudiantes SET legajo = id WHERE id = " + nuevoId + " AND legajo IS NULL");
    } else {
        sql = "UPDATE estudiantes SET curso_id = " + cursoSql + ", nombre = '" + db.escapar(s(nombre.trimmed()))
            + "', segundo_nombre = " + segundoNombreSql + ", apellido = '" + db.escapar(s(apellido.trimmed()))
            + "', segundo_apellido = " + segundoApellidoSql + ", dni = " + dniSql
            + " WHERE id = " + to_string(estudianteId) + " AND docente_id = " + to_string(docenteId);
        if (!db.ejecutar(sql)) return error(q(db.ultimoError()));
    }

    return listarEstudiantes();
}

// Elimina un estudiante solamente si pertenece al docente actual.
RespuestaAPI SigeDAO::eliminarEstudiante(int estudianteId) {
    string sql = "DELETE FROM estudiantes WHERE id = " + to_string(estudianteId)
        + " AND docente_id = " + to_string(docenteId);
    if (!db.ejecutar(sql)) return error(q(db.ultimoError()));
    return listarEstudiantes();
}

// Lista planillas del docente.
RespuestaAPI SigeDAO::listarPlanillas() {
    string sql =
        "SELECT p.id, p.titulo, p.materia, p.curso, p.estado, IF(p.archived_at IS NULL,0,1), "
        "IFNULL(m.id,0), IFNULL(m.nombre,p.materia), IFNULL(c.id,0), IFNULL(c.anio,''), IFNULL(c.division,''), IFNULL(c.ciclo_lectivo,0) "
        "FROM planillas_notas p "
        "LEFT JOIN materias_cursos mc ON mc.id = p.materia_curso_id "
        "LEFT JOIN materias m ON m.id = mc.materia_id "
        "LEFT JOIN cursos c ON c.id = mc.curso_id "
        "WHERE p.docente_id = " + to_string(docenteId) + " "
        "ORDER BY p.archived_at IS NULL DESC, p.updated_at DESC";

    QJsonArray data;
    for (const vector<string>& fila : db.consultar(sql)) {
        QJsonObject materia;
        materia["id"] = entero(fila[6]);
        materia["nombre"] = q(fila[7]);

        QJsonObject curso;
        curso["id"] = entero(fila[8]);
        curso["anio"] = q(fila[9]);
        curso["division"] = q(fila[10]);
        curso["ciclo_lectivo"] = entero(fila[11]);
        curso["nombre"] = q(fila[9] + " " + fila[10] + " " + fila[11]).trimmed();

        QJsonObject planilla;
        planilla["id"] = entero(fila[0]);
        planilla["titulo"] = q(fila[1]);
        planilla["materia"] = materia;
        planilla["curso"] = curso;
        planilla["estado"] = q(fila[4]);
        planilla["archivada"] = fila[5] == "1";
        data.append(planilla);
    }
    return correcto(data);
}

// Agrega a la planilla los estudiantes del curso que falten.
void SigeDAO::sincronizarAlumnosPlanilla(int planillaId) {
    string sql =
        "INSERT INTO alumnos_planilla (planilla_id, nombre, legajo, posicion) "
        "SELECT p.id, CONCAT(e.apellido, IF(e.segundo_apellido IS NULL OR e.segundo_apellido='', '', CONCAT(' ', e.segundo_apellido)), ', ', e.nombre, IF(e.segundo_nombre IS NULL OR e.segundo_nombre='', '', CONCAT(' ', e.segundo_nombre))), "
        "e.id, 999 "
        "FROM planillas_notas p "
        "INNER JOIN materias_cursos mc ON mc.id = p.materia_curso_id "
        "INNER JOIN estudiantes e ON e.curso_id = mc.curso_id AND e.docente_id = p.docente_id "
        "LEFT JOIN alumnos_planilla ap ON ap.planilla_id = p.id AND ap.legajo = e.id "
        "WHERE p.id = " + to_string(planillaId) + " AND p.docente_id = " + to_string(docenteId) + " AND ap.id IS NULL";

    db.ejecutar(sql);
}

// Devuelve una planilla completa.
QJsonObject SigeDAO::planillaCompleta(int planillaId) {
    sincronizarAlumnosPlanilla(planillaId);

    string sql =
        "SELECT p.id, p.titulo, p.estado, p.promedio_minimo, p.promedio_maximo, p.created_at, p.updated_at, "
        "IF(p.archived_at IS NULL,0,1), IFNULL(m.id,0), IFNULL(m.nombre,p.materia), IFNULL(c.id,0), IFNULL(c.anio,''), IFNULL(c.division,''), IFNULL(c.ciclo_lectivo,0) "
        "FROM planillas_notas p "
        "LEFT JOIN materias_cursos mc ON mc.id = p.materia_curso_id "
        "LEFT JOIN materias m ON m.id = mc.materia_id "
        "LEFT JOIN cursos c ON c.id = mc.curso_id "
        "WHERE p.id = " + to_string(planillaId) + " AND p.docente_id = " + to_string(docenteId) + " LIMIT 1";

    vector<vector<string>> filas = db.consultar(sql);
    if (filas.empty()) return QJsonObject();
    vector<string> p = filas[0];

    QJsonObject materia;
    materia["id"] = entero(p[8]);
    materia["nombre"] = q(p[9]);

    QJsonObject curso;
    curso["id"] = entero(p[10]);
    curso["anio"] = q(p[11]);
    curso["division"] = q(p[12]);
    curso["ciclo_lectivo"] = entero(p[13]);
    curso["nombre"] = q(p[11] + " " + p[12] + " " + p[13]).trimmed();

    QJsonArray columnas;
    string sqlColumnas =
        "SELECT id, titulo, puntaje_maximo, calificacion_aprobacion, posicion "
        "FROM columnas_notas WHERE planilla_id = " + to_string(planillaId) + " ORDER BY posicion, id";

    for (const vector<string>& fila : db.consultar(sqlColumnas)) {
        QJsonObject columna;
        columna["id"] = entero(fila[0]);
        columna["titulo"] = q(fila[1]);
        columna["puntaje_maximo"] = QString::fromStdString(fila[2]).toDouble();
        columna["calificacion_aprobacion"] = QString::fromStdString(fila[3]).toDouble();
        columna["posicion"] = entero(fila[4]);
        columnas.append(columna);
    }

    QMap<int, QJsonObject> notasPorAlumno;
    QMap<int, double> sumaPorAlumno;
    QMap<int, int> cantidadPorAlumno;
    string sqlNotas =
        "SELECT ca.alumno_planilla_id, ca.columna_id, IFNULL(ca.nota,''), ca.updated_at "
        "FROM calificaciones ca "
        "INNER JOIN alumnos_planilla ap ON ap.id = ca.alumno_planilla_id "
        "WHERE ap.planilla_id = " + to_string(planillaId);

    for (const vector<string>& nota : db.consultar(sqlNotas)) {
        int alumnoId = entero(nota[0]);
        QJsonObject valorNota;
        valorNota["nota"] = numeroJson(nota[2]);
        valorNota["updated_at"] = q(nota[3]);
        QJsonObject notasAlumno = notasPorAlumno.value(alumnoId);
        notasAlumno[q(nota[1])] = valorNota;
        notasPorAlumno[alumnoId] = notasAlumno;
        if (!nota[2].empty()) {
            sumaPorAlumno[alumnoId] += QString::fromStdString(nota[2]).toDouble();
            cantidadPorAlumno[alumnoId] += 1;
        }
    }

    QJsonArray alumnos;
    string sqlAlumnos =
        "SELECT id, nombre, IFNULL(legajo,''), posicion, IFNULL(promedio_ajustado,''), "
        "promedio_sugerencia_rechazada, promedio_sugerencia_resuelta "
        "FROM alumnos_planilla WHERE planilla_id = " + to_string(planillaId) + " ORDER BY nombre, posicion, id";

    for (const vector<string>& fila : db.consultar(sqlAlumnos)) {
        int alumnoId = entero(fila[0]);
        int cantidad = cantidadPorAlumno.value(alumnoId);
        QJsonValue promedioCalculado(QJsonValue::Null);
        QJsonValue promedioFinal(QJsonValue::Null);
        QString resultado;
        if (cantidad > 0) {
            promedioCalculado = sumaPorAlumno.value(alumnoId) / cantidad;
            promedioFinal = promedioCalculado;
        }
        if (!fila[4].empty()) promedioFinal = QString::fromStdString(fila[4]).toDouble();
        if (!promedioFinal.isNull()) {
            resultado = promedioFinal.toDouble() >= QString::fromStdString(p[3]).toDouble() ? "aprobado" : "desaprobado";
        }

        QJsonObject alumno;
        alumno["id"] = alumnoId;
        alumno["estudiante_id"] = QString::fromStdString(fila[2]).toInt();
        alumno["identificador"] = QString::fromStdString(fila[2]).toInt();
        alumno["nombre_completo"] = q(fila[1]);
        alumno["nombre"] = q(fila[1]);
        alumno["legajo"] = q(fila[2]);
        alumno["calificaciones"] = notasPorAlumno.value(alumnoId);
        alumno["promedio_calculado"] = promedioCalculado;
        alumno["promedio_ajustado"] = fila[4].empty() ? QJsonValue(QJsonValue::Null) : numeroJson(fila[4]);
        alumno["promedio_final"] = promedioFinal;
        alumno["promedio_sugerencia_rechazada"] = fila[5] == "1";
        alumno["promedio_sugerencia_resuelta"] = fila[6] == "1";
        alumno["resultado"] = resultado;
        alumnos.append(alumno);
    }

    QJsonObject planilla;
    planilla["id"] = entero(p[0]);
    planilla["titulo"] = q(p[1]);
    planilla["estado"] = q(p[2]);
    planilla["promedio_minimo"] = QString::fromStdString(p[3]).toDouble();
    planilla["promedio_maximo"] = QString::fromStdString(p[4]).toDouble();
    planilla["created_at"] = q(p[5]);
    planilla["updated_at"] = q(p[6]);
    planilla["archivada"] = p[7] == "1";
    planilla["materia"] = materia;
    planilla["curso"] = curso;
    planilla["columnas"] = columnas;
    planilla["alumnos"] = alumnos;
    return planilla;
}

RespuestaAPI SigeDAO::obtenerPlanilla(int planillaId) {
    QJsonObject planilla = planillaCompleta(planillaId);
    if (planilla.isEmpty()) return error("La planilla no existe o no pertenece al docente.");
    return correcto(planilla);
}

// Crea una planilla desde una asignación materia-curso.
RespuestaAPI SigeDAO::crearPlanilla(const QString& titulo, int asignacionId) {
    string asignacionSql =
        "SELECT mc.id, m.nombre, CONCAT(c.anio, ' ', c.division, ' ', c.ciclo_lectivo) "
        "FROM materias_cursos mc "
        "INNER JOIN materias m ON m.id = mc.materia_id "
        "INNER JOIN cursos c ON c.id = mc.curso_id "
        "WHERE mc.id = " + to_string(asignacionId) + " AND mc.docente_id = " + to_string(docenteId) + " LIMIT 1";

    vector<vector<string>> filas = db.consultar(asignacionSql);
    if (filas.empty()) return error("La asignación no pertenece al docente.");

    string sql =
        "INSERT INTO planillas_notas (docente_id, materia_curso_id, titulo, materia, curso, periodo, estado) VALUES ("
        + to_string(docenteId) + ", " + to_string(asignacionId) + ", '"
        + db.escapar(s(titulo)) + "', '" + db.escapar(filas[0][1]) + "', '"
        + db.escapar(filas[0][2]) + "', NULL, 'activa')";

    if (!db.ejecutar(sql)) return error(q(db.ultimoError()));
    int id = entero(primeraCelda(db.consultar("SELECT LAST_INSERT_ID()")));
    if (id <= 0) return listarPlanillas();
    return obtenerPlanilla(id);
}

// Crea una columna de calificación.
RespuestaAPI SigeDAO::crearColumna(int planillaId, const QString& titulo, double aprobacion, double maximo) {
    if (aprobacion < 7 || aprobacion > maximo) return error("Revise la aprobación y el puntaje máximo.");
    if (planillaCompleta(planillaId).value("estado").toString() == "cerrada") return error("La planilla está cerrada.", 423, "SHEET_LOCKED");

    string tituloFinal = s(titulo.trimmed());
    if (tituloFinal.empty()) {
        string cantidad = primeraCelda(db.consultar("SELECT COUNT(*) + 1 FROM columnas_notas WHERE planilla_id = " + to_string(planillaId)), "1");
        tituloFinal = "Nota " + cantidad;
    }

    string posicion = primeraCelda(db.consultar("SELECT IFNULL(MAX(posicion),0) + 1 FROM columnas_notas WHERE planilla_id = " + to_string(planillaId)), "1");
    string sql =
        "INSERT INTO columnas_notas (planilla_id, titulo, puntaje_maximo, calificacion_aprobacion, posicion) VALUES ("
        + to_string(planillaId) + ", '" + db.escapar(tituloFinal) + "', "
        + s(QString::number(maximo, 'f', 2)) + ", " + s(QString::number(aprobacion, 'f', 2)) + ", " + posicion + ")";

    if (!db.ejecutar(sql)) return error(q(db.ultimoError()));
    return obtenerPlanilla(planillaId);
}

// Edita una columna existente.
RespuestaAPI SigeDAO::editarColumna(int planillaId, int columnaId, const QString& titulo, double aprobacion, double maximo) {
    if (aprobacion < 7 || aprobacion > maximo) return error("Revise la aprobación y el puntaje máximo.");
    if (planillaCompleta(planillaId).value("estado").toString() == "cerrada") return error("La planilla está cerrada.", 423, "SHEET_LOCKED");

    string sql =
        "UPDATE columnas_notas SET titulo = '" + db.escapar(s(titulo.trimmed())) + "', "
        "calificacion_aprobacion = " + s(QString::number(aprobacion, 'f', 2)) + ", "
        "puntaje_maximo = " + s(QString::number(maximo, 'f', 2)) + " "
        "WHERE id = " + to_string(columnaId) + " AND planilla_id = " + to_string(planillaId);

    if (!db.ejecutar(sql)) return error(q(db.ultimoError()));
    return obtenerPlanilla(planillaId);
}

// Elimina una columna y sus calificaciones.
RespuestaAPI SigeDAO::eliminarColumna(int planillaId, int columnaId) {
    if (planillaCompleta(planillaId).value("estado").toString() == "cerrada") return error("La planilla está cerrada.", 423, "SHEET_LOCKED");

    string sql = "DELETE FROM columnas_notas WHERE id = " + to_string(columnaId) + " AND planilla_id = " + to_string(planillaId);
    if (!db.ejecutar(sql)) return error(q(db.ultimoError()));
    return obtenerPlanilla(planillaId);
}

// Guarda una nota y recalcula el promedio del alumno.
RespuestaAPI SigeDAO::guardarNota(int planillaId, int alumnoId, int columnaId, const QJsonValue& nota, const QString& actualizado) {
    vector<vector<string>> datosPlanilla = db.consultar(
        "SELECT estado, promedio_minimo FROM planillas_notas WHERE id = " + to_string(planillaId)
        + " AND docente_id = " + to_string(docenteId) + " LIMIT 1"
    );
    if (datosPlanilla.empty()) return error("La planilla no pertenece al docente.");
    if (datosPlanilla[0][0] == "cerrada") return error("La planilla está cerrada.", 423, "SHEET_LOCKED");

    vector<vector<string>> columnas = db.consultar(
        "SELECT cn.puntaje_maximo "
        "FROM columnas_notas cn "
        "INNER JOIN planillas_notas p ON p.id = cn.planilla_id "
        "INNER JOIN alumnos_planilla ap ON ap.planilla_id = p.id "
        "WHERE cn.id = " + to_string(columnaId) + " "
        "AND ap.id = " + to_string(alumnoId) + " "
        "AND p.id = " + to_string(planillaId) + " "
        "AND p.docente_id = " + to_string(docenteId) + " LIMIT 1"
    );
    if (columnas.empty()) return error("La nota no pertenece a la planilla del docente.");

    vector<vector<string>> existente = db.consultar(
        "SELECT IFNULL(nota,''), updated_at FROM calificaciones WHERE alumno_planilla_id = "
        + to_string(alumnoId) + " AND columna_id = " + to_string(columnaId) + " LIMIT 1"
    );
    if (!existente.empty() && !actualizado.trimmed().isEmpty() && q(existente[0][1]) != actualizado.trimmed()) {
        RespuestaAPI conflicto;
        conflicto.correcta = false;
        conflicto.estadoHttp = 409;
        conflicto.codigo = "SYNC_CONFLICT";
        conflicto.mensaje = "La calificación fue modificada desde otro dispositivo.";
        QJsonObject detalle;
        detalle["code"] = conflicto.codigo;
        detalle["message"] = conflicto.mensaje;
        detalle["server_value"] = numeroJson(existente[0][0]);
        detalle["server_updated_at"] = q(existente[0][1]);
        QJsonObject raiz;
        raiz["error"] = detalle;
        conflicto.json = QJsonDocument(raiz);
        return conflicto;
    }

    string valorSql = "NULL";
    if (!nota.isNull() && !nota.isUndefined()) {
        double valor = nota.toDouble();
        double maximo = QString::fromStdString(columnas[0][0]).toDouble();
        if (valor < 0 || valor > maximo) return error("La nota está fuera del rango permitido.");
        valorSql = s(QString::number(valor, 'f', 2));
    }

    string sql =
        "INSERT INTO calificaciones (alumno_planilla_id, columna_id, nota) VALUES ("
        + to_string(alumnoId) + ", " + to_string(columnaId) + ", " + valorSql + ") "
        "ON DUPLICATE KEY UPDATE nota = VALUES(nota), updated_at = CURRENT_TIMESTAMP";

    if (!db.ejecutar(sql)) return error(q(db.ultimoError()));

    vector<vector<string>> promedio = db.consultar(
        "SELECT AVG(nota) FROM calificaciones WHERE alumno_planilla_id = " + to_string(alumnoId) + " AND nota IS NOT NULL"
    );
    vector<vector<string>> actualizada = db.consultar(
        "SELECT IFNULL(nota,''), updated_at FROM calificaciones WHERE alumno_planilla_id = " + to_string(alumnoId)
        + " AND columna_id = " + to_string(columnaId) + " LIMIT 1"
    );

    QJsonObject data;
    data["alumno_planilla_id"] = alumnoId;
    data["columna_id"] = columnaId;
    data["nota"] = actualizada.empty() ? QJsonValue(QJsonValue::Null) : numeroJson(actualizada[0][0]);
    data["promedio_final"] = promedio.empty() ? QJsonValue(QJsonValue::Null) : numeroJson(promedio[0][0]);
    if (data.value("promedio_final").isNull()) data["resultado"] = "";
    else data["resultado"] = data.value("promedio_final").toDouble() >= QString::fromStdString(datosPlanilla[0][1]).toDouble()
        ? "aprobado" : "desaprobado";
    data["updated_at"] = actualizada.empty() ? "" : q(actualizada[0][1]);
    return correcto(data);
}

// Cambia el estado de una planilla.
RespuestaAPI SigeDAO::cambiarEstado(int planillaId, const QString& estado) {
    string valor = s(estado);
    if (valor != "borrador" && valor != "activa" && valor != "cerrada") return error("Estado inválido.");

    string sql =
        "UPDATE planillas_notas SET estado = '" + db.escapar(valor) + "' "
        "WHERE id = " + to_string(planillaId) + " AND docente_id = " + to_string(docenteId);

    if (!db.ejecutar(sql)) return error(q(db.ultimoError()));
    return obtenerPlanilla(planillaId);
}

RespuestaAPI SigeDAO::archivarPlanilla(int planillaId) {
    string sql = "UPDATE planillas_notas SET archived_at = NOW() WHERE id = " + to_string(planillaId)
        + " AND docente_id = " + to_string(docenteId);
    if (!db.ejecutar(sql)) return error(q(db.ultimoError()));
    return correcto(QJsonObject());
}

RespuestaAPI SigeDAO::desarchivarPlanilla(int planillaId) {
    string sql = "UPDATE planillas_notas SET archived_at = NULL WHERE id = " + to_string(planillaId)
        + " AND docente_id = " + to_string(docenteId);
    if (!db.ejecutar(sql)) return error(q(db.ultimoError()));
    return correcto(QJsonObject());
}

// Lista sesiones de asistencia.
RespuestaAPI SigeDAO::listarSesionesAsistencia() {
    string sql =
        "SELECT s.id, s.fecha, s.hora_inicio, s.hora_fin, s.estado, m.nombre, CONCAT(c.anio, ' ', c.division, ' ', c.ciclo_lectivo) "
        "FROM asistencia_sesiones s "
        "INNER JOIN materias_cursos mc ON mc.id = s.materia_curso_id "
        "INNER JOIN materias m ON m.id = mc.materia_id "
        "INNER JOIN cursos c ON c.id = mc.curso_id "
        "WHERE mc.docente_id = " + to_string(docenteId) + " "
        "ORDER BY CASE WHEN s.fecha >= CURDATE() THEN 0 ELSE 1 END, "
        "CASE WHEN s.fecha >= CURDATE() THEN s.fecha END ASC, "
        "CASE WHEN s.fecha < CURDATE() THEN s.fecha END DESC, "
        "s.hora_inicio ASC";

    QJsonArray data;
    for (const vector<string>& fila : db.consultar(sql)) {
        QJsonObject sesion;
        sesion["id"] = entero(fila[0]);
        sesion["fecha"] = q(fila[1]);
        sesion["hora_inicio"] = q(fila[2]);
        sesion["hora_fin"] = q(fila[3]);
        sesion["estado"] = q(fila[4]);
        sesion["materia"] = q(fila[5]);
        sesion["curso"] = q(fila[6]);
        data.append(sesion);
    }
    return correcto(data);
}

// Crea una sesión de asistencia.
RespuestaAPI SigeDAO::crearSesionAsistencia(int asignacionId, const QString& fecha, const QString& inicio, const QString& fin) {
    if (asignacionId <= 0 || fecha.trimmed().isEmpty() || inicio.trimmed().isEmpty() || fin.trimmed().isEmpty()) {
        return error("Complete materia, fecha y horario.");
    }

    string validar =
        "SELECT id FROM materias_cursos WHERE id = " + to_string(asignacionId)
        + " AND docente_id = " + to_string(docenteId) + " LIMIT 1";

    if (db.consultar(validar).empty()) return error("La asignación no pertenece al docente.");

    string sql =
        "INSERT INTO asistencia_sesiones (materia_curso_id, horario_id, fecha, hora_inicio, hora_fin) VALUES ("
        + to_string(asignacionId) + ", NULL, '" + db.escapar(s(fecha.trimmed())) + "', '"
        + db.escapar(s(inicio.trimmed())) + "', '" + db.escapar(s(fin.trimmed())) + "') "
        "ON DUPLICATE KEY UPDATE id = LAST_INSERT_ID(id)";

    if (!db.ejecutar(sql)) return error(q(db.ultimoError()));

    vector<vector<string>> id = db.consultar("SELECT LAST_INSERT_ID()");
    if (id.empty()) return listarSesionesAsistencia();
    return obtenerSesionAsistencia(entero(primeraCelda(id)));
}

// Devuelve estudiantes y estados de una sesión.
RespuestaAPI SigeDAO::obtenerSesionAsistencia(int sesionId) {
    string sesionSql =
        "SELECT s.id, s.fecha, s.hora_inicio, s.hora_fin, s.estado, s.materia_curso_id, m.nombre, CONCAT(c.anio, ' ', c.division, ' ', c.ciclo_lectivo) "
        "FROM asistencia_sesiones s "
        "INNER JOIN materias_cursos mc ON mc.id = s.materia_curso_id "
        "INNER JOIN materias m ON m.id = mc.materia_id "
        "INNER JOIN cursos c ON c.id = mc.curso_id "
        "WHERE s.id = " + to_string(sesionId) + " AND mc.docente_id = " + to_string(docenteId) + " LIMIT 1";

    vector<vector<string>> filas = db.consultar(sesionSql);
    if (filas.empty()) return error("La asistencia no existe o no pertenece al docente.");
    vector<string> s1 = filas[0];

    vector<vector<string>> estadosExistentes = db.consultar(
        "SELECT COUNT(*) FROM asistencia_estados WHERE materia_curso_id = " + s1[5]
    );
    if (!estadosExistentes.empty() && estadosExistentes[0][0] == "0") {
        db.ejecutar(
            "INSERT INTO asistencia_estados (materia_curso_id, nombre, color, es_base, posicion) VALUES "
            "(" + s1[5] + ", 'Presente', '#12683c', 1, 1), "
            "(" + s1[5] + ", 'Ausente', '#ba1a1a', 1, 2), "
            "(" + s1[5] + ", 'Tarde', '#8a5700', 1, 3), "
            "(" + s1[5] + ", 'Justificada', '#006a61', 1, 4)"
        );
    }

    QJsonArray estados;
    string estadosSql =
        "SELECT id, nombre, color FROM asistencia_estados "
        "WHERE materia_curso_id = " + s1[5] + " ORDER BY posicion, id";

    for (const vector<string>& fila : db.consultar(estadosSql)) {
        QJsonObject estado;
        estado["id"] = entero(fila[0]);
        estado["nombre"] = q(fila[1]);
        estado["color"] = q(fila[2]);
        estados.append(estado);
    }

    QJsonArray estudiantes;
    string estudiantesSql =
        "SELECT e.id, CONCAT(e.apellido, IF(e.segundo_apellido IS NULL OR e.segundo_apellido='', '', CONCAT(' ', e.segundo_apellido)), ', ', e.nombre, IF(e.segundo_nombre IS NULL OR e.segundo_nombre='', '', CONCAT(' ', e.segundo_nombre))), "
        "IFNULL(ar.estado_id,0), IFNULL(ar.observacion,'') "
        "FROM asistencia_sesiones s "
        "INNER JOIN materias_cursos mc ON mc.id = s.materia_curso_id "
        "INNER JOIN estudiantes e ON e.curso_id = mc.curso_id "
        "LEFT JOIN asistencia_registros ar ON ar.sesion_id = s.id AND ar.estudiante_id = e.id "
        "WHERE s.id = " + to_string(sesionId) + " AND e.docente_id = " + to_string(docenteId) + " "
        "ORDER BY e.apellido, e.segundo_apellido, e.nombre, e.segundo_nombre";

    for (const vector<string>& fila : db.consultar(estudiantesSql)) {
        QJsonObject estudiante;
        estudiante["id"] = entero(fila[0]);
        estudiante["nombre"] = q(fila[1]);
        estudiante["estado_id"] = entero(fila[2]);
        estudiante["observacion"] = q(fila[3]);
        estudiantes.append(estudiante);
    }

    QJsonObject sesion;
    sesion["id"] = entero(s1[0]);
    sesion["fecha"] = q(s1[1]);
    sesion["hora_inicio"] = q(s1[2]);
    sesion["hora_fin"] = q(s1[3]);
    sesion["estado"] = q(s1[4]);
    sesion["materia"] = q(s1[6]);
    sesion["curso"] = q(s1[7]);
    sesion["estados"] = estados;
    sesion["estudiantes"] = estudiantes;
    return correcto(sesion);
}

// Guarda la asistencia de un estudiante.
RespuestaAPI SigeDAO::guardarAsistencia(int sesionId, int estudianteId, int estadoId, const QString& observacion) {
    string validar =
        "SELECT s.id FROM asistencia_sesiones s "
        "INNER JOIN materias_cursos mc ON mc.id = s.materia_curso_id "
        "INNER JOIN estudiantes e ON e.curso_id = mc.curso_id "
        "INNER JOIN asistencia_estados ae ON ae.materia_curso_id = mc.id "
        "WHERE s.id = " + to_string(sesionId) + " AND e.id = " + to_string(estudianteId)
        + " AND ae.id = " + to_string(estadoId)
        + " AND s.estado = 'abierta' AND mc.docente_id = " + to_string(docenteId)
        + " AND e.docente_id = " + to_string(docenteId) + " LIMIT 1";

    if (db.consultar(validar).empty()) return error("El registro no pertenece al docente.");

    string sql =
        "INSERT INTO asistencia_registros (sesion_id, estudiante_id, estado_id, observacion) VALUES ("
        + to_string(sesionId) + ", " + to_string(estudianteId) + ", " + to_string(estadoId) + ", '"
        + db.escapar(s(observacion)) + "') "
        "ON DUPLICATE KEY UPDATE estado_id = VALUES(estado_id), observacion = VALUES(observacion), updated_at = CURRENT_TIMESTAMP";

    if (!db.ejecutar(sql)) return error(q(db.ultimoError()));
    return correcto(QJsonObject());
}

// Marca a todos los estudiantes de una sesión con el mismo estado.
RespuestaAPI SigeDAO::marcarTodosAsistencia(int sesionId, int estadoId) {
    string validar =
        "SELECT s.id FROM asistencia_sesiones s "
        "INNER JOIN materias_cursos mc ON mc.id = s.materia_curso_id "
        "INNER JOIN asistencia_estados ae ON ae.materia_curso_id = mc.id "
        "WHERE s.id = " + to_string(sesionId) + " AND ae.id = " + to_string(estadoId)
        + " AND s.estado = 'abierta' AND mc.docente_id = " + to_string(docenteId) + " LIMIT 1";
    if (db.consultar(validar).empty()) return error("No se puede modificar esta asistencia.");

    string sql =
        "INSERT INTO asistencia_registros (sesion_id, estudiante_id, estado_id, observacion) "
        "SELECT s.id, e.id, " + to_string(estadoId) + ", '' "
        "FROM asistencia_sesiones s "
        "INNER JOIN materias_cursos mc ON mc.id = s.materia_curso_id "
        "INNER JOIN estudiantes e ON e.curso_id = mc.curso_id AND e.docente_id = mc.docente_id "
        "WHERE s.id = " + to_string(sesionId) + " AND mc.docente_id = " + to_string(docenteId) + " "
        "ON DUPLICATE KEY UPDATE estado_id = VALUES(estado_id), updated_at = CURRENT_TIMESTAMP";
    if (!db.ejecutar(sql)) return error(q(db.ultimoError()));
    return obtenerSesionAsistencia(sesionId);
}

// Cuenta notificaciones pendientes.
RespuestaAPI SigeDAO::contarNotificaciones() {
    string sql =
        "SELECT COUNT(*) FROM notifications n "
        "LEFT JOIN notification_reads nr ON nr.notification_id = n.id AND nr.user_id = " + to_string(docenteId) + " "
        "WHERE (n.user_id = " + to_string(docenteId) + " AND n.read_at IS NULL) "
        "OR (n.user_id IS NULL AND nr.read_at IS NULL)";

    vector<vector<string>> filas = db.consultar(sql);
    QJsonObject data;
    data["total"] = filas.empty() ? 0 : entero(filas[0][0]);
    return correcto(data);
}

// Lista las notificaciones del usuario.
RespuestaAPI SigeDAO::listarNotificaciones() {
    string sql =
        "SELECT n.id, n.title, n.body, IFNULL(CASE WHEN n.user_id IS NULL THEN nr.read_at ELSE n.read_at END,''), n.created_at, "
        "IFNULL(CONCAT(u.first_name, ' ', u.last_name),'Sistema') "
        "FROM notifications n "
        "LEFT JOIN notification_reads nr ON nr.notification_id = n.id AND nr.user_id = " + to_string(docenteId) + " "
        "LEFT JOIN users u ON u.id = n.sender_id "
        "WHERE n.user_id = " + to_string(docenteId) + " OR n.user_id IS NULL "
        "ORDER BY n.created_at DESC, n.id DESC "
        "LIMIT 100";

    QJsonArray data;
    for (const vector<string>& fila : db.consultar(sql)) {
        QJsonObject item;
        item["id"] = entero(fila[0]);
        item["titulo"] = q(fila[1]);
        item["mensaje"] = q(fila[2]);
        item["leida"] = !fila[3].empty();
        item["fecha"] = q(fila[4]);
        item["origen"] = q(fila[5]);
        data.append(item);
    }
    return correcto(data);
}

// Marca una notificación como leída.
RespuestaAPI SigeDAO::marcarNotificacionLeida(int notificacionId) {
    string sql =
        "UPDATE notifications SET read_at = COALESCE(read_at, NOW()) "
        "WHERE id = " + to_string(notificacionId) + " AND user_id = " + to_string(docenteId);

    if (!db.ejecutar(sql)) return error(q(db.ultimoError()));
    sql = "INSERT INTO notification_reads (notification_id, user_id, read_at) "
        "SELECT id, " + to_string(docenteId) + ", NOW() FROM notifications "
        "WHERE id = " + to_string(notificacionId) + " AND user_id IS NULL "
        "ON DUPLICATE KEY UPDATE read_at = COALESCE(read_at, NOW())";
    if (!db.ejecutar(sql)) return error(q(db.ultimoError()));
    return contarNotificaciones();
}

// Marca todas las notificaciones como leidas.
RespuestaAPI SigeDAO::marcarTodasNotificacionesLeidas() {
    string sql =
        "UPDATE notifications SET read_at = COALESCE(read_at, NOW()) "
        "WHERE user_id = " + to_string(docenteId) + " AND read_at IS NULL";

    if (!db.ejecutar(sql)) return error(q(db.ultimoError()));
    sql = "INSERT INTO notification_reads (notification_id, user_id, read_at) "
        "SELECT n.id, " + to_string(docenteId) + ", NOW() FROM notifications n "
        "LEFT JOIN notification_reads nr ON nr.notification_id = n.id AND nr.user_id = " + to_string(docenteId) + " "
        "WHERE n.user_id IS NULL AND nr.notification_id IS NULL";
    if (!db.ejecutar(sql)) return error(q(db.ultimoError()));
    return contarNotificaciones();
}

// Lista usuarios para la vista de administracion.
RespuestaAPI SigeDAO::listarUsuariosAdmin() {
    vector<vector<string>> rol = db.consultar("SELECT role_id FROM users WHERE id = " + to_string(docenteId) + " LIMIT 1");
    QJsonObject data;
    data["admin"] = !rol.empty() && rol[0][0] == "1";

    QJsonArray usuarios;
    if (data["admin"].toBool()) {
        string sql =
            "SELECT u.id, CONCAT(u.first_name, ' ', u.last_name), u.first_name, u.last_name, u.email, r.id, r.name, u.active, u.created_at "
            "FROM users u "
            "INNER JOIN roles r ON r.id = u.role_id "
            "ORDER BY u.active DESC, u.created_at DESC, u.last_name, u.first_name";

        for (const vector<string>& fila : db.consultar(sql)) {
            QJsonObject usuario;
            usuario["id"] = entero(fila[0]);
            usuario["nombre"] = q(fila[1]);
            usuario["nombre_simple"] = q(fila[2]);
            usuario["apellido"] = q(fila[3]);
            usuario["correo"] = q(fila[4]);
            usuario["rol_id"] = entero(fila[5]);
            usuario["rol"] = q(fila[6]);
            usuario["activo"] = fila[7] == "1";
            usuario["fecha"] = q(fila[8]);
            usuarios.append(usuario);
        }
    }

    data["usuarios"] = usuarios;
    return correcto(data);
}

// Actualiza datos básicos de un usuario desde administración.
RespuestaAPI SigeDAO::actualizarUsuarioAdmin(int usuarioId, const QString& nombre, const QString& apellido, const QString& correo, int rolId, bool activo) {
    vector<vector<string>> rol = db.consultar("SELECT role_id FROM users WHERE id = " + to_string(docenteId) + " LIMIT 1");
    if (rol.empty() || rol[0][0] != "1") return error("Acceso restringido.");
    if (usuarioId <= 0 || nombre.trimmed().isEmpty() || apellido.trimmed().isEmpty() || correo.trimmed().isEmpty()) {
        return error("Complete nombre, apellido y correo.");
    }
    if (rolId != 1 && rolId != 2) return error("Rol inválido.");

    string sql =
        "UPDATE users SET first_name = '" + db.escapar(s(nombre.trimmed())) + "', "
        "last_name = '" + db.escapar(s(apellido.trimmed())) + "', "
        "email = '" + db.escapar(s(correo.trimmed())) + "', "
        "role_id = " + to_string(rolId) + ", "
        "active = " + string(activo ? "1" : "0") + " "
        "WHERE id = " + to_string(usuarioId);

    if (!db.ejecutar(sql)) return error(q(db.ultimoError()));
    return listarUsuariosAdmin();
}
