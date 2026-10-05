# Presente

Aplicación de escritorio en C++ y Qt conectada con el sistema web S.I.G.E.

La versión gráfica inicia sesión mediante la API HTTPS de S.I.G.E. y luego
consulta la información académica directamente desde MySQL para cargar más
rápido.

## Arquitectura

```text
Interfaz Qt
    |
    v
dao/SigeDAO
    |
    v
datos/ConexionBD
    |
    v
MySQL de S.I.G.E.
```

El login usa `api/ApiCliente` para validar el correo y la contraseña contra
S.I.G.E.

## Archivos principales

- `gui.cpp`: interfaz gráfica y acciones del usuario.
- `api/ApiCliente.h/.cpp`: login HTTPS y respuestas JSON.
- `dao/SigeDAO.h/.cpp`: consultas y operaciones contra S.I.G.E.
- `datos/ConexionBD.h/.cpp`: conexión directa a MySQL.
- `datos/ConfigBD.h`: configuración local privada, ignorada por Git.
- `datos/ConfigAPI.h`: URL pública de S.I.G.E.
- `modelo/Docente.h`: datos básicos del docente autenticado.
- `PresenteGUI.pro`: configuración del proyecto Qt.
- `datos/ConfigBD.example.h`: ejemplo sin credenciales privadas.

## Iniciar sesión

Use cualquier cuenta docente activa de S.I.G.E. La aplicación envía el correo
y la contraseña mediante HTTPS y recibe un token válido durante 12 horas.

- La contraseña no se guarda en el equipo.
- El token permanece solamente en memoria.
- Al cerrar la ventana se revoca el token.
- Cada docente recibe solamente sus propios datos.

## Módulos disponibles

- Panel de resumen.
- Cursos de S.I.G.E.
- Materias de S.I.G.E.
- Alta, edición y archivado de cursos y materias.
- Asignación de materias a cursos.
- Alta, edición y eliminación de estudiantes, con filtro por curso.
- Asistencias por fecha, horario, materia y curso.
- Marcado individual o de todos los estudiantes como presentes.
- Notificaciones.
- Administración de usuarios para cuentas administradoras.
- Informes de estudiantes con exportación CSV compatible con Excel.
- Listado de planillas activas y archivadas.
- Creación y apertura de planillas.
- Creación, modificación y eliminación de columnas.
- Carga automática de calificaciones.
- Cálculo de promedios.
- Estados borrador, activa y cerrada.
- Archivado y desarchivado.
- Detección de conflictos de edición.

La carga completa de una planilla obtiene todas las calificaciones en una sola
consulta. Esto evita una consulta adicional por estudiante y reduce la demora
al abrir cursos numerosos.

Las notas se guardan al salir de la celda. Una celda vacía se envía como
`null`. Las planillas cerradas se muestran en modo de solo lectura.

## Compilar

Abrir PowerShell en la carpeta del proyecto y ejecutar:

```powershell
$env:PATH = "C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;$env:PATH"
qmake PresenteGUI.pro
mingw32-make -f Makefile.Release
windeployqt release\PresenteGUI.exe
```

El proyecto usa los módulos Qt Widgets, Qt Network y MySQL Connector C.
Después de compilar, copie `libmysql.dll` en la carpeta `release`.

## Versión entregable

Los ejecutables y las librerías generadas no se guardan en el repositorio. El
equipo debe usar Windows de 64 bits y tener acceso a Internet. Windows puede
mostrar una advertencia porque es un proyecto escolar sin firma digital.

## Ejecutar

```powershell
cd release
.\PresenteGUI.exe
```

Es necesario tener acceso a Internet y poder abrir:

```text
https://sige.torga.com.ar/api/v1
```

La URL se configura en `datos/ConfigAPI.h`. Para compilar localmente, copie
`datos/ConfigBD.example.h` como `datos/ConfigBD.h` y complete los datos de
MySQL. `ConfigBD.h` está ignorado por Git y nunca debe subirse ni compartirse.

Los ejecutables compilados con una configuración de MySQL contienen esos datos
de conexión. No deben publicarse ni distribuirse mientras la aplicación use
una conexión directa a la base de datos.

Los errores de validación, permisos, planillas cerradas y conflictos son
devueltos por S.I.G.E. en formato JSON y mostrados por Presente.
