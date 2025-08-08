#include "ProcessModelWithQt3D.h"
#include "ui_ProcessModelWithQt3D.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QtMath>
#include <QRegularExpression>
#include <QQuaternion>
#include <Qt3DRender/QDirectionalLight>
#include <limits>

ProcessModelWithQt3D::ProcessModelWithQt3D(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ProcessModelWithQt3D)
{
    ui->setupUi(this);
    setup3DScene();
}

ProcessModelWithQt3D::~ProcessModelWithQt3D()
{
    delete ui;
}

void ProcessModelWithQt3D::setup3DScene()
{
    // Create Qt3D window
    qt3dWindow = new Qt3DExtras::Qt3DWindow();
    qt3dWindow->defaultFrameGraph()->setClearColor(QColor(30, 30, 35));

    // Container widget to embed into QWidget UI
    qt3dContainerWidget = QWidget::createWindowContainer(qt3dWindow, this);
    qt3dContainerWidget->setMinimumSize(200, 200);
    qt3dContainerWidget->setFocusPolicy(Qt::StrongFocus);

    // Put container into the UI placeholder `model3DViewContainer`
    if (auto layout = ui->layoutContainer) {
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(qt3dContainerWidget);
    }

    // Root entity
    rootEntity = new Qt3DCore::QEntity();

    // Camera
    camera = qt3dWindow->camera();
    camera->lens()->setPerspectiveProjection(45.0f, 16.0f/9.0f, 0.1f, 1000.0f);
    camera->setPosition(QVector3D(0.0f, 0.0f, 10.0f));
    camera->setViewCenter(QVector3D(0.0f, 0.0f, 0.0f));

    // Camera controls
    cameraController = new Qt3DExtras::QOrbitCameraController(rootEntity);
    cameraController->setCamera(camera);
    cameraController->setLinearSpeed(50.0f);
    cameraController->setLookSpeed(180.0f);

    qt3dWindow->setRootEntity(rootEntity);
}

bool ProcessModelWithQt3D::loadStlIntoEntity(const QString &filePath)
{
    QList<Triangle> triangles;
    QVector3D minP, maxP;
    if (!parseStlFile(filePath, triangles, minP, maxP)) {
        QMessageBox::warning(this, "加载失败", "无法解析 STL 文件");
        return false;
    }

    // Remove previous entity
    if (modelEntity) {
        modelEntity->setParent(nullptr);
        delete modelEntity;
        modelEntity = nullptr;
    }

    modelEntity = new Qt3DCore::QEntity(rootEntity);
    modelTransform = new Qt3DCore::QTransform(modelEntity);
    modelTransform->setScale(1.0f);

    modelMeshRenderer = new Qt3DRender::QGeometryRenderer(modelEntity);
    buildGeometryFromTriangles(triangles);

    auto material = new Qt3DExtras::QPhongMaterial(modelEntity);
    material->setDiffuse(QColor(200, 200, 220));

    modelEntity->addComponent(modelMeshRenderer);
    modelEntity->addComponent(material);
    modelEntity->addComponent(modelTransform);

    frameCameraToAabb(minP, maxP);

    return true;
}

void ProcessModelWithQt3D::on_load3DModelBtn_clicked()
{
    QString filePath = QFileDialog::getOpenFileName(this, "打开文件", QString(), "STL Files (*.stl)");
    if (filePath.isEmpty()) {
        qDebug() << "文件加载取消";
        return;
    }

    loadStlIntoEntity(filePath);
}

void ProcessModelWithQt3D::on_directionComboBox_currentIndexChanged(int index)
{
    if (!modelTransform) return;
    // 0: XY, 1: XZ, 2: YZ - rotate model accordingly as an example
    switch (index) {
    case 0: modelTransform->setRotation(QQuaternion::fromEulerAngles(0, 0, 0)); break;
    case 1: modelTransform->setRotation(QQuaternion::fromEulerAngles(90, 0, 0)); break;
    case 2: modelTransform->setRotation(QQuaternion::fromEulerAngles(0, 90, 0)); break;
    default: break;
    }
}

void ProcessModelWithQt3D::on_createGrayImage_clicked()
{
    QMessageBox::information(this, "灰度图", "灰度图生成功能待实现");
}

void ProcessModelWithQt3D::on_saveGrayImage_clicked()
{
    QMessageBox::information(this, "保存", "保存功能待实现");
}

bool ProcessModelWithQt3D::parseStlFile(const QString &filePath, QList<Triangle> &outTriangles, QVector3D &outMin, QVector3D &outMax)
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) return false;
    QByteArray data = f.readAll();
    f.close();

    // Detect ASCII vs Binary
    auto isLikelyAscii = [&]() -> bool {
        if (data.size() < 6) return false;
        QByteArray head = data.left(6).trimmed();
        if (!head.startsWith("solid")) return false;
        // If contains non-text or has binary count at pos 80, treat as binary
        if (data.size() > 84) {
            // Check if bytes 80-84 look like a reasonable triangle count
            const uchar *p = reinterpret_cast<const uchar*>(data.constData());
            quint32 triCount = *reinterpret_cast<const quint32*>(p + 80);
            // Heuristic: if triCount*50 + 84 equals size, it's binary
            if (84u + triCount * 50u == static_cast<quint32>(data.size()))
                return false;
        }
        return true;
    }();

    outTriangles.clear();
    outMin = QVector3D(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
    outMax = QVector3D(-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max());

    bool ok = false;
    if (isLikelyAscii) {
        ok = parseAsciiStl(data, outTriangles, outMin, outMax);
        if (!ok) {
            // Fallback to binary
            ok = parseBinaryStl(data, outTriangles, outMin, outMax);
        }
    } else {
        ok = parseBinaryStl(data, outTriangles, outMin, outMax);
        if (!ok) {
            ok = parseAsciiStl(data, outTriangles, outMin, outMax);
        }
    }
    return ok;
}

static inline QVector3D readFloat3LE(const uchar *p)
{
    float x = *reinterpret_cast<const float*>(p);
    float y = *reinterpret_cast<const float*>(p + 4);
    float z = *reinterpret_cast<const float*>(p + 8);
    return QVector3D(x, y, z);
}

bool ProcessModelWithQt3D::parseBinaryStl(const QByteArray &data, QList<Triangle> &outTriangles, QVector3D &outMin, QVector3D &outMax)
{
    if (data.size() < 84) return false;
    const uchar *p = reinterpret_cast<const uchar*>(data.constData());
    quint32 triCount = *reinterpret_cast<const quint32*>(p + 80);
    quint64 expected = 84ull + 50ull * triCount;
    if (static_cast<quint64>(data.size()) < expected) return false;

    outTriangles.reserve(static_cast<int>(triCount));

    const uchar *cur = p + 84;
    for (quint32 i = 0; i < triCount; ++i) {
        Triangle t;
        t.normal = readFloat3LE(cur); cur += 12;
        t.v0 = readFloat3LE(cur); cur += 12;
        t.v1 = readFloat3LE(cur); cur += 12;
        t.v2 = readFloat3LE(cur); cur += 12;
        cur += 2; // attribute byte count

        outTriangles.append(t);

        outMin.setX(qMin(outMin.x(), qMin(t.v0.x(), qMin(t.v1.x(), t.v2.x()))));
        outMin.setY(qMin(outMin.y(), qMin(t.v0.y(), qMin(t.v1.y(), t.v2.y()))));
        outMin.setZ(qMin(outMin.z(), qMin(t.v0.z(), qMin(t.v1.z(), t.v2.z()))));
        outMax.setX(qMax(outMax.x(), qMax(t.v0.x(), qMax(t.v1.x(), t.v2.x()))));
        outMax.setY(qMax(outMax.y(), qMax(t.v0.y(), qMax(t.v1.y(), t.v2.y()))));
        outMax.setZ(qMax(outMax.z(), qMax(t.v0.z(), qMax(t.v1.z(), t.v2.z()))));
    }

    return true;
}

bool ProcessModelWithQt3D::parseAsciiStl(const QByteArray &data, QList<Triangle> &outTriangles, QVector3D &outMin, QVector3D &outMax)
{
    QTextStream ts(QString::fromUtf8(data));

    QString line;
    Triangle current;
    int vertexCountInFacet = 0;

    auto updateAabb = [&](const QVector3D &v){
        outMin.setX(qMin(outMin.x(), v.x()));
        outMin.setY(qMin(outMin.y(), v.y()));
        outMin.setZ(qMin(outMin.z(), v.z()));
        outMax.setX(qMax(outMax.x(), v.x()));
        outMax.setY(qMax(outMax.y(), v.y()));
        outMax.setZ(qMax(outMax.z(), v.z()));
    };

    while (ts.readLineInto(&line)) {
        QString trimmed = line.trimmed();
        if (trimmed.startsWith("facet normal", Qt::CaseInsensitive)) {
            QStringList parts = trimmed.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
            if (parts.size() >= 5) {
                current.normal = QVector3D(parts[2].toFloat(), parts[3].toFloat(), parts[4].toFloat());
            }
            vertexCountInFacet = 0;
        } else if (trimmed.startsWith("vertex", Qt::CaseInsensitive)) {
            QStringList parts = trimmed.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
            if (parts.size() >= 4) {
                QVector3D v(parts[1].toFloat(), parts[2].toFloat(), parts[3].toFloat());
                if (vertexCountInFacet == 0) current.v0 = v;
                else if (vertexCountInFacet == 1) current.v1 = v;
                else if (vertexCountInFacet == 2) current.v2 = v;
                vertexCountInFacet++;
                updateAabb(v);
            }
        } else if (trimmed.startsWith("endfacet", Qt::CaseInsensitive)) {
            if (vertexCountInFacet == 3) {
                outTriangles.append(current);
            }
            vertexCountInFacet = 0;
        }
    }

    return !outTriangles.isEmpty();
}

void ProcessModelWithQt3D::buildGeometryFromTriangles(const QList<Triangle> &triangles)
{
    if (!modelMeshRenderer) return;

    const int triCount = triangles.size();
    const int vertexCount = triCount * 3;
    const int strideFloats = 3; // positions only per attribute buffer; normals in separate buffer

    QByteArray positionBytes;
    positionBytes.resize(static_cast<int>(vertexCount * strideFloats * sizeof(float)));
    float *posPtr = reinterpret_cast<float*>(positionBytes.data());

    QByteArray normalBytes;
    normalBytes.resize(static_cast<int>(vertexCount * strideFloats * sizeof(float)));
    float *nrmPtr = reinterpret_cast<float*>(normalBytes.data());

    int idx = 0;
    for (const Triangle &t : triangles) {
        const QVector3D n = t.normal;
        const QVector3D vs[3] = { t.v0, t.v1, t.v2 };
        for (int i = 0; i < 3; ++i) {
            const QVector3D &v = vs[i];
            posPtr[0] = v.x(); posPtr[1] = v.y(); posPtr[2] = v.z(); posPtr += 3;
            nrmPtr[0] = n.x(); nrmPtr[1] = n.y(); nrmPtr[2] = n.z(); nrmPtr += 3;
            idx++;
        }
    }

    auto geometry = new Qt3DRender::QGeometry(modelMeshRenderer);

    // Position buffer and attribute
    auto positionBuffer = new Qt3DRender::QBuffer(geometry);
    positionBuffer->setUsage(Qt3DRender::QBuffer::StaticDraw);
    positionBuffer->setData(positionBytes);

    auto positionAttribute = new Qt3DRender::QAttribute(geometry);
    positionAttribute->setName(Qt3DRender::QAttribute::defaultPositionAttributeName());
    positionAttribute->setAttributeType(Qt3DRender::QAttribute::VertexAttribute);
    positionAttribute->setBuffer(positionBuffer);
    positionAttribute->setVertexBaseType(Qt3DRender::QAttribute::Float);
    positionAttribute->setVertexSize(3);
    positionAttribute->setByteStride(sizeof(float) * 3);
    positionAttribute->setCount(vertexCount);

    // Normal buffer and attribute
    auto normalBuffer = new Qt3DRender::QBuffer(geometry);
    normalBuffer->setUsage(Qt3DRender::QBuffer::StaticDraw);
    normalBuffer->setData(normalBytes);

    auto normalAttribute = new Qt3DRender::QAttribute(geometry);
    normalAttribute->setName(Qt3DRender::QAttribute::defaultNormalAttributeName());
    normalAttribute->setAttributeType(Qt3DRender::QAttribute::VertexAttribute);
    normalAttribute->setBuffer(normalBuffer);
    normalAttribute->setVertexBaseType(Qt3DRender::QAttribute::Float);
    normalAttribute->setVertexSize(3);
    normalAttribute->setByteStride(sizeof(float) * 3);
    normalAttribute->setCount(vertexCount);

    geometry->addAttribute(positionAttribute);
    geometry->addAttribute(normalAttribute);

    modelMeshRenderer->setPrimitiveType(Qt3DRender::QGeometryRenderer::Triangles);
    modelMeshRenderer->setGeometry(geometry);
}

void ProcessModelWithQt3D::frameCameraToAabb(const QVector3D &minP, const QVector3D &maxP)
{
    QVector3D center = (minP + maxP) * 0.5f;
    QVector3D extent = maxP - minP;
    float radius = 0.5f * qMax(extent.length(), qMax(qMax(extent.x(), extent.y()), extent.z()));
    if (radius <= 0.0f) radius = 1.0f;

    // Use camera vertical FOV
    float fovDeg = 45.0f;
    if (camera && camera->lens()) fovDeg = camera->lens()->fieldOfView();
    float fovRad = qDegreesToRadians(fovDeg);
    float distance = radius / qTan(fovRad * 0.5f) * 1.5f; // margin

    camera->setViewCenter(center);
    camera->setPosition(center + QVector3D(0, 0, distance));
}