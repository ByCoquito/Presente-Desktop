#include "ApiCliente.h"

#include <QEventLoop>
#include <QJsonParseError>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

// Guarda la URL principal de la API.
ApiCliente::ApiCliente(const QString& url) {
    urlBase = url;
}

// Guarda el token recibido al iniciar sesión.
void ApiCliente::guardarToken(const QString& nuevoToken) {
    token = nuevoToken;
}

// Borra el token al cerrar sesión.
void ApiCliente::borrarToken() {
    token.clear();
}

// Envia una solicitud y espera la respuesta del servidor.
RespuestaAPI ApiCliente::enviar(const QString& metodo, const QString& ruta, const QJsonObject& datos) {
    RespuestaAPI resultado;
    QNetworkRequest solicitud(QUrl(urlBase + ruta));
    solicitud.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    solicitud.setRawHeader("Accept", "application/json");

    if (!token.isEmpty()) {
        solicitud.setRawHeader("Authorization", "Bearer " + token.toUtf8());
    }

    QByteArray cuerpo = QJsonDocument(datos).toJson(QJsonDocument::Compact);
    QNetworkReply* respuesta = nullptr;

    if (metodo == "GET") respuesta = red.get(solicitud);
    else if (metodo == "POST") respuesta = red.post(solicitud, cuerpo);
    else if (metodo == "PUT") respuesta = red.put(solicitud, cuerpo);
    else if (metodo == "PATCH") respuesta = red.sendCustomRequest(solicitud, "PATCH", cuerpo);
    else if (metodo == "DELETE") respuesta = red.deleteResource(solicitud);

    if (respuesta == nullptr) {
        resultado.mensaje = "Método HTTP no permitido.";
        return resultado;
    }

    QEventLoop espera;
    QTimer tiempoMaximo;
    tiempoMaximo.setSingleShot(true);

    QObject::connect(respuesta, &QNetworkReply::finished, &espera, &QEventLoop::quit);
    QObject::connect(&tiempoMaximo, &QTimer::timeout, &espera, &QEventLoop::quit);

    tiempoMaximo.start(15000);
    espera.exec();

    if (!tiempoMaximo.isActive()) {
        respuesta->abort();
        resultado.mensaje = "S.I.G.E. demoró demasiado en responder.";
        respuesta->deleteLater();
        return resultado;
    }

    tiempoMaximo.stop();
    resultado.estadoHttp = respuesta->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QByteArray contenido = respuesta->readAll();

    QJsonParseError errorJson;
    resultado.json = QJsonDocument::fromJson(contenido, &errorJson);

    if (errorJson.error != QJsonParseError::NoError) {
        resultado.mensaje = respuesta->errorString();
        if (resultado.mensaje.isEmpty()) resultado.mensaje = "S.I.G.E. devolvió una respuesta no válida.";
    } else if (resultado.json.isObject()) {
        QJsonObject objeto = resultado.json.object();
        QJsonObject error = objeto.value("error").toObject();
        resultado.codigo = error.value("code").toString();
        resultado.mensaje = error.value("message").toString();
    }

    resultado.correcta = resultado.estadoHttp >= 200 && resultado.estadoHttp < 300;
    if (!resultado.correcta && resultado.mensaje.isEmpty()) {
        resultado.mensaje = "No se pudo completar la solicitud.";
    }

    respuesta->deleteLater();
    return resultado;
}

RespuestaAPI ApiCliente::get(const QString& ruta) {
    return enviar("GET", ruta);
}

RespuestaAPI ApiCliente::post(const QString& ruta, const QJsonObject& datos) {
    return enviar("POST", ruta, datos);
}

RespuestaAPI ApiCliente::put(const QString& ruta, const QJsonObject& datos) {
    return enviar("PUT", ruta, datos);
}

RespuestaAPI ApiCliente::patch(const QString& ruta, const QJsonObject& datos) {
    return enviar("PATCH", ruta, datos);
}

RespuestaAPI ApiCliente::eliminar(const QString& ruta) {
    return enviar("DELETE", ruta);
}
