#include "api/ApiCliente.h"
#include "dao/SigeDAO.h"
#include "datos/ConfigAPI.h"
#include "modelo/Docente.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QComboBox>
#include <QColor>
#include <QCloseEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QDoubleValidator>
#include <QDate>
#include <QFormLayout>
#include <QFrame>
#include <QFile>
#include <QFileDialog>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMap>
#include <QMessageBox>
#include <QPushButton>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTimer>
#include <QTime>
#include <QVBoxLayout>
#include <QWidget>

#include <string>

using namespace std;

// Convierte string de C++ a QString de Qt.
QString qtexto(const string& texto) {
    return QString::fromStdString(texto);
}

// Muestra un número sin ceros decimales innecesarios.
QString numeroVisible(double numero) {
    QString texto = QString::number(numero, 'f', 2);
    while (texto.endsWith('0')) texto.chop(1);
    if (texto.endsWith('.')) texto.chop(1);
    return texto;
}

// Devuelve un saludo breve según la hora actual.
QString saludoActual() {
    int hora = QTime::currentTime().hour();
    if (hora < 12) return "Buenos días";
    if (hora < 20) return "Buenas tardes";
    return "Buenas noches";
}

// Obtiene el arreglo data de una respuesta correcta.
QJsonArray arregloData(const RespuestaAPI& respuesta) {
    if (!respuesta.json.isObject()) return QJsonArray();
    return respuesta.json.object().value("data").toArray();
}

// Obtiene el objeto data de una respuesta correcta.
QJsonObject objetoData(const RespuestaAPI& respuesta) {
    if (!respuesta.json.isObject()) return QJsonObject();
    return respuesta.json.object().value("data").toObject();
}

// Devuelve el nombre de una materia recibida por JSON.
QString nombreMateria(const QJsonObject& objeto) {
    QJsonValue materia = objeto.value("materia");
    if (materia.isObject()) return materia.toObject().value("nombre").toString();
    if (materia.isString()) return materia.toString();
    return objeto.value("materia_nombre").toString();
}

// Devuelve el nombre de un curso recibido por JSON.
QString nombreCurso(const QJsonObject& objeto) {
    QJsonValue curso = objeto.value("curso");
    if (curso.isObject()) {
        QJsonObject datos = curso.toObject();
        QString nombre = datos.value("nombre").toString();
        if (!nombre.isEmpty()) return nombre;
        return datos.value("anio").toString() + " "
            + datos.value("division").toString() + " "
            + QString::number(datos.value("ciclo_lectivo").toInt());
    }
    if (curso.isString()) return curso.toString();
    return objeto.value("curso_nombre").toString();
}

class AplicacionGUI : public QWidget {
private:
    ApiCliente api;
    SigeDAO dao;
    Docente docente;

    QStackedWidget* pantallas = nullptr;
    QListWidget* menu = nullptr;
    QLineEdit* loginCorreo = nullptr;
    QLineEdit* loginClave = nullptr;
    QPushButton* loginBoton = nullptr;

    QTableWidget* tablaCursos = nullptr;
    QTableWidget* tablaMaterias = nullptr;
    QTableWidget* tablaEstudiantes = nullptr;
    QComboBox* filtroCurso = nullptr;
    QMap<int, QString> nombresCursos;

    QStackedWidget* vistasPlanillas = nullptr;
    QTableWidget* tablaPlanillas = nullptr;
    QComboBox* nuevaAsignacion = nullptr;
    QJsonArray asignaciones;

    QLabel* detalleTitulo = nullptr;
    QLabel* detalleInformacion = nullptr;
    QComboBox* detalleEstado = nullptr;
    QComboBox* detalleColumnas = nullptr;
    QTableWidget* tablaDetalle = nullptr;
    QPushButton* nuevaColumnaBoton = nullptr;
    QPushButton* editarColumnaBoton = nullptr;
    QPushButton* borrarColumnaBoton = nullptr;
    QJsonObject planillaActual;
    int planillaActualId = 0;

    QStackedWidget* vistasAsistencias = nullptr;
    QTableWidget* tablaSesionesAsistencia = nullptr;
    QTableWidget* tablaDetalleAsistencia = nullptr;
    QLabel* asistenciaTitulo = nullptr;
    QLabel* asistenciaInfo = nullptr;
    QJsonObject asistenciaActual;
    int asistenciaActualId = 0;

    bool cargandoPlanilla = false;
    bool cargandoAsistencia = false;
    bool sesionIniciada = false;

    QLabel* resumenCursos = nullptr;
    QLabel* resumenMaterias = nullptr;
    QLabel* resumenEstudiantes = nullptr;
    QLabel* resumenPlanillas = nullptr;
    QLabel* resumenPlanillasEstadistica = nullptr;
    QLabel* resumenAsistencia = nullptr;
    QLabel* resumenAlertas = nullptr;
    QTableWidget* tablaClasesPanel = nullptr;
    QTableWidget* tablaAlertasPanel = nullptr;
    QLabel* contadorNotificaciones = nullptr;
    QTableWidget* tablaNotificaciones = nullptr;
    QTableWidget* tablaAdministracion = nullptr;
    QTableWidget* tablaInformes = nullptr;

    QLabel* paginaCursosLabel = nullptr;
    QLabel* paginaMateriasLabel = nullptr;
    QLabel* paginaEstudiantesLabel = nullptr;
    QLabel* paginaAsistenciasLabel = nullptr;
    QLabel* paginaPlanillasLabel = nullptr;
    QLabel* paginaNotificacionesLabel = nullptr;
    QLabel* paginaAdministracionLabel = nullptr;
    QLabel* paginaInformesLabel = nullptr;

    int paginaCursos = 0;
    int paginaMaterias = 0;
    int paginaEstudiantes = 0;
    int paginaAsistencias = 0;
    int paginaPlanillas = 0;
    int paginaNotificaciones = 0;
    int paginaAdministracion = 0;
    int paginaInformes = 0;

    bool cursosCargados = false;
    bool materiasCargadas = false;
    bool estudiantesCargados = false;
    bool planillasCargadas = false;
    bool asignacionesCargadas = false;
    bool asistenciasCargadas = false;
    bool notificacionesCargadas = false;
    bool administracionCargada = false;
    bool informesCargados = false;

public:
    // Constructor principal de la aplicación.
    AplicacionGUI() : QWidget(), api(API_URL), dao(api) {
        setWindowTitle("Presente - S.I.G.E.");
        resize(1280, 800);
        setMinimumSize(1000, 650);
        aplicarEstilo();
        crearLogin();
    }

protected:
    // Revoca el token cuando se cierra la ventana.
    void closeEvent(QCloseEvent* evento) override {
        if (sesionIniciada) {
            dao.cerrarSesion();
            sesionIniciada = false;
        }
        evento->accept();
    }

private:
    // Aplica el estilo general de la aplicación.
    void aplicarEstilo() {
        setStyleSheet(
            "QWidget { background:#f8f9ff; color:#0b1c30; font-family:'Segoe UI'; font-size:13px; }"
            "QFrame#loginCard { background:white; border:1px solid #e2e8f0; border-radius:8px; }"
            "QFrame#panelCard, QFrame#formCard { background:white; border:1px solid #d9e2f1; border-radius:8px; }"
            "QFrame#headerCard { background:transparent; border:none; }"
            "QFrame#sideBar { background:#071f35; border:none; border-right:1px solid #12324d; }"
            "QFrame#loginBrand { background:#d3e4fe; border:none; }"
            "QWidget#loginForm { background:white; border:none; }"
            "QFrame#promoCard { background:rgba(255,255,255,220); border:1px solid #e2e8f0; border-radius:8px; }"
            "QFrame#topBar { background:white; border:none; border-bottom:1px solid #e2e8f0; }"
            "QLabel { background:transparent; border:none; }"
            "QLineEdit, QComboBox, QDoubleSpinBox { background:#eff4ff; border:1px solid #e2e8f0; border-radius:8px; padding:7px 10px; min-height:28px; color:#0b1c30; }"
            "QLineEdit:focus, QComboBox:focus, QDoubleSpinBox:focus { background:white; border:2px solid #006a61; }"
            "QLineEdit:read-only { background:#f1f3f7; color:#75777d; }"
            "QPushButton { background:#006a61; color:white; border:none; border-radius:8px; padding:8px 16px; font-weight:700; min-height:30px; }"
            "QPushButton:hover { background:#005049; }"
            "QPushButton:pressed { background:#003d38; }"
            "QPushButton:disabled { background:#c5c6cd; color:#f8f9ff; }"
            "QPushButton#secondaryButton { background:white; color:#0b1c30; border:1px solid #e2e8f0; }"
            "QPushButton#secondaryButton:hover { background:#eff4ff; border-color:#c5c6cd; }"
            "QPushButton#dangerButton { background:#ba1a1a; color:white; }"
            "QPushButton#dangerButton:hover { background:#93000a; }"
            "QPushButton#heroButton { text-align:left; padding-left:22px; font-size:15px; }"
            "QAbstractItemView { outline:0; }"
            "QTableWidget { background:white; border:1px solid #d9e2f1; border-radius:8px; gridline-color:#e7edf6; selection-background-color:#eef8f6; selection-color:#0b1c30; alternate-background-color:#fbfcff; }"
            "QTableWidget::item { padding:7px; border:none; border-bottom:1px solid #e7edf6; }"
            "QTableWidget::item:selected { background:#eef8f6; color:#0b1c30; border:none; }"
            "QTableWidget::item:focus { border:none; outline:0; }"
            "QHeaderView::section { background:#eff4ff; color:#0b1c30; padding:10px 8px; border:none; border-right:1px solid #e2e8f0; border-bottom:1px solid #d9e2f1; font-weight:700; }"
            "QListWidget { background:#071f35; color:#dce7f4; border:none; padding-top:10px; outline:none; }"
            "QListWidget::item { padding:11px 10px; margin:3px 2px; border-radius:8px; font-weight:600; }"
            "QListWidget::item:hover { background:#123b54; color:white; }"
            "QListWidget::item:selected { background:#087d78; color:white; font-weight:700; }"
            "QComboBox::drop-down { border:none; width:26px; }"
            "QScrollBar:vertical { background:#f3f6fb; width:11px; margin:0; }"
            "QScrollBar::handle:vertical { background:#bcc7d8; border-radius:5px; min-height:28px; }"
            "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }"
        );
    }

    // Crea la pantalla de inicio de sesión.
    void crearLogin() {
        QVBoxLayout* principal = new QVBoxLayout(this);
        principal->setAlignment(Qt::AlignCenter);
        principal->setContentsMargins(24, 24, 24, 24);

        QFrame* tarjeta = new QFrame();
        tarjeta->setObjectName("loginCard");
        tarjeta->setMinimumSize(920, 580);
        tarjeta->setMaximumSize(1180, 680);

        QHBoxLayout* tarjetaLayout = new QHBoxLayout(tarjeta);
        tarjetaLayout->setContentsMargins(0, 0, 0, 0);
        tarjetaLayout->setSpacing(0);

        QFrame* marca = new QFrame();
        marca->setObjectName("loginBrand");
        marca->setMinimumWidth(470);

        QVBoxLayout* marcaLayout = new QVBoxLayout(marca);
        marcaLayout->setContentsMargins(48, 48, 48, 48);
        marcaLayout->setAlignment(Qt::AlignCenter);

        QFrame* promo = new QFrame();
        promo->setObjectName("promoCard");
        promo->setMinimumSize(390, 300);
        promo->setMaximumSize(440, 330);
        QVBoxLayout* promoLayout = new QVBoxLayout(promo);
        promoLayout->setContentsMargins(20, 20, 20, 18);
        promoLayout->setSpacing(10);

        QLabel* promoIcono = new QLabel("+");
        promoIcono->setAlignment(Qt::AlignCenter);
        promoIcono->setFixedSize(44, 44);
        promoIcono->setStyleSheet("background:#b9ece5; color:#006a61; border-radius:10px; font-size:25px; font-weight:800;");

        QLabel* promoTitulo = new QLabel("Panel docente");
        promoTitulo->setStyleSheet("color:#091426; font-size:20px; font-weight:800;");

        QLabel* promoTexto = new QLabel("Cursos, asistencias, estudiantes y notas.");
        promoTexto->setWordWrap(true);
        promoTexto->setStyleSheet("color:#45474c; font-size:14px;");

        QHBoxLayout* barras = new QHBoxLayout();
        barras->setAlignment(Qt::AlignBottom | Qt::AlignLeft);
        barras->setSpacing(14);
        QFrame* barra1 = new QFrame();
        QFrame* barra2 = new QFrame();
        QFrame* barra3 = new QFrame();
        barra1->setFixedSize(82, 48);
        barra2->setFixedSize(82, 78);
        barra3->setFixedSize(82, 64);
        barra1->setStyleSheet("background:#006a61; border-radius:8px;");
        barra2->setStyleSheet("background:#86f2e4; border-radius:8px;");
        barra3->setStyleSheet("background:#86f2e4; border-radius:8px;");
        barras->addWidget(barra1);
        barras->addWidget(barra2);
        barras->addWidget(barra3);

        promoLayout->addWidget(promoIcono);
        promoLayout->addWidget(promoTitulo);
        promoLayout->addWidget(promoTexto);
        promoLayout->addStretch();
        promoLayout->addLayout(barras);
        marcaLayout->addWidget(promo);

        QWidget* formulario = new QWidget();
        formulario->setObjectName("loginForm");
        QVBoxLayout* form = new QVBoxLayout(formulario);
        form->setContentsMargins(48, 42, 48, 42);
        form->setSpacing(10);

        QHBoxLayout* logo = new QHBoxLayout();
        logo->setSpacing(10);
        QLabel* logoMarca = new QLabel("P");
        logoMarca->setAlignment(Qt::AlignCenter);
        logoMarca->setFixedSize(36, 36);
        logoMarca->setStyleSheet("background:#091426; color:white; border-radius:9px; font-size:18px; font-weight:800;");
        QLabel* logoNombre = new QLabel("Presente");
        logoNombre->setStyleSheet("color:#091426; font-size:23px; font-weight:800;");
        logo->addWidget(logoMarca);
        logo->addWidget(logoNombre);
        logo->addStretch();

        QLabel* titulo = new QLabel("Bienvenido de nuevo");
        titulo->setStyleSheet("font-size:29px; font-weight:800; color:#091426;");

        QLabel* subtitulo = new QLabel("Ingresa tus credenciales para acceder a tu panel de gestión docente.");
        subtitulo->setWordWrap(true);
        subtitulo->setStyleSheet("color:#45474c; font-size:14px;");

        QLabel* correoLabel = new QLabel("CORREO ELECTRÓNICO");
        correoLabel->setStyleSheet("color:#45474c; font-size:10px; font-weight:800;");
        QLabel* claveLabel = new QLabel("CONTRASEÑA");
        claveLabel->setStyleSheet("color:#45474c; font-size:10px; font-weight:800;");

        loginCorreo = new QLineEdit();
        loginCorreo->setPlaceholderText("docente@ejemplo.com");
        loginCorreo->setMinimumHeight(44);

        loginClave = new QLineEdit();
        loginClave->setPlaceholderText("Ingresa tu contraseña");
        loginClave->setEchoMode(QLineEdit::Password);
        loginClave->setMinimumHeight(44);

        loginBoton = new QPushButton("ACCEDER");
        loginBoton->setMinimumHeight(46);

        QLabel* ayuda = new QLabel("Conexión segura con sige.torga.com.ar");
        ayuda->setAlignment(Qt::AlignCenter);
        ayuda->setStyleSheet("color:#75777d; font-size:12px;");

        form->addLayout(logo);
        form->addSpacing(24);
        form->addWidget(titulo);
        form->addWidget(subtitulo);
        form->addSpacing(12);
        form->addWidget(correoLabel);
        form->addWidget(loginCorreo);
        form->addWidget(claveLabel);
        form->addWidget(loginClave);
        form->addSpacing(4);
        form->addWidget(loginBoton);
        form->addSpacing(10);
        form->addWidget(ayuda);
        form->addStretch();

        tarjetaLayout->addWidget(formulario, 4);
        tarjetaLayout->addWidget(marca, 6);
        principal->addWidget(tarjeta);

        QLabel* copyright = new QLabel("Copyright 2026 - Presente conectado con S.I.G.E.");
        copyright->setAlignment(Qt::AlignCenter);
        copyright->setStyleSheet("color:#75777d; font-size:11px;");
        principal->addWidget(copyright);

        connect(loginBoton, &QPushButton::clicked, this, [this]() { iniciarSesion(); });
        connect(loginCorreo, &QLineEdit::returnPressed, this, [this]() { iniciarSesion(); });
        connect(loginClave, &QLineEdit::returnPressed, this, [this]() { iniciarSesion(); });
    }

    // Valida los datos contra la API de S.I.G.E.
    void iniciarSesion() {
        QString correo = loginCorreo->text().trimmed();
        QString clave = loginClave->text();

        if (correo.isEmpty() || clave.isEmpty()) {
            QMessageBox::warning(this, "Faltan datos", "Ingresa el correo y la contraseña.");
            return;
        }

        loginBoton->setEnabled(false);
        loginBoton->setText("Conectando...");
        QApplication::processEvents();

        RespuestaAPI respuesta = dao.iniciarSesion(correo, clave, docente);

        loginBoton->setEnabled(true);
        loginBoton->setText("ACCEDER");
        loginClave->clear();

        if (!respuesta.correcta) {
            mostrarError(respuesta, "No se pudo iniciar sesión.");
            return;
        }

        sesionIniciada = true;

        // Espera a que termine la señal del botón antes de borrar el login.
        QTimer::singleShot(0, this, [this]() {
            crearVentanaPrincipal();
        });
    }

    // Cambia el login por el panel principal.
    void crearVentanaPrincipal() {
        QLayout* viejo = layout();
        QLayoutItem* item;
        while ((item = viejo->takeAt(0)) != nullptr) {
            if (item->widget() != nullptr) delete item->widget();
            delete item;
        }
        viejo->setContentsMargins(0, 0, 0, 0);

        QWidget* contenedor = new QWidget();
        QHBoxLayout* principal = new QHBoxLayout(contenedor);
        principal->setContentsMargins(0, 0, 0, 0);
        principal->setSpacing(0);

        QFrame* lateral = new QFrame();
        lateral->setObjectName("sideBar");
        lateral->setFixedWidth(228);
        QVBoxLayout* lateralLayout = new QVBoxLayout(lateral);
        lateralLayout->setContentsMargins(14, 18, 14, 14);
        lateralLayout->setSpacing(8);

        QFrame* marcaCaja = new QFrame();
        marcaCaja->setStyleSheet("background:transparent; border:none;");
        marcaCaja->setFixedHeight(40);
        QHBoxLayout* marcaLayout = new QHBoxLayout(marcaCaja);
        marcaLayout->setContentsMargins(4, 0, 4, 0);
        marcaLayout->setSpacing(9);
        QLabel* marcaIcono = new QLabel("S");
        marcaIcono->setAlignment(Qt::AlignCenter);
        marcaIcono->setFixedSize(34, 34);
        marcaIcono->setStyleSheet("background:#f8f9ff; color:#071f35; border-radius:9px; font-size:17px; font-weight:800;");
        QLabel* marca = new QLabel("S.I.G.E.");
        marca->setStyleSheet("font-size:22px; font-weight:800; color:#86f2e4;");
        marcaLayout->addWidget(marcaIcono);
        marcaLayout->addWidget(marca);
        marcaLayout->addStretch();
        QLabel* marcaSub = new QLabel("Sistema de gestión");
        marcaSub->setFixedHeight(18);
        marcaSub->setStyleSheet("color:#c8d6e5; padding-left:47px; font-size:10px; font-weight:700;");

        menu = new QListWidget();
        menu->setMinimumHeight(520);
        menu->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        menu->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        menu->addItems({"Panel", "Cursos", "Materias", "Asistencias", "Estudiantes", "Planillas de notas", "Notificaciones", "Administración", "Informes", "Cerrar sesión"});
        menu->item(9)->setForeground(QColor("#ba1a1a"));

        QLabel* usuario = new QLabel("Docente\n" + qtexto(docente.nombre));
        usuario->setWordWrap(true);
        usuario->setFixedHeight(76);
        usuario->setStyleSheet("background:#102f49; color:white; border:1px solid #1d4662; border-radius:8px; padding:12px; font-weight:600;");

        lateralLayout->addWidget(marcaCaja);
        lateralLayout->addWidget(marcaSub);
        lateralLayout->addSpacing(10);
        lateralLayout->addWidget(menu);
        lateralLayout->addStretch();
        lateralLayout->addWidget(usuario);

        pantallas = new QStackedWidget();
        pantallas->addWidget(crearPanel());
        pantallas->addWidget(crearCursos());
        pantallas->addWidget(crearMaterias());
        pantallas->addWidget(crearAsistencias());
        pantallas->addWidget(crearEstudiantes());
        pantallas->addWidget(crearPlanillas());
        pantallas->addWidget(crearNotificaciones());
        pantallas->addWidget(crearAdministracion());
        pantallas->addWidget(crearInformes());

        QFrame* barraSuperior = new QFrame();
        barraSuperior->setObjectName("topBar");
        barraSuperior->setFixedHeight(58);
        QHBoxLayout* barraSuperiorLayout = new QHBoxLayout(barraSuperior);
        barraSuperiorLayout->setContentsMargins(24, 0, 24, 0);

        QLabel* contexto = new QLabel("S.I.G.E.");
        contexto->setStyleSheet("color:#45474c; font-weight:600;");
        QLabel* conectado = new QLabel("CONECTADO");
        conectado->setFixedHeight(28);
        conectado->setStyleSheet("background:#d9fbe9; color:#12683c; border-radius:8px; padding:5px 9px; font-size:10px; font-weight:800;");

        QString inicial = qtexto(docente.nombre).trimmed().left(1).toUpper();
        if (inicial.isEmpty()) inicial = "D";
        QLabel* avatar = new QLabel(inicial);
        avatar->setAlignment(Qt::AlignCenter);
        avatar->setFixedSize(34, 34);
        avatar->setStyleSheet("background:#006a61; color:white; border-radius:17px; font-weight:800;");

        QPushButton* campana = botonSecundario("🔔");
        campana->setMinimumHeight(34);
        contadorNotificaciones = new QLabel("0");
        contadorNotificaciones->setAlignment(Qt::AlignCenter);
        contadorNotificaciones->setFixedSize(26, 26);
        contadorNotificaciones->setStyleSheet("background:#ba1a1a; color:white; border-radius:13px; font-size:11px; font-weight:800;");

        barraSuperiorLayout->addWidget(contexto);
        barraSuperiorLayout->addWidget(conectado);
        barraSuperiorLayout->addStretch();
        barraSuperiorLayout->addWidget(campana);
        barraSuperiorLayout->addWidget(contadorNotificaciones);
        barraSuperiorLayout->addWidget(new QLabel(qtexto(docente.nombre)));
        barraSuperiorLayout->addWidget(avatar);

        QWidget* area = new QWidget();
        QVBoxLayout* areaLayout = new QVBoxLayout(area);
        areaLayout->setContentsMargins(0, 0, 0, 0);
        areaLayout->setSpacing(0);
        areaLayout->addWidget(barraSuperior);
        areaLayout->addWidget(pantallas);

        principal->addWidget(lateral);
        principal->addWidget(area);
        viejo->addWidget(contenedor);

        connect(menu, &QListWidget::currentRowChanged, this, [this](int fila) {
            if (fila == 9) {
                close();
            } else if (fila >= 0) {
                pantallas->setCurrentIndex(fila);

                // Cada módulo se consulta solamente la primera vez que se abre.
                if (fila == 1 && !cursosCargados) cargarCursos();
                if (fila == 2 && !materiasCargadas) cargarMaterias();
                if (fila == 3 && !asistenciasCargadas) cargarSesionesAsistencia();
                if (fila == 4) {
                    if (!cursosCargados) cargarCursos();
                    if (!estudiantesCargados) cargarEstudiantes();
                }
                if (fila == 5 && !planillasCargadas) cargarPlanillas();
                if (fila == 6 && !notificacionesCargadas) cargarNotificaciones();
                if (fila == 7 && !administracionCargada) cargarAdministracion();
                if (fila == 8 && !informesCargados) cargarInformes();
            }
        });

        connect(campana, &QPushButton::clicked, this, [this]() { menu->setCurrentRow(6); });

        menu->setCurrentRow(0);
        cargarResumenPanel();
        actualizarContadorNotificaciones();
    }

    // Prepara los margenes de una pantalla.
    void prepararPagina(QVBoxLayout* layout) {
        layout->setContentsMargins(34, 28, 34, 30);
        layout->setSpacing(16);
    }

    // Crea un encabezado para una pantalla.
    QFrame* crearEncabezado(const QString& titulo, const QString& texto) {
        QFrame* caja = new QFrame();
        caja->setObjectName("headerCard");
        QVBoxLayout* layout = new QVBoxLayout(caja);
        layout->setContentsMargins(2, 0, 2, 10);
        layout->setSpacing(5);

        QLabel* tituloLabel = new QLabel(titulo);
        tituloLabel->setStyleSheet("font-size:29px; font-weight:800; color:#091426;");
        QLabel* textoLabel = new QLabel(texto);
        textoLabel->setWordWrap(true);
        textoLabel->setStyleSheet("color:#45474c; font-size:14px;");

        layout->addWidget(tituloLabel);
        layout->addWidget(textoLabel);
        return caja;
    }

    // Crea una barra para botones y filtros.
    QFrame* crearBarra() {
        QFrame* caja = new QFrame();
        caja->setObjectName("formCard");
        QHBoxLayout* layout = new QHBoxLayout(caja);
        layout->setContentsMargins(16, 12, 16, 12);
        layout->setSpacing(10);
        return caja;
    }

    // Crea un botón secundario.
    QPushButton* botonSecundario(const QString& texto) {
        QPushButton* boton = new QPushButton(texto);
        boton->setObjectName("secondaryButton");
        return boton;
    }

    // Prepara una tabla para mostrar datos.
    void prepararTabla(QTableWidget* tabla) {
        tabla->verticalHeader()->setVisible(false);
        tabla->setAlternatingRowColors(true);
        tabla->setSelectionBehavior(QAbstractItemView::SelectRows);
        tabla->setSelectionMode(QAbstractItemView::SingleSelection);
        tabla->setEditTriggers(QAbstractItemView::NoEditTriggers);
        tabla->setFocusPolicy(Qt::NoFocus);
        tabla->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        tabla->verticalHeader()->setDefaultSectionSize(40);
        tabla->setShowGrid(false);
        tabla->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

    // Crea una celda no editable.
    QTableWidgetItem* celda(const QString& texto) {
        QTableWidgetItem* item = new QTableWidgetItem(texto);
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        return item;
    }

    // Muestra solo 10 filas por página.
    void aplicarPaginacion(QTableWidget* tabla, int& pagina, QLabel* etiqueta) {
        if (tabla == nullptr) return;
        const int porPagina = 10;
        int total = tabla->rowCount();
        int paginas = total == 0 ? 1 : ((total - 1) / porPagina) + 1;
        if (pagina < 0) pagina = 0;
        if (pagina >= paginas) pagina = paginas - 1;

        for (int fila = 0; fila < total; fila++) {
            bool visible = fila >= pagina * porPagina && fila < (pagina + 1) * porPagina;
            tabla->setRowHidden(fila, !visible);
        }

        if (etiqueta != nullptr) {
            etiqueta->setText("Página " + QString::number(pagina + 1) + " de " + QString::number(paginas));
        }
    }

    // Crea botones de paginación.
    QFrame* crearPaginador(int* pagina, QLabel*& etiqueta, QTableWidget* tabla) {
        QFrame* caja = crearBarra();
        QPushButton* anterior = botonSecundario("Anterior");
        QPushButton* siguiente = botonSecundario("Siguiente");
        etiqueta = new QLabel("Página 1 de 1");
        etiqueta->setStyleSheet("font-weight:700; color:#45474c;");

        QHBoxLayout* layout = (QHBoxLayout*)caja->layout();
        layout->addStretch();
        layout->addWidget(anterior);
        layout->addWidget(etiqueta);
        layout->addWidget(siguiente);

        connect(anterior, &QPushButton::clicked, this, [this, pagina, etiqueta, tabla]() {
            if (*pagina > 0) (*pagina)--;
            aplicarPaginacion(tabla, *pagina, etiqueta);
        });
        connect(siguiente, &QPushButton::clicked, this, [this, pagina, etiqueta, tabla]() {
            (*pagina)++;
            aplicarPaginacion(tabla, *pagina, etiqueta);
        });
        return caja;
    }

    // Muestra una hora como hh:mm.
    QString horaCorta(const QString& hora) {
        return hora.left(5);
    }

    // Crea una tarjeta con un contador del panel.
    QFrame* crearTarjetaResumen(const QString& titulo, QLabel*& valor) {
        QFrame* tarjeta = new QFrame();
        tarjeta->setObjectName("panelCard");
        tarjeta->setMinimumHeight(118);
        QVBoxLayout* layout = new QVBoxLayout(tarjeta);
        layout->setContentsMargins(18, 17, 18, 17);
        layout->setSpacing(10);

        QLabel* tituloLabel = new QLabel(titulo);
        tituloLabel->setStyleSheet("color:#45474c; font-size:11px; font-weight:800;");
        valor = new QLabel("0");
        valor->setStyleSheet("font-size:31px; font-weight:800; color:#006a61;");

        layout->addWidget(tituloLabel);
        layout->addWidget(valor);
        return tarjeta;
    }

    // Crea una tarjeta de estadistica como en el panel web.
    QFrame* crearTarjetaEstadistica(const QString& titulo, QLabel*& valor, const QString& detalle, const QString& color = "#091426") {
        QFrame* tarjeta = new QFrame();
        tarjeta->setObjectName("panelCard");
        tarjeta->setMinimumHeight(128);
        QVBoxLayout* layout = new QVBoxLayout(tarjeta);
        layout->setContentsMargins(18, 18, 18, 18);
        layout->setSpacing(10);

        QLabel* tituloLabel = new QLabel(titulo.toUpper());
        tituloLabel->setStyleSheet("color:#0b1c30; font-size:11px; font-weight:800;");
        valor = new QLabel("0");
        valor->setStyleSheet("font-size:39px; font-weight:800; color:" + color + ";");
        QLabel* detalleLabel = new QLabel(detalle);
        detalleLabel->setStyleSheet("color:#45474c; font-size:13px;");

        layout->addWidget(tituloLabel);
        layout->addWidget(valor);
        layout->addWidget(detalleLabel);
        return tarjeta;
    }

    // Crea el panel de inicio.
    QWidget* crearPanel() {
        QWidget* pagina = new QWidget();
        QVBoxLayout* layout = new QVBoxLayout(pagina);
        prepararPagina(layout);

        QHBoxLayout* acceso = new QHBoxLayout();
        acceso->setSpacing(14);
        acceso->addWidget(crearTarjetaResumen("Cursos activos", resumenCursos));
        acceso->addWidget(crearTarjetaResumen("Materias", resumenMaterias));
        acceso->addWidget(crearTarjetaResumen("Estudiantes", resumenEstudiantes));
        acceso->addWidget(crearTarjetaResumen("Planillas", resumenPlanillas));

        QHBoxLayout* estadisticas = new QHBoxLayout();
        estadisticas->setSpacing(14);
        estadisticas->addWidget(crearTarjetaEstadistica("Asistencia general", resumenAsistencia, "Promedio de estudiantes", "#091426"));
        estadisticas->addWidget(crearTarjetaEstadistica("Planillas de notas", resumenPlanillasEstadistica, "Planillas creadas", "#091426"));
        estadisticas->addWidget(crearTarjetaEstadistica("Alertas activas", resumenAlertas, "Estudiantes para revisar", "#ba1a1a"));

        QHBoxLayout* grilla = new QHBoxLayout();
        grilla->setSpacing(18);

        QFrame* clases = new QFrame();
        clases->setObjectName("panelCard");
        QVBoxLayout* clasesLayout = new QVBoxLayout(clases);
        clasesLayout->setContentsMargins(18, 18, 18, 18);
        QLabel* clasesTitulo = new QLabel("Clases de hoy");
        clasesTitulo->setStyleSheet("font-size:21px; font-weight:800; color:#091426;");
        tablaClasesPanel = new QTableWidget();
        tablaClasesPanel->setColumnCount(2);
        tablaClasesPanel->setHorizontalHeaderLabels({"Materia", "Curso"});
        prepararTabla(tablaClasesPanel);
        clasesLayout->addWidget(clasesTitulo);
        clasesLayout->addWidget(tablaClasesPanel);

        QFrame* alertas = new QFrame();
        alertas->setObjectName("panelCard");
        QVBoxLayout* alertasLayout = new QVBoxLayout(alertas);
        alertasLayout->setContentsMargins(18, 18, 18, 18);
        QLabel* alertasTitulo = new QLabel("Alertas");
        alertasTitulo->setStyleSheet("font-size:21px; font-weight:800; color:#ba1a1a;");
        tablaAlertasPanel = new QTableWidget();
        tablaAlertasPanel->setColumnCount(2);
        tablaAlertasPanel->setHorizontalHeaderLabels({"Estudiante", "Asistencia"});
        prepararTabla(tablaAlertasPanel);
        alertasLayout->addWidget(alertasTitulo);
        alertasLayout->addWidget(tablaAlertasPanel);

        grilla->addWidget(clases, 2);
        grilla->addWidget(alertas, 1);

        QPushButton* abrirEstudiantes = new QPushButton("Estudiantes");
        abrirEstudiantes->setMinimumHeight(42);

        QFrame* encabezado = crearEncabezado(saludoActual() + ", " + qtexto(docente.nombre), "Resumen de actividad de hoy.");
        ((QVBoxLayout*)encabezado->layout())->addWidget(abrirEstudiantes, 0, Qt::AlignRight);

        layout->addWidget(encabezado);
        layout->addLayout(estadisticas);
        layout->addLayout(acceso);
        layout->addLayout(grilla);
        layout->addStretch();

        connect(abrirEstudiantes, &QPushButton::clicked, this, [this]() { menu->setCurrentRow(4); });
        return pagina;
    }

    // Crea el listado de cursos.
    QWidget* crearCursos() {
        QWidget* pagina = new QWidget();
        QVBoxLayout* layout = new QVBoxLayout(pagina);
        prepararPagina(layout);

        QPushButton* nuevo = new QPushButton("Nuevo curso");
        QPushButton* editar = botonSecundario("Editar");
        QPushButton* archivar = botonSecundario("Archivar");
        QPushButton* actualizar = botonSecundario("Actualizar");
        QFrame* barra = crearBarra();
        ((QHBoxLayout*)barra->layout())->addWidget(nuevo);
        ((QHBoxLayout*)barra->layout())->addWidget(editar);
        ((QHBoxLayout*)barra->layout())->addWidget(archivar);
        ((QHBoxLayout*)barra->layout())->addStretch();
        ((QHBoxLayout*)barra->layout())->addWidget(actualizar);

        tablaCursos = new QTableWidget();
        tablaCursos->setColumnCount(4);
        tablaCursos->setHorizontalHeaderLabels({"ID", "Año", "División", "Ciclo lectivo"});
        prepararTabla(tablaCursos);

        layout->addWidget(crearEncabezado("Cursos", "Cursos, divisiones y ciclo lectivo."));
        layout->addWidget(barra);
        layout->addWidget(tablaCursos);
        layout->addWidget(crearPaginador(&paginaCursos, paginaCursosLabel, tablaCursos));

        connect(actualizar, &QPushButton::clicked, this, [this]() { cargarCursos(); });
        connect(nuevo, &QPushButton::clicked, this, [this]() { dialogoCurso(false); });
        connect(editar, &QPushButton::clicked, this, [this]() { dialogoCurso(true); });
        connect(archivar, &QPushButton::clicked, this, [this]() { archivarCursoSeleccionado(); });
        connect(tablaCursos, &QTableWidget::cellDoubleClicked, this, [this](int, int) { dialogoCurso(true); });
        return pagina;
    }

    // Crea el listado de materias.
    QWidget* crearMaterias() {
        QWidget* pagina = new QWidget();
        QVBoxLayout* layout = new QVBoxLayout(pagina);
        prepararPagina(layout);

        QPushButton* nueva = new QPushButton("Nueva materia");
        QPushButton* editar = botonSecundario("Editar");
        QPushButton* archivar = botonSecundario("Archivar");
        QPushButton* asignar = botonSecundario("Asignar a curso");
        QPushButton* actualizar = botonSecundario("Actualizar");
        QFrame* barra = crearBarra();
        ((QHBoxLayout*)barra->layout())->addWidget(nueva);
        ((QHBoxLayout*)barra->layout())->addWidget(editar);
        ((QHBoxLayout*)barra->layout())->addWidget(archivar);
        ((QHBoxLayout*)barra->layout())->addWidget(asignar);
        ((QHBoxLayout*)barra->layout())->addStretch();
        ((QHBoxLayout*)barra->layout())->addWidget(actualizar);

        tablaMaterias = new QTableWidget();
        tablaMaterias->setColumnCount(2);
        tablaMaterias->setHorizontalHeaderLabels({"ID", "Materia"});
        prepararTabla(tablaMaterias);

        layout->addWidget(crearEncabezado("Materias", "Materias cargadas para el docente."));
        layout->addWidget(barra);
        layout->addWidget(tablaMaterias);
        layout->addWidget(crearPaginador(&paginaMaterias, paginaMateriasLabel, tablaMaterias));

        connect(actualizar, &QPushButton::clicked, this, [this]() { cargarMaterias(); });
        connect(nueva, &QPushButton::clicked, this, [this]() { dialogoMateria(false); });
        connect(editar, &QPushButton::clicked, this, [this]() { dialogoMateria(true); });
        connect(archivar, &QPushButton::clicked, this, [this]() { archivarMateriaSeleccionada(); });
        connect(asignar, &QPushButton::clicked, this, [this]() { dialogoAsignarMateria(); });
        connect(tablaMaterias, &QTableWidget::cellDoubleClicked, this, [this](int, int) { dialogoMateria(true); });
        return pagina;
    }

    // Crea el listado de estudiantes.
    QWidget* crearEstudiantes() {
        QWidget* pagina = new QWidget();
        QVBoxLayout* layout = new QVBoxLayout(pagina);
        prepararPagina(layout);

        filtroCurso = new QComboBox();
        filtroCurso->setMinimumWidth(250);
        QPushButton* nuevo = new QPushButton("Agregar estudiante");
        QPushButton* editar = botonSecundario("Editar");
        QPushButton* eliminar = botonSecundario("Eliminar");
        eliminar->setObjectName("dangerButton");
        QPushButton* actualizar = botonSecundario("Actualizar");
        QFrame* barra = crearBarra();
        ((QHBoxLayout*)barra->layout())->addWidget(nuevo);
        ((QHBoxLayout*)barra->layout())->addWidget(editar);
        ((QHBoxLayout*)barra->layout())->addWidget(eliminar);
        ((QHBoxLayout*)barra->layout())->addSpacing(12);
        ((QHBoxLayout*)barra->layout())->addWidget(new QLabel("Curso:"));
        ((QHBoxLayout*)barra->layout())->addWidget(filtroCurso);
        ((QHBoxLayout*)barra->layout())->addStretch();
        ((QHBoxLayout*)barra->layout())->addWidget(actualizar);

        tablaEstudiantes = new QTableWidget();
        tablaEstudiantes->setColumnCount(4);
        tablaEstudiantes->setHorizontalHeaderLabels({"ID", "Apellido y nombre", "DNI", "Curso"});
        prepararTabla(tablaEstudiantes);

        layout->addWidget(crearEncabezado("Estudiantes", "Listado de estudiantes."));
        layout->addWidget(barra);
        layout->addWidget(tablaEstudiantes);
        layout->addWidget(crearPaginador(&paginaEstudiantes, paginaEstudiantesLabel, tablaEstudiantes));

        connect(actualizar, &QPushButton::clicked, this, [this]() {
            cargarEstudiantes(filtroCurso->currentData().toInt());
        });
        connect(nuevo, &QPushButton::clicked, this, [this]() { dialogoEstudiante(false); });
        connect(editar, &QPushButton::clicked, this, [this]() { dialogoEstudiante(true); });
        connect(eliminar, &QPushButton::clicked, this, [this]() { eliminarEstudianteSeleccionado(); });
        connect(tablaEstudiantes, &QTableWidget::cellDoubleClicked, this, [this](int, int) { dialogoEstudiante(true); });
        return pagina;
    }

    // Crea el módulo de asistencias.
    QWidget* crearAsistencias() {
        QWidget* pagina = new QWidget();
        QVBoxLayout* paginaLayout = new QVBoxLayout(pagina);
        paginaLayout->setContentsMargins(0, 0, 0, 0);

        vistasAsistencias = new QStackedWidget();
        vistasAsistencias->addWidget(crearListadoAsistencias());
        vistasAsistencias->addWidget(crearDetalleAsistencia());
        paginaLayout->addWidget(vistasAsistencias);
        return pagina;
    }

    // Crea la lista de asistencias cargadas.
    QWidget* crearListadoAsistencias() {
        QWidget* pagina = new QWidget();
        QVBoxLayout* layout = new QVBoxLayout(pagina);
        prepararPagina(layout);

        QPushButton* nueva = new QPushButton("Nueva asistencia");
        QPushButton* abrir = new QPushButton("Abrir seleccionada");
        QPushButton* actualizar = botonSecundario("Actualizar");

        QFrame* barra = crearBarra();
        QHBoxLayout* acciones = (QHBoxLayout*)barra->layout();
        acciones->addWidget(nueva);
        acciones->addWidget(abrir);
        acciones->addStretch();
        acciones->addWidget(actualizar);

        tablaSesionesAsistencia = new QTableWidget();
        tablaSesionesAsistencia->setColumnCount(6);
        tablaSesionesAsistencia->setHorizontalHeaderLabels({"ID", "Fecha", "Horario", "Materia", "Curso", "Estado"});
        prepararTabla(tablaSesionesAsistencia);

        layout->addWidget(crearEncabezado("Asistencias", "Buscar asistencias y tomar asistencia."));
        layout->addWidget(barra);
        layout->addWidget(tablaSesionesAsistencia);
        layout->addWidget(crearPaginador(&paginaAsistencias, paginaAsistenciasLabel, tablaSesionesAsistencia));

        connect(actualizar, &QPushButton::clicked, this, [this]() { cargarSesionesAsistencia(); });
        connect(nueva, &QPushButton::clicked, this, [this]() { dialogoNuevaAsistencia(); });
        connect(abrir, &QPushButton::clicked, this, [this]() { abrirAsistenciaSeleccionada(); });
        connect(tablaSesionesAsistencia, &QTableWidget::cellDoubleClicked, this, [this](int, int) { abrirAsistenciaSeleccionada(); });
        return pagina;
    }

    // Crea el detalle de una asistencia.
    QWidget* crearDetalleAsistencia() {
        QWidget* pagina = new QWidget();
        QVBoxLayout* layout = new QVBoxLayout(pagina);
        prepararPagina(layout);

        QFrame* encabezado = new QFrame();
        encabezado->setObjectName("headerCard");
        QVBoxLayout* encabezadoLayout = new QVBoxLayout(encabezado);
        asistenciaTitulo = new QLabel("Asistencia");
        asistenciaTitulo->setStyleSheet("font-size:27px; font-weight:800; color:#091426;");
        asistenciaInfo = new QLabel();
        asistenciaInfo->setStyleSheet("color:#45474c; font-size:14px;");
        encabezadoLayout->addWidget(asistenciaTitulo);
        encabezadoLayout->addWidget(asistenciaInfo);

        QPushButton* volver = botonSecundario("Volver");
        QPushButton* marcarTodos = new QPushButton("Marcar todos presentes");
        QFrame* barra = crearBarra();
        ((QHBoxLayout*)barra->layout())->addWidget(volver);
        ((QHBoxLayout*)barra->layout())->addStretch();
        ((QHBoxLayout*)barra->layout())->addWidget(marcarTodos);

        tablaDetalleAsistencia = new QTableWidget();
        tablaDetalleAsistencia->setColumnCount(4);
        tablaDetalleAsistencia->setHorizontalHeaderLabels({"ID", "Estudiante", "Estado", "Observación"});
        prepararTabla(tablaDetalleAsistencia);
        tablaDetalleAsistencia->setEditTriggers(QAbstractItemView::NoEditTriggers);

        layout->addWidget(encabezado);
        layout->addWidget(barra);
        layout->addWidget(tablaDetalleAsistencia);

        connect(volver, &QPushButton::clicked, this, [this]() {
            vistasAsistencias->setCurrentIndex(0);
            cargarSesionesAsistencia();
        });
        connect(marcarTodos, &QPushButton::clicked, this, [this]() { marcarTodosPresentes(); });
        return pagina;
    }

    // Crea el módulo de planillas y su vista de detalle.
    QWidget* crearPlanillas() {
        QWidget* pagina = new QWidget();
        QVBoxLayout* paginaLayout = new QVBoxLayout(pagina);
        paginaLayout->setContentsMargins(0, 0, 0, 0);

        vistasPlanillas = new QStackedWidget();
        vistasPlanillas->addWidget(crearListadoPlanillas());
        vistasPlanillas->addWidget(crearDetallePlanilla());
        paginaLayout->addWidget(vistasPlanillas);
        return pagina;
    }

    // Crea la pantalla de notificaciones.
    QWidget* crearNotificaciones() {
        QWidget* pagina = new QWidget();
        QVBoxLayout* layout = new QVBoxLayout(pagina);
        prepararPagina(layout);

        QPushButton* marcarTodas = new QPushButton("Marcar todas como leídas");
        QPushButton* marcarUna = botonSecundario("Marcar seleccionada");
        QPushButton* actualizar = botonSecundario("Actualizar");

        QFrame* barra = crearBarra();
        QHBoxLayout* acciones = (QHBoxLayout*)barra->layout();
        acciones->addWidget(marcarTodas);
        acciones->addWidget(marcarUna);
        acciones->addStretch();
        acciones->addWidget(actualizar);

        tablaNotificaciones = new QTableWidget();
        tablaNotificaciones->setColumnCount(5);
        tablaNotificaciones->setHorizontalHeaderLabels({"ID", "Título", "Mensaje", "Origen", "Fecha"});
        prepararTabla(tablaNotificaciones);

        layout->addWidget(crearEncabezado("Notificaciones", "Mensajes y avisos."));
        layout->addWidget(barra);
        layout->addWidget(tablaNotificaciones);
        layout->addWidget(crearPaginador(&paginaNotificaciones, paginaNotificacionesLabel, tablaNotificaciones));

        connect(actualizar, &QPushButton::clicked, this, [this]() { cargarNotificaciones(); });
        connect(marcarUna, &QPushButton::clicked, this, [this]() { marcarNotificacionActual(); });
        connect(marcarTodas, &QPushButton::clicked, this, [this]() { marcarTodasNotificaciones(); });
        connect(tablaNotificaciones, &QTableWidget::cellDoubleClicked, this, [this](int, int) { marcarNotificacionActual(); });
        return pagina;
    }

    // Crea la pantalla de administración.
    QWidget* crearAdministracion() {
        QWidget* pagina = new QWidget();
        QVBoxLayout* layout = new QVBoxLayout(pagina);
        prepararPagina(layout);

        QPushButton* actualizar = botonSecundario("Actualizar");
        QFrame* barra = crearBarra();
        ((QHBoxLayout*)barra->layout())->addStretch();
        ((QHBoxLayout*)barra->layout())->addWidget(actualizar);

        tablaAdministracion = new QTableWidget();
        tablaAdministracion->setColumnCount(6);
        tablaAdministracion->setHorizontalHeaderLabels({"ID", "Usuario", "Correo", "Rol", "Estado", "Alta"});
        prepararTabla(tablaAdministracion);

        layout->addWidget(crearEncabezado("Administración", "Usuarios, roles y estado."));
        layout->addWidget(barra);
        layout->addWidget(tablaAdministracion);
        layout->addWidget(crearPaginador(&paginaAdministracion, paginaAdministracionLabel, tablaAdministracion));

        connect(actualizar, &QPushButton::clicked, this, [this]() { cargarAdministracion(); });
        connect(tablaAdministracion, &QTableWidget::cellDoubleClicked, this, [this](int, int) { dialogoEditarUsuario(); });
        return pagina;
    }

    // Crea un informe simple de estudiantes listo para exportar.
    QWidget* crearInformes() {
        QWidget* pagina = new QWidget();
        QVBoxLayout* layout = new QVBoxLayout(pagina);
        prepararPagina(layout);

        QPushButton* exportar = new QPushButton("Exportar CSV");
        QPushButton* actualizar = botonSecundario("Actualizar");
        QFrame* barra = crearBarra();
        ((QHBoxLayout*)barra->layout())->addWidget(exportar);
        ((QHBoxLayout*)barra->layout())->addStretch();
        ((QHBoxLayout*)barra->layout())->addWidget(actualizar);

        tablaInformes = new QTableWidget();
        tablaInformes->setColumnCount(4);
        tablaInformes->setHorizontalHeaderLabels({"ID", "Estudiante", "Curso", "DNI"});
        prepararTabla(tablaInformes);

        layout->addWidget(crearEncabezado("Informes", "Resumen de estudiantes y cursos."));
        layout->addWidget(barra);
        layout->addWidget(tablaInformes);
        layout->addWidget(crearPaginador(&paginaInformes, paginaInformesLabel, tablaInformes));

        connect(actualizar, &QPushButton::clicked, this, [this]() { cargarInformes(); });
        connect(exportar, &QPushButton::clicked, this, [this]() { exportarInforme(); });
        return pagina;
    }


    // Crea la lista principal de planillas.
    QWidget* crearListadoPlanillas() {
        QWidget* pagina = new QWidget();
        QVBoxLayout* layout = new QVBoxLayout(pagina);
        prepararPagina(layout);

        QPushButton* nueva = new QPushButton("Nueva planilla");
        QPushButton* abrir = new QPushButton("Abrir seleccionada");
        QPushButton* archivar = botonSecundario("Archivar / Desarchivar");
        QPushButton* actualizar = botonSecundario("Actualizar");

        QFrame* barra = crearBarra();
        QHBoxLayout* acciones = (QHBoxLayout*)barra->layout();
        acciones->addWidget(nueva);
        acciones->addWidget(abrir);
        acciones->addWidget(archivar);
        acciones->addStretch();
        acciones->addWidget(actualizar);

        tablaPlanillas = new QTableWidget();
        tablaPlanillas->setColumnCount(6);
        tablaPlanillas->setHorizontalHeaderLabels({"ID", "Título", "Materia", "Curso", "Estado", "Archivada"});
        prepararTabla(tablaPlanillas);

        layout->addWidget(crearEncabezado("Planillas de notas", "Planillas, columnas y calificaciones."));
        layout->addWidget(barra);
        layout->addWidget(tablaPlanillas);
        layout->addWidget(crearPaginador(&paginaPlanillas, paginaPlanillasLabel, tablaPlanillas));

        connect(actualizar, &QPushButton::clicked, this, [this]() { cargarPlanillas(); });
        connect(nueva, &QPushButton::clicked, this, [this]() { dialogoNuevaPlanilla(); });
        connect(abrir, &QPushButton::clicked, this, [this]() { abrirPlanillaSeleccionada(); });
        connect(archivar, &QPushButton::clicked, this, [this]() { cambiarArchivado(); });
        connect(tablaPlanillas, &QTableWidget::cellDoubleClicked, this, [this](int, int) { abrirPlanillaSeleccionada(); });
        return pagina;
    }

    // Crea la pantalla interna de una planilla.
    QWidget* crearDetallePlanilla() {
        QWidget* pagina = new QWidget();
        QVBoxLayout* layout = new QVBoxLayout(pagina);
        prepararPagina(layout);

        QFrame* encabezado = new QFrame();
        encabezado->setObjectName("headerCard");
        QVBoxLayout* encabezadoLayout = new QVBoxLayout(encabezado);
        detalleTitulo = new QLabel("Planilla");
        detalleTitulo->setStyleSheet("font-size:27px; font-weight:800; color:#091426;");
        detalleInformacion = new QLabel();
        detalleInformacion->setStyleSheet("color:#45474c; font-size:14px;");
        encabezadoLayout->addWidget(detalleTitulo);
        encabezadoLayout->addWidget(detalleInformacion);

        QPushButton* volver = botonSecundario("Volver");
        detalleEstado = new QComboBox();
        detalleEstado->addItems({"borrador", "activa", "cerrada"});
        detalleEstado->setMinimumWidth(120);
        QPushButton* guardarEstado = new QPushButton("Cambiar estado");
        nuevaColumnaBoton = new QPushButton("Nueva columna");
        detalleColumnas = new QComboBox();
        detalleColumnas->setMinimumWidth(190);
        editarColumnaBoton = botonSecundario("Editar columna");
        borrarColumnaBoton = botonSecundario("Eliminar columna");

        QFrame* barraEstado = crearBarra();
        QHBoxLayout* accionesEstado = (QHBoxLayout*)barraEstado->layout();
        accionesEstado->addWidget(volver);
        accionesEstado->addStretch();
        accionesEstado->addWidget(new QLabel("Estado de la planilla:"));
        accionesEstado->addWidget(detalleEstado);
        accionesEstado->addWidget(guardarEstado);

        QFrame* barraColumnas = crearBarra();
        QHBoxLayout* accionesColumnas = (QHBoxLayout*)barraColumnas->layout();
        QLabel* columnasLabel = new QLabel("Columnas de calificación");
        columnasLabel->setStyleSheet("font-weight:800; color:#091426;");
        accionesColumnas->addWidget(columnasLabel);
        accionesColumnas->addWidget(detalleColumnas);
        accionesColumnas->addWidget(editarColumnaBoton);
        accionesColumnas->addWidget(borrarColumnaBoton);
        accionesColumnas->addStretch();
        accionesColumnas->addWidget(nuevaColumnaBoton);

        tablaDetalle = new QTableWidget();
        prepararTabla(tablaDetalle);
        tablaDetalle->setSelectionMode(QAbstractItemView::NoSelection);
        tablaDetalle->horizontalHeader()->setMinimumHeight(52);

        layout->addWidget(encabezado);
        layout->addWidget(barraEstado);
        layout->addWidget(barraColumnas);
        layout->addWidget(tablaDetalle);

        connect(volver, &QPushButton::clicked, this, [this]() {
            vistasPlanillas->setCurrentIndex(0);
            cargarPlanillas();
        });
        connect(nuevaColumnaBoton, &QPushButton::clicked, this, [this]() { dialogoColumna(false); });
        connect(editarColumnaBoton, &QPushButton::clicked, this, [this]() { dialogoColumna(true); });
        connect(borrarColumnaBoton, &QPushButton::clicked, this, [this]() { eliminarColumnaActual(); });
        connect(guardarEstado, &QPushButton::clicked, this, [this]() { guardarEstadoPlanilla(); });
        return pagina;
    }

    // Muestra un error devuelto por la API.
    void mostrarError(const RespuestaAPI& respuesta, const QString& defecto) {
        QString mensaje = respuesta.mensaje.isEmpty() ? defecto : respuesta.mensaje;
        if (!respuesta.codigo.isEmpty()) mensaje += "\n\nCódigo: " + respuesta.codigo;
        QMessageBox::warning(this, "S.I.G.E.", mensaje);
    }

    // Actualiza el número de la campana.
    void actualizarContadorNotificaciones() {
        RespuestaAPI respuesta = dao.contarNotificaciones();
        if (!respuesta.correcta || contadorNotificaciones == nullptr) return;
        int total = objetoData(respuesta).value("total").toInt();
        contadorNotificaciones->setText(QString::number(total));
        contadorNotificaciones->setVisible(total > 0);
    }

    // Abre el formulario para crear una asistencia.
    void dialogoNuevaAsistencia() {
        if (!asignacionesCargadas) cargarAsignaciones();

        if (asignaciones.isEmpty()) {
            QMessageBox::information(this, "Sin asignaciones", "No hay materias asignadas a cursos.");
            return;
        }

        QDialog dialogo(this);
        dialogo.setWindowTitle("Nueva asistencia");
        dialogo.setMinimumWidth(460);
        QFormLayout* form = new QFormLayout(&dialogo);

        QComboBox* asignacion = new QComboBox();
        for (const QJsonValue& valor : asignaciones) {
            QJsonObject item = valor.toObject();
            asignacion->addItem(nombreMateria(item) + " - " + nombreCurso(item), item.value("id").toInt());
        }

        QLineEdit* fecha = new QLineEdit(QDate::currentDate().toString("yyyy-MM-dd"));
        QLineEdit* inicio = new QLineEdit("08:00:00");
        QLineEdit* fin = new QLineEdit("09:00:00");
        QDialogButtonBox* botones = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

        form->addRow("Materia y curso:", asignacion);
        form->addRow("Fecha:", fecha);
        form->addRow("Inicio:", inicio);
        form->addRow("Fin:", fin);
        form->addRow(botones);

        connect(botones, &QDialogButtonBox::accepted, &dialogo, &QDialog::accept);
        connect(botones, &QDialogButtonBox::rejected, &dialogo, &QDialog::reject);
        if (dialogo.exec() != QDialog::Accepted) return;

        RespuestaAPI respuesta = dao.crearSesionAsistencia(
            asignacion->currentData().toInt(),
            fecha->text(),
            inicio->text(),
            fin->text()
        );

        if (!respuesta.correcta) {
            mostrarError(respuesta, "No se pudo crear la asistencia.");
            return;
        }

        cargarSesionesAsistencia();
    }

    // Carga notificaciones.
    void cargarNotificaciones() {
        RespuestaAPI respuesta = dao.listarNotificaciones();
        if (!respuesta.correcta) {
            mostrarError(respuesta, "No se pudieron cargar las notificaciones.");
            return;
        }

        QJsonArray lista = arregloData(respuesta);
        tablaNotificaciones->setRowCount(lista.size());
        for (int fila = 0; fila < lista.size(); fila++) {
            QJsonObject item = lista[fila].toObject();
            bool leida = item.value("leida").toBool();
            tablaNotificaciones->setItem(fila, 0, celda(QString::number(item.value("id").toInt())));
            tablaNotificaciones->setItem(fila, 1, celda(item.value("titulo").toString()));
            tablaNotificaciones->setItem(fila, 2, celda(item.value("mensaje").toString()));
            tablaNotificaciones->setItem(fila, 3, celda(item.value("origen").toString()));
            tablaNotificaciones->setItem(fila, 4, celda(item.value("fecha").toString()));

            if (!leida) {
                for (int columna = 0; columna < tablaNotificaciones->columnCount(); columna++) {
                    tablaNotificaciones->item(fila, columna)->setBackground(QColor("#d9fbe9"));
                }
            }
        }
        notificacionesCargadas = true;
        actualizarContadorNotificaciones();
        paginaNotificaciones = 0;
        aplicarPaginacion(tablaNotificaciones, paginaNotificaciones, paginaNotificacionesLabel);
    }

    // Marca la notificación seleccionada como leída.
    void marcarNotificacionActual() {
        int fila = tablaNotificaciones->currentRow();
        if (fila < 0 || tablaNotificaciones->item(fila, 0) == nullptr) return;

        RespuestaAPI respuesta = dao.marcarNotificacionLeida(tablaNotificaciones->item(fila, 0)->text().toInt());
        if (!respuesta.correcta) {
            mostrarError(respuesta, "No se pudo actualizar la notificación.");
            return;
        }
        cargarNotificaciones();
    }

    // Marca todas las notificaciones como leídas.
    void marcarTodasNotificaciones() {
        RespuestaAPI respuesta = dao.marcarTodasNotificacionesLeidas();
        if (!respuesta.correcta) {
            mostrarError(respuesta, "No se pudieron actualizar las notificaciones.");
            return;
        }
        cargarNotificaciones();
    }


    // Carga el informe desde los datos directos de MySQL.
    void cargarInformes() {
        RespuestaAPI respuesta = dao.listarEstudiantes();
        if (!respuesta.correcta) {
            mostrarError(respuesta, "No se pudo generar el informe.");
            return;
        }

        QJsonArray estudiantes = arregloData(respuesta);
        tablaInformes->setRowCount(estudiantes.size());
        for (int fila = 0; fila < estudiantes.size(); fila++) {
            QJsonObject estudiante = estudiantes[fila].toObject();
            tablaInformes->setItem(fila, 0, celda(QString::number(estudiante.value("identificador").toInt())));
            tablaInformes->setItem(fila, 1, celda(estudiante.value("nombre_completo").toString()));
            tablaInformes->setItem(fila, 2, celda(estudiante.value("curso_nombre").toString()));
            tablaInformes->setItem(fila, 3, celda(estudiante.value("dni").toString()));
        }
        informesCargados = true;
        paginaInformes = 0;
        aplicarPaginacion(tablaInformes, paginaInformes, paginaInformesLabel);
    }

    // Exporta todas las filas del informe en un CSV compatible con Excel.
    void exportarInforme() {
        if (!informesCargados) cargarInformes();
        QString ruta = QFileDialog::getSaveFileName(this, "Exportar informe", "informe_estudiantes.csv", "Archivos CSV (*.csv)");
        if (ruta.isEmpty()) return;

        QFile archivo(ruta);
        if (!archivo.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            QMessageBox::warning(this, "Informes", "No se pudo crear el archivo.");
            return;
        }

        QByteArray contenido = QByteArray::fromHex("EFBBBF");
        contenido += "ID;Estudiante;Curso;DNI\r\n";
        for (int fila = 0; fila < tablaInformes->rowCount(); fila++) {
            QStringList valores;
            for (int columna = 0; columna < tablaInformes->columnCount(); columna++) {
                QString valor = tablaInformes->item(fila, columna) == nullptr ? "" : tablaInformes->item(fila, columna)->text();
                valor.replace('"', "\"\"");
                valores.append("\"" + valor + "\"");
            }
            contenido += (valores.join(';') + "\r\n").toUtf8();
        }
        archivo.write(contenido);
        archivo.close();
        QMessageBox::information(this, "Informes", "El informe se exportó correctamente.");
    }

    // Carga usuarios de administración.
    void cargarAdministracion() {
        RespuestaAPI respuesta = dao.listarUsuariosAdmin();
        if (!respuesta.correcta) {
            mostrarError(respuesta, "No se pudo cargar administración.");
            return;
        }

        QJsonObject data = objetoData(respuesta);
        if (!data.value("admin").toBool()) {
            tablaAdministracion->setRowCount(1);
            tablaAdministracion->setItem(0, 0, celda("-"));
            tablaAdministracion->setItem(0, 1, celda("Acceso restringido"));
            tablaAdministracion->setItem(0, 2, celda("Solo administradores"));
            tablaAdministracion->setItem(0, 3, celda("-"));
            tablaAdministracion->setItem(0, 4, celda("-"));
            tablaAdministracion->setItem(0, 5, celda("-"));
            administracionCargada = true;
            paginaAdministracion = 0;
            aplicarPaginacion(tablaAdministracion, paginaAdministracion, paginaAdministracionLabel);
            return;
        }

        QJsonArray usuarios = data.value("usuarios").toArray();
        tablaAdministracion->setRowCount(usuarios.size());
        for (int fila = 0; fila < usuarios.size(); fila++) {
            QJsonObject usuario = usuarios[fila].toObject();
            tablaAdministracion->setItem(fila, 0, celda(QString::number(usuario.value("id").toInt())));
            tablaAdministracion->setItem(fila, 1, celda(usuario.value("nombre").toString()));
            tablaAdministracion->setItem(fila, 2, celda(usuario.value("correo").toString()));
            tablaAdministracion->setItem(fila, 3, celda(usuario.value("rol").toString()));
            tablaAdministracion->setItem(fila, 4, celda(usuario.value("activo").toBool() ? "Activo" : "Bloqueado"));
            tablaAdministracion->setItem(fila, 5, celda(usuario.value("fecha").toString()));
        }
        administracionCargada = true;
        paginaAdministracion = 0;
        aplicarPaginacion(tablaAdministracion, paginaAdministracion, paginaAdministracionLabel);
    }

    // Edita el usuario seleccionado desde administración.
    void dialogoEditarUsuario() {
        int fila = tablaAdministracion->currentRow();
        if (fila < 0 || tablaAdministracion->item(fila, 0) == nullptr) return;
        int usuarioId = tablaAdministracion->item(fila, 0)->text().toInt();
        if (usuarioId <= 0) return;

        QString nombreCompleto = tablaAdministracion->item(fila, 1)->text();
        QStringList partes = nombreCompleto.split(' ', Qt::SkipEmptyParts);
        QString nombre = partes.isEmpty() ? "" : partes.first();
        QString apellido = partes.size() > 1 ? partes.mid(1).join(" ") : "";

        QDialog dialogo(this);
        dialogo.setWindowTitle("Editar usuario");
        dialogo.setMinimumWidth(460);
        QFormLayout* form = new QFormLayout(&dialogo);

        QLineEdit* campoNombre = new QLineEdit(nombre);
        QLineEdit* campoApellido = new QLineEdit(apellido);
        QLineEdit* campoCorreo = new QLineEdit(tablaAdministracion->item(fila, 2)->text());
        QComboBox* campoRol = new QComboBox();
        campoRol->addItem("Administrador", 1);
        campoRol->addItem("Docente", 2);
        campoRol->setCurrentIndex(tablaAdministracion->item(fila, 3)->text() == "administrador" ? 0 : 1);
        QComboBox* campoEstado = new QComboBox();
        campoEstado->addItem("Activo", 1);
        campoEstado->addItem("Bloqueado", 0);
        campoEstado->setCurrentIndex(tablaAdministracion->item(fila, 4)->text() == "Activo" ? 0 : 1);

        QDialogButtonBox* botones = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        form->addRow("Nombre:", campoNombre);
        form->addRow("Apellido:", campoApellido);
        form->addRow("Correo:", campoCorreo);
        form->addRow("Rol:", campoRol);
        form->addRow("Estado:", campoEstado);
        form->addRow(botones);

        connect(botones, &QDialogButtonBox::accepted, &dialogo, &QDialog::accept);
        connect(botones, &QDialogButtonBox::rejected, &dialogo, &QDialog::reject);
        if (dialogo.exec() != QDialog::Accepted) return;

        RespuestaAPI respuesta = dao.actualizarUsuarioAdmin(
            usuarioId,
            campoNombre->text(),
            campoApellido->text(),
            campoCorreo->text(),
            campoRol->currentData().toInt(),
            campoEstado->currentData().toInt() == 1
        );

        if (!respuesta.correcta) {
            mostrarError(respuesta, "No se pudo actualizar el usuario.");
            return;
        }
        cargarAdministracion();
    }

    // Carga el panel principal con datos reales.
    void cargarResumenPanel() {
        RespuestaAPI respuesta = dao.resumenPanel();
        if (!respuesta.correcta) {
            mostrarError(respuesta, "No se pudo cargar el panel.");
            return;
        }

        QJsonObject data = objetoData(respuesta);
        int cursos = data.value("cursos").toInt();
        int materias = data.value("materias").toInt();
        int estudiantes = data.value("estudiantes").toInt();
        int planillas = data.value("planillas").toInt();
        double asistencia = data.value("asistencia").toDouble();
        int alertas = data.value("alertas").toInt();

        resumenCursos->setText(QString::number(cursos));
        resumenMaterias->setText(QString::number(materias));
        resumenEstudiantes->setText(QString::number(estudiantes));
        resumenPlanillas->setText(QString::number(planillas));
        resumenPlanillasEstadistica->setText(QString::number(planillas));
        resumenAsistencia->setText(numeroVisible(asistencia) + "%");
        resumenAlertas->setText(QString::number(alertas));

        QJsonArray clases = data.value("clases").toArray();
        if (clases.isEmpty()) {
            tablaClasesPanel->setRowCount(1);
            tablaClasesPanel->setItem(0, 0, new QTableWidgetItem("Sin clases"));
            tablaClasesPanel->setItem(0, 1, new QTableWidgetItem("-"));
        } else {
            tablaClasesPanel->setRowCount(clases.size());
            for (int fila = 0; fila < clases.size(); fila++) {
                QJsonObject clase = clases[fila].toObject();
                tablaClasesPanel->setItem(fila, 0, new QTableWidgetItem(clase.value("materia").toString()));
                tablaClasesPanel->setItem(fila, 1, new QTableWidgetItem(clase.value("curso").toString()));
            }
        }

        QJsonArray listaAlertas = data.value("alertas_lista").toArray();
        if (listaAlertas.isEmpty()) {
            tablaAlertasPanel->setRowCount(1);
            tablaAlertasPanel->setItem(0, 0, new QTableWidgetItem("Sin alertas"));
            tablaAlertasPanel->setItem(0, 1, new QTableWidgetItem("-"));
        } else {
            tablaAlertasPanel->setRowCount(listaAlertas.size());
            for (int fila = 0; fila < listaAlertas.size(); fila++) {
                QJsonObject alerta = listaAlertas[fila].toObject();
                tablaAlertasPanel->setItem(fila, 0, new QTableWidgetItem(alerta.value("nombre").toString()));
                QTableWidgetItem* asistenciaItem = new QTableWidgetItem(numeroVisible(alerta.value("asistencia").toDouble()) + "%");
                asistenciaItem->setTextAlignment(Qt::AlignCenter);
                asistenciaItem->setForeground(QColor("#ba1a1a"));
                tablaAlertasPanel->setItem(fila, 1, asistenciaItem);
            }
        }
    }

    // Carga las sesiones de asistencia.
    void cargarSesionesAsistencia() {
        RespuestaAPI respuesta = dao.listarSesionesAsistencia();
        if (!respuesta.correcta) {
            mostrarError(respuesta, "No se pudieron obtener las asistencias.");
            return;
        }

        QJsonArray sesiones = arregloData(respuesta);
        tablaSesionesAsistencia->setRowCount(sesiones.size());
        for (int fila = 0; fila < sesiones.size(); fila++) {
            QJsonObject sesion = sesiones[fila].toObject();
            QString horario = horaCorta(sesion.value("hora_inicio").toString()) + " - " + horaCorta(sesion.value("hora_fin").toString());

            tablaSesionesAsistencia->setItem(fila, 0, new QTableWidgetItem(QString::number(sesion.value("id").toInt())));
            tablaSesionesAsistencia->setItem(fila, 1, new QTableWidgetItem(sesion.value("fecha").toString()));
            tablaSesionesAsistencia->setItem(fila, 2, new QTableWidgetItem(horario));
            tablaSesionesAsistencia->setItem(fila, 3, new QTableWidgetItem(sesion.value("materia").toString()));
            tablaSesionesAsistencia->setItem(fila, 4, new QTableWidgetItem(sesion.value("curso").toString()));

            QTableWidgetItem* estado = new QTableWidgetItem(sesion.value("estado").toString());
            estado->setTextAlignment(Qt::AlignCenter);
            estado->setForeground(sesion.value("estado").toString() == "cerrada" ? QColor("#ba1a1a") : QColor("#006a61"));
            tablaSesionesAsistencia->setItem(fila, 5, estado);
        }
        asistenciasCargadas = true;
        paginaAsistencias = 0;
        aplicarPaginacion(tablaSesionesAsistencia, paginaAsistencias, paginaAsistenciasLabel);
    }

    // Devuelve el id de la asistencia seleccionada.
    int asistenciaSeleccionada() {
        int fila = tablaSesionesAsistencia->currentRow();
        if (fila < 0 || tablaSesionesAsistencia->item(fila, 0) == nullptr) return 0;
        return tablaSesionesAsistencia->item(fila, 0)->text().toInt();
    }

    // Abre el detalle de una asistencia.
    void abrirAsistenciaSeleccionada() {
        int id = asistenciaSeleccionada();
        if (id <= 0) {
            QMessageBox::information(this, "Seleccione", "Seleccione una asistencia para abrirla.");
            return;
        }

        RespuestaAPI respuesta = dao.obtenerSesionAsistencia(id);
        if (!respuesta.correcta) {
            mostrarError(respuesta, "No se pudo abrir la asistencia.");
            return;
        }

        asistenciaActual = objetoData(respuesta);
        asistenciaActualId = asistenciaActual.value("id").toInt(id);
        completarDetalleAsistencia();
        vistasAsistencias->setCurrentIndex(1);
    }

    // Muestra estudiantes, estados y observaciones de una asistencia.
    void completarDetalleAsistencia() {
        cargandoAsistencia = true;
        asistenciaTitulo->setText(asistenciaActual.value("materia").toString());
        asistenciaInfo->setText(
            asistenciaActual.value("curso").toString() + "  |  "
            + asistenciaActual.value("fecha").toString() + "  |  "
            + horaCorta(asistenciaActual.value("hora_inicio").toString()) + " - "
            + horaCorta(asistenciaActual.value("hora_fin").toString())
        );

        QJsonArray estados = asistenciaActual.value("estados").toArray();
        QJsonArray estudiantes = asistenciaActual.value("estudiantes").toArray();
        bool cerrada = asistenciaActual.value("estado").toString() == "cerrada";

        tablaDetalleAsistencia->setRowCount(estudiantes.size());
        for (int fila = 0; fila < estudiantes.size(); fila++) {
            QJsonObject estudiante = estudiantes[fila].toObject();
            int estudianteId = estudiante.value("id").toInt();

            tablaDetalleAsistencia->setItem(fila, 0, new QTableWidgetItem(QString::number(estudianteId)));
            tablaDetalleAsistencia->setItem(fila, 1, new QTableWidgetItem(estudiante.value("nombre").toString()));

            QComboBox* estado = new QComboBox();
            estado->setProperty("estudianteId", estudianteId);
            estado->setEnabled(!cerrada);
            estado->addItem("Sin cargar", 0);
            for (const QJsonValue& valor : estados) {
                QJsonObject item = valor.toObject();
                estado->addItem(item.value("nombre").toString(), item.value("id").toInt());
            }
            estado->setCurrentIndex(estado->findData(estudiante.value("estado_id").toInt()));
            tablaDetalleAsistencia->setCellWidget(fila, 2, estado);

            QLineEdit* observacion = new QLineEdit(estudiante.value("observacion").toString());
            observacion->setProperty("estudianteId", estudianteId);
            observacion->setReadOnly(cerrada);
            tablaDetalleAsistencia->setCellWidget(fila, 3, observacion);

            connect(estado, &QComboBox::currentIndexChanged, this, [this, estado, observacion]() {
                guardarRegistroAsistencia(estado->property("estudianteId").toInt(), estado->currentData().toInt(), observacion->text());
            });
            connect(observacion, &QLineEdit::editingFinished, this, [this, estado, observacion]() {
                guardarRegistroAsistencia(observacion->property("estudianteId").toInt(), estado->currentData().toInt(), observacion->text());
            });
        }

        tablaDetalleAsistencia->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        tablaDetalleAsistencia->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        tablaDetalleAsistencia->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        tablaDetalleAsistencia->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
        cargandoAsistencia = false;
    }

    // Guarda un registro de asistencia.
    void guardarRegistroAsistencia(int estudianteId, int estadoId, const QString& observacion) {
        if (cargandoAsistencia || estadoId <= 0) return;

        RespuestaAPI respuesta = dao.guardarAsistencia(asistenciaActualId, estudianteId, estadoId, observacion);
        if (!respuesta.correcta) {
            mostrarError(respuesta, "No se pudo guardar la asistencia.");
        }
    }

    // Marca a todos los alumnos como presentes con una sola consulta.
    void marcarTodosPresentes() {
        if (asistenciaActualId <= 0 || asistenciaActual.value("estado").toString() == "cerrada") return;
        int estadoId = 0;
        for (const QJsonValue& valor : asistenciaActual.value("estados").toArray()) {
            QJsonObject estado = valor.toObject();
            if (estado.value("nombre").toString().compare("Presente", Qt::CaseInsensitive) == 0) {
                estadoId = estado.value("id").toInt();
                break;
            }
        }
        if (estadoId <= 0) return;
        RespuestaAPI respuesta = dao.marcarTodosAsistencia(asistenciaActualId, estadoId);
        if (!respuesta.correcta) mostrarError(respuesta, "No se pudo completar la asistencia.");
        else {
            asistenciaActual = objetoData(respuesta);
            completarDetalleAsistencia();
            cargarResumenPanel();
        }
    }

    // Abre el formulario para crear o editar un curso.
    void dialogoCurso(bool editar) {
        int fila = tablaCursos->currentRow();
        if (editar && (fila < 0 || tablaCursos->item(fila, 0) == nullptr)) {
            QMessageBox::information(this, "Cursos", "Seleccione un curso para editar.");
            return;
        }

        int cursoId = editar ? tablaCursos->item(fila, 0)->text().toInt() : 0;
        QDialog dialogo(this);
        dialogo.setWindowTitle(editar ? "Editar curso" : "Nuevo curso");
        dialogo.setMinimumWidth(420);
        QFormLayout* form = new QFormLayout(&dialogo);
        QLineEdit* anio = new QLineEdit(editar ? tablaCursos->item(fila, 1)->text() : "");
        QLineEdit* division = new QLineEdit(editar ? tablaCursos->item(fila, 2)->text() : "");
        QLineEdit* ciclo = new QLineEdit(editar ? tablaCursos->item(fila, 3)->text() : QString::number(QDate::currentDate().year()));
        QDialogButtonBox* botones = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
        form->addRow("Año:", anio);
        form->addRow("División:", division);
        form->addRow("Ciclo lectivo:", ciclo);
        form->addRow(botones);
        connect(botones, &QDialogButtonBox::accepted, &dialogo, &QDialog::accept);
        connect(botones, &QDialogButtonBox::rejected, &dialogo, &QDialog::reject);
        if (dialogo.exec() != QDialog::Accepted) return;

        bool cicloValido = false;
        int cicloLectivo = ciclo->text().toInt(&cicloValido);
        if (!cicloValido) {
            QMessageBox::warning(this, "Curso", "Ingrese un ciclo lectivo válido.");
            return;
        }
        RespuestaAPI respuesta = dao.guardarCurso(cursoId, anio->text(), division->text(), cicloLectivo);
        if (!respuesta.correcta) mostrarError(respuesta, "No se pudo guardar el curso.");
        else {
            cargarCursos();
            estudiantesCargados = false;
            asignacionesCargadas = false;
            cargarResumenPanel();
        }
    }

    // Archiva el curso seleccionado.
    void archivarCursoSeleccionado() {
        int fila = tablaCursos->currentRow();
        if (fila < 0 || tablaCursos->item(fila, 0) == nullptr) return;
        if (QMessageBox::question(this, "Archivar curso", "El curso dejará de aparecer en los listados activos. ¿Continuar?") != QMessageBox::Yes) return;
        RespuestaAPI respuesta = dao.archivarCurso(tablaCursos->item(fila, 0)->text().toInt());
        if (!respuesta.correcta) mostrarError(respuesta, "No se pudo archivar el curso.");
        else {
            cargarCursos();
            estudiantesCargados = false;
            asignacionesCargadas = false;
            cargarResumenPanel();
        }
    }

    // Abre el formulario para crear o editar una materia.
    void dialogoMateria(bool editar) {
        int fila = tablaMaterias->currentRow();
        if (editar && (fila < 0 || tablaMaterias->item(fila, 0) == nullptr)) {
            QMessageBox::information(this, "Materias", "Seleccione una materia para editar.");
            return;
        }
        int materiaId = editar ? tablaMaterias->item(fila, 0)->text().toInt() : 0;
        QDialog dialogo(this);
        dialogo.setWindowTitle(editar ? "Editar materia" : "Nueva materia");
        dialogo.setMinimumWidth(420);
        QFormLayout* form = new QFormLayout(&dialogo);
        QLineEdit* nombre = new QLineEdit(editar ? tablaMaterias->item(fila, 1)->text() : "");
        QDialogButtonBox* botones = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
        form->addRow("Nombre:", nombre);
        form->addRow(botones);
        connect(botones, &QDialogButtonBox::accepted, &dialogo, &QDialog::accept);
        connect(botones, &QDialogButtonBox::rejected, &dialogo, &QDialog::reject);
        if (dialogo.exec() != QDialog::Accepted) return;
        RespuestaAPI respuesta = dao.guardarMateria(materiaId, nombre->text());
        if (!respuesta.correcta) mostrarError(respuesta, "No se pudo guardar la materia.");
        else {
            cargarMaterias();
            asignacionesCargadas = false;
            cargarResumenPanel();
        }
    }

    // Archiva la materia seleccionada.
    void archivarMateriaSeleccionada() {
        int fila = tablaMaterias->currentRow();
        if (fila < 0 || tablaMaterias->item(fila, 0) == nullptr) return;
        if (QMessageBox::question(this, "Archivar materia", "La materia dejará de aparecer en los listados activos. ¿Continuar?") != QMessageBox::Yes) return;
        RespuestaAPI respuesta = dao.archivarMateria(tablaMaterias->item(fila, 0)->text().toInt());
        if (!respuesta.correcta) mostrarError(respuesta, "No se pudo archivar la materia.");
        else {
            cargarMaterias();
            asignacionesCargadas = false;
            cargarResumenPanel();
        }
    }

    // Asigna la materia seleccionada a uno de los cursos activos.
    void dialogoAsignarMateria() {
        int fila = tablaMaterias->currentRow();
        if (fila < 0 || tablaMaterias->item(fila, 0) == nullptr) {
            QMessageBox::information(this, "Materias", "Seleccione una materia para asignar.");
            return;
        }
        if (!cursosCargados) cargarCursos();
        if (nombresCursos.isEmpty()) {
            QMessageBox::information(this, "Cursos", "Primero debe crear un curso.");
            return;
        }

        QDialog dialogo(this);
        dialogo.setWindowTitle("Asignar materia a curso");
        dialogo.setMinimumWidth(460);
        QFormLayout* form = new QFormLayout(&dialogo);
        QLineEdit* materia = new QLineEdit(tablaMaterias->item(fila, 1)->text());
        materia->setReadOnly(true);
        QComboBox* curso = new QComboBox();
        for (auto it = nombresCursos.cbegin(); it != nombresCursos.cend(); ++it) curso->addItem(it.value(), it.key());
        QDialogButtonBox* botones = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
        form->addRow("Materia:", materia);
        form->addRow("Curso:", curso);
        form->addRow(botones);
        connect(botones, &QDialogButtonBox::accepted, &dialogo, &QDialog::accept);
        connect(botones, &QDialogButtonBox::rejected, &dialogo, &QDialog::reject);
        if (dialogo.exec() != QDialog::Accepted) return;

        RespuestaAPI respuesta = dao.asignarMateriaCurso(tablaMaterias->item(fila, 0)->text().toInt(), curso->currentData().toInt());
        if (!respuesta.correcta) mostrarError(respuesta, "No se pudo asignar la materia.");
        else {
            asignacionesCargadas = false;
            asistenciasCargadas = false;
            planillasCargadas = false;
            QMessageBox::information(this, "Materias", "La materia quedó asignada al curso.");
        }
    }


    // Abre el formulario para crear o editar un estudiante.
    void dialogoEstudiante(bool editar) {
        int fila = tablaEstudiantes->currentRow();
        if (editar && (fila < 0 || tablaEstudiantes->item(fila, 0) == nullptr)) {
            QMessageBox::information(this, "Estudiantes", "Seleccione un estudiante para editar.");
            return;
        }
        QTableWidgetItem* idItem = editar ? tablaEstudiantes->item(fila, 0) : nullptr;
        int estudianteId = idItem == nullptr ? 0 : idItem->text().toInt();

        QDialog dialogo(this);
        dialogo.setWindowTitle(editar ? "Editar estudiante" : "Agregar estudiante");
        dialogo.setMinimumWidth(480);
        QFormLayout* form = new QFormLayout(&dialogo);
        QLineEdit* nombre = new QLineEdit(editar ? idItem->data(Qt::UserRole + 1).toString() : "");
        QLineEdit* segundoNombre = new QLineEdit(editar ? idItem->data(Qt::UserRole + 2).toString() : "");
        QLineEdit* apellido = new QLineEdit(editar ? idItem->data(Qt::UserRole + 3).toString() : "");
        QLineEdit* segundoApellido = new QLineEdit(editar ? idItem->data(Qt::UserRole + 4).toString() : "");
        QLineEdit* dni = new QLineEdit(editar ? tablaEstudiantes->item(fila, 2)->text() : "");
        QComboBox* curso = new QComboBox();
        curso->addItem("Sin curso", 0);
        for (auto it = nombresCursos.cbegin(); it != nombresCursos.cend(); ++it) curso->addItem(it.value(), it.key());
        int cursoActual = editar ? idItem->data(Qt::UserRole).toInt() : filtroCurso->currentData().toInt();
        curso->setCurrentIndex(qMax(0, curso->findData(cursoActual)));
        QDialogButtonBox* botones = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
        form->addRow("Nombre:", nombre);
        form->addRow("Segundo nombre:", segundoNombre);
        form->addRow("Apellido:", apellido);
        form->addRow("Segundo apellido:", segundoApellido);
        form->addRow("DNI:", dni);
        form->addRow("Curso:", curso);
        form->addRow(botones);
        connect(botones, &QDialogButtonBox::accepted, &dialogo, &QDialog::accept);
        connect(botones, &QDialogButtonBox::rejected, &dialogo, &QDialog::reject);
        if (dialogo.exec() != QDialog::Accepted) return;

        RespuestaAPI respuesta = dao.guardarEstudiante(estudianteId, nombre->text(), segundoNombre->text(),
            apellido->text(), segundoApellido->text(), dni->text(), curso->currentData().toInt());
        if (!respuesta.correcta) mostrarError(respuesta, "No se pudo guardar el estudiante.");
        else {
            cargarEstudiantes(filtroCurso->currentData().toInt());
            cargarResumenPanel();
        }
    }

    // Elimina el estudiante seleccionado después de confirmar.
    void eliminarEstudianteSeleccionado() {
        int fila = tablaEstudiantes->currentRow();
        if (fila < 0 || tablaEstudiantes->item(fila, 0) == nullptr) return;
        if (QMessageBox::question(this, "Eliminar estudiante", "Se eliminarán también sus registros relacionados. ¿Continuar?") != QMessageBox::Yes) return;
        RespuestaAPI respuesta = dao.eliminarEstudiante(tablaEstudiantes->item(fila, 0)->text().toInt());
        if (!respuesta.correcta) mostrarError(respuesta, "No se pudo eliminar el estudiante.");
        else {
            cargarEstudiantes(filtroCurso->currentData().toInt());
            cargarResumenPanel();
        }
    }

    // Carga los cursos recibidos desde S.I.G.E.
    void cargarCursos() {
        RespuestaAPI respuesta = dao.listarCursos();
        if (!respuesta.correcta) {
            mostrarError(respuesta, "No se pudieron obtener los cursos.");
            return;
        }

        QJsonArray cursos = arregloData(respuesta);
        tablaCursos->setRowCount(cursos.size());
        filtroCurso->clear();
        filtroCurso->addItem("Todos los cursos", 0);
        nombresCursos.clear();

        for (int fila = 0; fila < cursos.size(); fila++) {
            QJsonObject curso = cursos[fila].toObject();
            int id = curso.value("id").toInt();
            QString nombre = curso.value("nombre").toString();
            if (nombre.isEmpty()) {
                nombre = curso.value("anio").toString() + " " + curso.value("division").toString()
                    + " " + QString::number(curso.value("ciclo_lectivo").toInt());
            }

            tablaCursos->setItem(fila, 0, new QTableWidgetItem(QString::number(id)));
            tablaCursos->setItem(fila, 1, new QTableWidgetItem(curso.value("anio").toString()));
            tablaCursos->setItem(fila, 2, new QTableWidgetItem(curso.value("division").toString()));
            tablaCursos->setItem(fila, 3, new QTableWidgetItem(QString::number(curso.value("ciclo_lectivo").toInt())));
            filtroCurso->addItem(nombre, id);
            nombresCursos[id] = nombre;
        }

        resumenCursos->setText(QString::number(cursos.size()));
        cursosCargados = true;
        paginaCursos = 0;
        aplicarPaginacion(tablaCursos, paginaCursos, paginaCursosLabel);
    }

    // Carga las materias recibidas desde S.I.G.E.
    void cargarMaterias() {
        RespuestaAPI respuesta = dao.listarMaterias();
        if (!respuesta.correcta) {
            mostrarError(respuesta, "No se pudieron obtener las materias.");
            return;
        }

        QJsonArray materias = arregloData(respuesta);
        tablaMaterias->setRowCount(materias.size());
        for (int fila = 0; fila < materias.size(); fila++) {
            QJsonObject materia = materias[fila].toObject();
            tablaMaterias->setItem(fila, 0, new QTableWidgetItem(QString::number(materia.value("id").toInt())));
            tablaMaterias->setItem(fila, 1, new QTableWidgetItem(materia.value("nombre").toString()));
        }
        resumenMaterias->setText(QString::number(materias.size()));
        materiasCargadas = true;
        paginaMaterias = 0;
        aplicarPaginacion(tablaMaterias, paginaMaterias, paginaMateriasLabel);
    }

    // Guarda las asignaciones para crear planillas.
    void cargarAsignaciones() {
        RespuestaAPI respuesta = dao.listarAsignaciones();
        if (!respuesta.correcta) {
            mostrarError(respuesta, "No se pudieron obtener las asignaciones.");
            return;
        }
        asignaciones = arregloData(respuesta);
        asignacionesCargadas = true;
    }

    // Carga estudiantes, opcionalmente filtrados por curso.
    void cargarEstudiantes(int cursoId = 0) {
        RespuestaAPI respuesta = dao.listarEstudiantes(cursoId);
        if (!respuesta.correcta) {
            mostrarError(respuesta, "No se pudieron obtener los estudiantes.");
            return;
        }

        QJsonArray estudiantes = arregloData(respuesta);
        tablaEstudiantes->setRowCount(estudiantes.size());
        for (int fila = 0; fila < estudiantes.size(); fila++) {
            QJsonObject estudiante = estudiantes[fila].toObject();
            QString nombre = estudiante.value("nombre_listado").toString();
            if (nombre.isEmpty()) nombre = estudiante.value("nombre_completo").toString();
            QString curso = estudiante.value("curso_nombre").toString();
            if (curso.isEmpty()) curso = nombreCurso(estudiante);
            if (curso.isEmpty()) curso = nombresCursos.value(estudiante.value("curso_id").toInt());

            QTableWidgetItem* idItem = new QTableWidgetItem(QString::number(estudiante.value("id").toInt()));
            idItem->setData(Qt::UserRole, estudiante.value("curso_id").toInt());
            idItem->setData(Qt::UserRole + 1, estudiante.value("nombre").toString());
            idItem->setData(Qt::UserRole + 2, estudiante.value("segundo_nombre").toString());
            idItem->setData(Qt::UserRole + 3, estudiante.value("apellido").toString());
            idItem->setData(Qt::UserRole + 4, estudiante.value("segundo_apellido").toString());
            tablaEstudiantes->setItem(fila, 0, idItem);
            tablaEstudiantes->setItem(fila, 1, new QTableWidgetItem(nombre));
            tablaEstudiantes->setItem(fila, 2, new QTableWidgetItem(estudiante.value("dni").toVariant().toString()));
            tablaEstudiantes->setItem(fila, 3, new QTableWidgetItem(curso));
        }

        if (cursoId == 0) {
            resumenEstudiantes->setText(QString::number(estudiantes.size()));
            estudiantesCargados = true;
        }
        paginaEstudiantes = 0;
        aplicarPaginacion(tablaEstudiantes, paginaEstudiantes, paginaEstudiantesLabel);
    }

    // Carga la lista de planillas activas y archivadas.
    void cargarPlanillas() {
        RespuestaAPI respuesta = dao.listarPlanillas();
        if (!respuesta.correcta) {
            mostrarError(respuesta, "No se pudieron obtener las planillas.");
            return;
        }

        QJsonArray planillas = arregloData(respuesta);
        tablaPlanillas->setRowCount(planillas.size());
        for (int fila = 0; fila < planillas.size(); fila++) {
            QJsonObject planilla = planillas[fila].toObject();
            QString estado = planilla.value("estado").toString();
            bool archivada = planilla.value("archivada").toBool();
            tablaPlanillas->setItem(fila, 0, new QTableWidgetItem(QString::number(planilla.value("id").toInt())));
            tablaPlanillas->setItem(fila, 1, new QTableWidgetItem(planilla.value("titulo").toString()));
            tablaPlanillas->setItem(fila, 2, new QTableWidgetItem(nombreMateria(planilla)));
            tablaPlanillas->setItem(fila, 3, new QTableWidgetItem(nombreCurso(planilla)));
            QTableWidgetItem* estadoItem = new QTableWidgetItem(estado.left(1).toUpper() + estado.mid(1));
            estadoItem->setTextAlignment(Qt::AlignCenter);
            if (estado == "activa") {
                estadoItem->setBackground(QColor("#d9fbe9"));
                estadoItem->setForeground(QColor("#12683c"));
            } else if (estado == "cerrada") {
                estadoItem->setBackground(QColor("#e2e8f0"));
                estadoItem->setForeground(QColor("#45474c"));
            } else {
                estadoItem->setBackground(QColor("#fff1d6"));
                estadoItem->setForeground(QColor("#8a5700"));
            }
            tablaPlanillas->setItem(fila, 4, estadoItem);

            QTableWidgetItem* archivadaItem = new QTableWidgetItem(archivada ? "Archivada" : "Disponible");
            archivadaItem->setTextAlignment(Qt::AlignCenter);
            archivadaItem->setForeground(QColor(archivada ? "#ba1a1a" : "#006a61"));
            tablaPlanillas->setItem(fila, 5, archivadaItem);
        }
        resumenPlanillas->setText(QString::number(planillas.size()));
        planillasCargadas = true;
        paginaPlanillas = 0;
        aplicarPaginacion(tablaPlanillas, paginaPlanillas, paginaPlanillasLabel);
    }

    // Abre un dialogo para crear una planilla.
    void dialogoNuevaPlanilla() {
        if (!asignacionesCargadas) cargarAsignaciones();

        if (asignaciones.isEmpty()) {
            QMessageBox::information(this, "Sin asignaciones", "No hay asignaciones materia-curso disponibles.");
            return;
        }

        QDialog dialogo(this);
        dialogo.setWindowTitle("Nueva planilla");
        dialogo.setMinimumWidth(460);
        QFormLayout* form = new QFormLayout(&dialogo);
        QLineEdit* titulo = new QLineEdit();
        titulo->setPlaceholderText("Ejemplo: Trabajos prácticos");
        nuevaAsignacion = new QComboBox();

        for (const QJsonValue& valor : asignaciones) {
            QJsonObject asignacion = valor.toObject();
            nuevaAsignacion->addItem(nombreMateria(asignacion) + " - " + nombreCurso(asignacion), asignacion.value("id").toInt());
        }

        QDialogButtonBox* botones = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        form->addRow("Título:", titulo);
        form->addRow("Materia y curso:", nuevaAsignacion);
        form->addRow(botones);
        connect(botones, &QDialogButtonBox::accepted, &dialogo, &QDialog::accept);
        connect(botones, &QDialogButtonBox::rejected, &dialogo, &QDialog::reject);

        if (dialogo.exec() != QDialog::Accepted) return;
        if (titulo->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, "Falta el título", "Ingresa un título para la planilla.");
            return;
        }

        RespuestaAPI respuesta = dao.crearPlanilla(titulo->text().trimmed(), nuevaAsignacion->currentData().toInt());
        if (!respuesta.correcta) {
            mostrarError(respuesta, "No se pudo crear la planilla.");
            return;
        }
        cargarPlanillas();
    }

    // Devuelve el ID de la planilla seleccionada.
    int planillaSeleccionada() {
        int fila = tablaPlanillas->currentRow();
        if (fila < 0 || tablaPlanillas->item(fila, 0) == nullptr) return 0;
        return tablaPlanillas->item(fila, 0)->text().toInt();
    }

    // Abre la planilla seleccionada en la tabla.
    void abrirPlanillaSeleccionada() {
        int id = planillaSeleccionada();
        if (id <= 0) {
            QMessageBox::information(this, "Seleccione", "Seleccione una planilla para abrirla.");
            return;
        }
        cargarDetallePlanilla(id);
    }

    // Obtiene y muestra una planilla completa.
    void cargarDetallePlanilla(int id) {
        RespuestaAPI respuesta = dao.obtenerPlanilla(id);
        if (!respuesta.correcta) {
            mostrarError(respuesta, "No se pudo abrir la planilla.");
            return;
        }

        planillaActual = objetoData(respuesta);
        planillaActualId = planillaActual.value("id").toInt(id);
        completarDetallePlanilla();
        vistasPlanillas->setCurrentIndex(1);
    }

    // Completa titulos, columnas, alumnos y notas.
    void completarDetallePlanilla() {
        cargandoPlanilla = true;
        detalleTitulo->setText(planillaActual.value("titulo").toString());
        detalleInformacion->setText(nombreMateria(planillaActual) + "  |  " + nombreCurso(planillaActual));

        QString estado = planillaActual.value("estado").toString();
        detalleEstado->setCurrentText(estado);
        bool cerrada = estado == "cerrada";
        if (estado == "activa") {
            detalleEstado->setStyleSheet("background:#d9fbe9; color:#12683c; border:1px solid #b9ece5; font-weight:700;");
        } else if (estado == "cerrada") {
            detalleEstado->setStyleSheet("background:#e2e8f0; color:#45474c; border:1px solid #c5c6cd; font-weight:700;");
        } else {
            detalleEstado->setStyleSheet("background:#fff1d6; color:#8a5700; border:1px solid #ffddb8; font-weight:700;");
        }
        nuevaColumnaBoton->setEnabled(!cerrada);

        QJsonArray columnas = planillaActual.value("columnas").toArray();
        QJsonArray alumnos = planillaActual.value("alumnos").toArray();
        editarColumnaBoton->setEnabled(!cerrada && !columnas.isEmpty());
        borrarColumnaBoton->setEnabled(!cerrada && !columnas.isEmpty());

        detalleColumnas->clear();
        for (const QJsonValue& valor : columnas) {
            QJsonObject columna = valor.toObject();
            detalleColumnas->addItem(columna.value("titulo").toString(), columna.value("id").toInt());
        }

        int cantidadColumnas = 1 + columnas.size() + 1;
        tablaDetalle->clear();
        tablaDetalle->setColumnCount(cantidadColumnas);
        tablaDetalle->setRowCount(alumnos.size());

        QStringList encabezados = {"Estudiante"};
        for (const QJsonValue& valor : columnas) {
            QJsonObject columna = valor.toObject();
            encabezados << columna.value("titulo").toString()
                + "\nAprob. " + numeroVisible(columna.value("calificacion_aprobacion").toDouble(7))
                + " / Máx. " + numeroVisible(columna.value("puntaje_maximo").toDouble(10));
        }
        encabezados << "Promedio";
        tablaDetalle->setHorizontalHeaderLabels(encabezados);

        double promedioMinimo = planillaActual.value("promedio_minimo").toDouble(7);

        for (int fila = 0; fila < alumnos.size(); fila++) {
            QJsonObject alumno = alumnos[fila].toObject();
            QString nombre = alumno.value("nombre_completo").toString();
            if (nombre.isEmpty()) nombre = alumno.value("nombre").toString();
            int identificador = alumno.value("identificador").toInt(alumno.value("legajo").toInt());

            QTableWidgetItem* estudianteItem = new QTableWidgetItem(nombre + "\nID " + QString::number(identificador));
            estudianteItem->setForeground(QColor("#0b1c30"));
            tablaDetalle->setItem(fila, 0, estudianteItem);
            tablaDetalle->setRowHeight(fila, 52);

            QJsonObject calificaciones = alumno.value("calificaciones").toObject();
            for (int indice = 0; indice < columnas.size(); indice++) {
                QJsonObject columna = columnas[indice].toObject();
                int columnaId = columna.value("id").toInt();
                QJsonValue valorCelda = calificaciones.value(QString::number(columnaId));
                QJsonValue notaJson;
                QString actualizado;

                if (valorCelda.isObject()) {
                    notaJson = valorCelda.toObject().value("nota");
                    actualizado = valorCelda.toObject().value("updated_at").toString();
                } else {
                    notaJson = valorCelda;
                }

                QLineEdit* nota = new QLineEdit();
                nota->setAlignment(Qt::AlignCenter);
                nota->setReadOnly(cerrada);
                nota->setValidator(new QDoubleValidator(0, columna.value("puntaje_maximo").toDouble(10), 2, nota));
                nota->setProperty("alumnoId", alumno.value("id").toInt());
                nota->setProperty("columnaId", columnaId);
                nota->setProperty("fila", fila);
                nota->setProperty("actualizado", actualizado);
                nota->setProperty("aprobacion", columna.value("calificacion_aprobacion").toDouble(7));

                if (!notaJson.isNull() && !notaJson.isUndefined()) nota->setText(numeroVisible(notaJson.toDouble()));
                nota->setProperty("original", nota->text());
                pintarNota(nota);

                connect(nota, &QLineEdit::editingFinished, this, [this, nota]() { guardarNota(nota, false); });
                tablaDetalle->setCellWidget(fila, indice + 1, nota);
            }

            QTableWidgetItem* promedio = new QTableWidgetItem("-");
            promedio->setTextAlignment(Qt::AlignCenter);
            QJsonValue promedioJson = alumno.value("promedio_final");
            if (!promedioJson.isNull() && !promedioJson.isUndefined()) {
                double valor = promedioJson.toDouble();
                promedio->setText(numeroVisible(valor));
                promedio->setForeground(valor >= promedioMinimo ? QColor("#12683c") : QColor("#ba1a1a"));
            }
            tablaDetalle->setItem(fila, cantidadColumnas - 1, promedio);
        }

        tablaDetalle->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        for (int columna = 1; columna < cantidadColumnas; columna++) {
            tablaDetalle->horizontalHeader()->setSectionResizeMode(columna, QHeaderView::ResizeToContents);
        }
        cargandoPlanilla = false;
    }

    // Colorea una nota según la aprobación configurada.
    void pintarNota(QLineEdit* campo) {
        if (campo->text().trimmed().isEmpty()) {
            campo->setStyleSheet("");
            return;
        }
        double valor = campo->text().replace(',', '.').toDouble();
        double aprobacion = campo->property("aprobacion").toDouble();
        if (valor >= aprobacion) campo->setStyleSheet("color:#12683c; background:#d9fbe9; border:1px solid #b9ece5; font-weight:700;");
        else campo->setStyleSheet("color:#ba1a1a; background:#ffdad6; border:1px solid #ffc8c0; font-weight:700;");
    }

    // Guarda una nota cuando el docente termina de editarla.
    void guardarNota(QLineEdit* campo, bool reintento) {
        if (cargandoPlanilla || campo->isReadOnly()) return;
        QString texto = campo->text().trimmed().replace(',', '.');
        if (!reintento && texto == campo->property("original").toString()) return;

        bool numeroCorrecto = false;
        if (!texto.isEmpty()) texto.toDouble(&numeroCorrecto);
        if (!texto.isEmpty() && !numeroCorrecto) {
            campo->setText(campo->property("original").toString());
            QMessageBox::warning(this, "Nota incorrecta", "Ingresa un número válido o deja el campo vacío.");
            return;
        }

        QJsonValue nota = texto.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(texto.toDouble());
        RespuestaAPI respuesta = dao.guardarNota(
            planillaActualId,
            campo->property("alumnoId").toInt(),
            campo->property("columnaId").toInt(),
            nota,
            campo->property("actualizado").toString()
        );

        if (respuesta.correcta) {
            QJsonObject data = objetoData(respuesta);
            QJsonValue normalizada = data.value("nota");
            QString visible = normalizada.isNull() ? "" : numeroVisible(normalizada.toDouble());
            campo->setText(visible);
            campo->setProperty("original", visible);
            campo->setProperty("actualizado", data.value("updated_at").toString());
            pintarNota(campo);

            int fila = campo->property("fila").toInt();
            if (fila >= 0) {
                QTableWidgetItem* promedio = tablaDetalle->item(fila, tablaDetalle->columnCount() - 1);
                QJsonValue promedioJson = data.value("promedio_final");
                if (promedioJson.isNull() || promedioJson.isUndefined()) promedio->setText("-");
                else {
                    double promedioValor = promedioJson.toDouble();
                    promedio->setText(numeroVisible(promedioValor));
                    promedio->setForeground(promedioValor >= planillaActual.value("promedio_minimo").toDouble(7)
                        ? QColor("#12683c") : QColor("#ba1a1a"));
                }
            }
            return;
        }

        if (respuesta.estadoHttp == 409 && !reintento) {
            QJsonObject error = respuesta.json.object().value("error").toObject();
            QJsonValue servidor = error.value("server_value");
            QString fechaServidor = error.value("server_updated_at").toString();
            QMessageBox pregunta(QMessageBox::Warning, "Conflicto de nota",
                "La nota fue modificada en otro dispositivo.\n\nSi elige Reintentar se enviara su valor. Si elige Servidor se conservara el valor de S.I.G.E.",
                QMessageBox::NoButton, this);
            QPushButton* reintentar = pregunta.addButton("Reintentar", QMessageBox::AcceptRole);
            QPushButton* usarServidor = pregunta.addButton("Usar servidor", QMessageBox::RejectRole);
            pregunta.exec();

            if (pregunta.clickedButton() == reintentar) {
                campo->setProperty("actualizado", fechaServidor);
                guardarNota(campo, true);
            } else if (pregunta.clickedButton() == usarServidor) {
                QString visible = servidor.isNull() ? "" : numeroVisible(servidor.toDouble());
                campo->setText(visible);
                campo->setProperty("original", visible);
                campo->setProperty("actualizado", fechaServidor);
                pintarNota(campo);
            }
            return;
        }

        campo->setText(campo->property("original").toString());
        pintarNota(campo);
        mostrarError(respuesta, "No se pudo guardar la nota.");
        if (respuesta.estadoHttp == 423) cargarDetallePlanilla(planillaActualId);
    }

    // Crea o edita una columna de la planilla.
    void dialogoColumna(bool editar) {
        if (planillaActual.value("estado").toString() == "cerrada") {
            QMessageBox::information(this, "Planilla cerrada", "La planilla es de solo lectura.");
            return;
        }

        int columnaId = editar ? detalleColumnas->currentData().toInt() : 0;
        QJsonObject columnaActual;
        for (const QJsonValue& valor : planillaActual.value("columnas").toArray()) {
            if (valor.toObject().value("id").toInt() == columnaId) columnaActual = valor.toObject();
        }
        if (editar && columnaActual.isEmpty()) {
            QMessageBox::information(this, "Sin columna", "Seleccione una columna para editar.");
            return;
        }

        QDialog dialogo(this);
        dialogo.setWindowTitle(editar ? "Editar columna" : "Nueva columna");
        QFormLayout* form = new QFormLayout(&dialogo);
        QLineEdit* titulo = new QLineEdit(columnaActual.value("titulo").toString());
        QDoubleSpinBox* aprobacion = new QDoubleSpinBox();
        QDoubleSpinBox* maximo = new QDoubleSpinBox();
        aprobacion->setRange(7, 1000);
        maximo->setRange(0.01, 1000);
        aprobacion->setValue(columnaActual.value("calificacion_aprobacion").toDouble(7));
        maximo->setValue(columnaActual.value("puntaje_maximo").toDouble(10));
        QDialogButtonBox* botones = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

        form->addRow("Título:", titulo);
        form->addRow("Aprobación:", aprobacion);
        form->addRow("Puntaje máximo:", maximo);
        form->addRow(botones);
        connect(botones, &QDialogButtonBox::accepted, &dialogo, &QDialog::accept);
        connect(botones, &QDialogButtonBox::rejected, &dialogo, &QDialog::reject);
        if (dialogo.exec() != QDialog::Accepted) return;

        RespuestaAPI respuesta = editar
            ? dao.editarColumna(planillaActualId, columnaId, titulo->text(), aprobacion->value(), maximo->value())
            : dao.crearColumna(planillaActualId, titulo->text(), aprobacion->value(), maximo->value());

        if (!respuesta.correcta) {
            mostrarError(respuesta, "No se pudo guardar la columna.");
            return;
        }
        planillaActual = objetoData(respuesta);
        completarDetallePlanilla();
    }

    // Elimina la columna seleccionada y sus notas.
    void eliminarColumnaActual() {
        int columnaId = detalleColumnas->currentData().toInt();
        if (columnaId <= 0) {
            QMessageBox::information(this, "Sin columna", "Seleccione una columna para eliminar.");
            return;
        }
        if (QMessageBox::question(this, "Eliminar columna", "También se eliminarán sus calificaciones. ¿Continuar?") != QMessageBox::Yes) return;

        RespuestaAPI respuesta = dao.eliminarColumna(planillaActualId, columnaId);
        if (!respuesta.correcta) {
            mostrarError(respuesta, "No se pudo eliminar la columna.");
            return;
        }
        planillaActual = objetoData(respuesta);
        completarDetallePlanilla();
    }

    // Cambia el estado de la planilla actual.
    void guardarEstadoPlanilla() {
        RespuestaAPI respuesta = dao.cambiarEstado(planillaActualId, detalleEstado->currentText());
        if (!respuesta.correcta) {
            mostrarError(respuesta, "No se pudo cambiar el estado.");
            return;
        }
        planillaActual = objetoData(respuesta);
        completarDetallePlanilla();
    }

    // Archiva o desarchiva la planilla seleccionada.
    void cambiarArchivado() {
        int fila = tablaPlanillas->currentRow();
        int id = planillaSeleccionada();
        if (id <= 0 || fila < 0) {
            QMessageBox::information(this, "Seleccione", "Seleccione una planilla.");
            return;
        }

        bool archivada = tablaPlanillas->item(fila, 5)->text() == "Archivada";
        QString accion = archivada ? "desarchivar" : "archivar";
        if (QMessageBox::question(this, "Confirmar", "¿Desea " + accion + " esta planilla?") != QMessageBox::Yes) return;

        RespuestaAPI respuesta = archivada ? dao.desarchivarPlanilla(id) : dao.archivarPlanilla(id);
        if (!respuesta.correcta) {
            mostrarError(respuesta, "No se pudo cambiar el archivado.");
            return;
        }
        cargarPlanillas();
    }
};

// Punto de inicio de la aplicación gráfica.
int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    AplicacionGUI ventana;
    ventana.show();
    return app.exec();
}
