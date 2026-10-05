#ifndef API_CLIENTE_H
#define API_CLIENTE_H

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QString>

// Guarda el resultado de una solicitud a la API.
struct RespuestaAPI {
    bool correcta = false;
    int estadoHttp = 0;
    QString codigo;
    QString mensaje;
    QJsonDocument json;
};

// Se encarga de enviar y recibir JSON por HTTPS.
class ApiCliente {
private:
    QNetworkAccessManager red;
    QString urlBase;
    QString token;

    RespuestaAPI enviar(const QString& metodo, const QString& ruta, const QJsonObject& datos = QJsonObject());

public:
    ApiCliente(const QString& url);

    void guardarToken(const QString& nuevoToken);
    void borrarToken();

    RespuestaAPI get(const QString& ruta);
    RespuestaAPI post(const QString& ruta, const QJsonObject& datos = QJsonObject());
    RespuestaAPI put(const QString& ruta, const QJsonObject& datos);
    RespuestaAPI patch(const QString& ruta, const QJsonObject& datos);
    RespuestaAPI eliminar(const QString& ruta);
};

#endif
