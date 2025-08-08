#pragma once

#include <QWidget>
#include <QPointer>
#include <QVector3D>

// Qt3D
#include <Qt3DCore/QEntity>
#include <Qt3DCore/QTransform>
#include <Qt3DRender/QCamera>
#include <Qt3DRender/QMaterial>
#include <Qt3DRender/QPickEvent>
#include <Qt3DRender/QGeometryRenderer>
#include <Qt3DRender/QGeometry>
#include <Qt3DRender/QAttribute>
#include <Qt3DRender/QBuffer>
#include <Qt3DExtras/Qt3DWindow>
#include <Qt3DExtras/QOrbitCameraController>
#include <Qt3DExtras/QPhongMaterial>
#include <QQuaternion>
#include <QVBoxLayout>
#include <QRegularExpression>
#include <limits>

QT_BEGIN_NAMESPACE
namespace Ui { class ProcessModelWithQt3D; }
QT_END_NAMESPACE

class ProcessModelWithQt3D : public QWidget
{
    Q_OBJECT

public:
    explicit ProcessModelWithQt3D(QWidget *parent = nullptr);
    ~ProcessModelWithQt3D() override;

private slots:
    void on_load3DModelBtn_clicked();
    void on_directionComboBox_currentIndexChanged(int index);
    void on_createGrayImage_clicked();
    void on_saveGrayImage_clicked();

private:
    void setup3DScene();
    bool loadStlIntoEntity(const QString &filePath);

    // STL helpers (no external libs)
    struct Triangle { QVector3D normal; QVector3D v0; QVector3D v1; QVector3D v2; };
    bool parseStlFile(const QString &filePath, QList<Triangle> &outTriangles, QVector3D &outMin, QVector3D &outMax);
    bool parseBinaryStl(const QByteArray &data, QList<Triangle> &outTriangles, QVector3D &outMin, QVector3D &outMax);
    bool parseAsciiStl(const QByteArray &data, QList<Triangle> &outTriangles, QVector3D &outMin, QVector3D &outMax);
    void buildGeometryFromTriangles(const QList<Triangle> &triangles);
    void frameCameraToAabb(const QVector3D &minP, const QVector3D &maxP);

private:
    Ui::ProcessModelWithQt3D *ui;

    // Qt3D members
    Qt3DExtras::Qt3DWindow *qt3dWindow {nullptr};
    QWidget *qt3dContainerWidget {nullptr};

    Qt3DCore::QEntity *rootEntity {nullptr};
    Qt3DRender::QCamera *camera {nullptr};
    Qt3DExtras::QOrbitCameraController *cameraController {nullptr};

    Qt3DCore::QEntity *modelEntity {nullptr};
    Qt3DCore::QTransform *modelTransform {nullptr};
    Qt3DRender::QGeometryRenderer *modelMeshRenderer {nullptr};
};