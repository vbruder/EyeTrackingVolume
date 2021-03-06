/**
 * \file
 *
 * \author Valentin Bruder
 *
 * \copyright Copyright (C) 2018 Valentin Bruder
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#pragma once

#include <QObject>
#include <QWidget>
#include <QDir>
#include <QPointer>
#include <QQuaternion>
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLBuffer>
#include <QOpenGLShader>
#include <QOpenGLShaderProgram>
#include <QMatrix4x4>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QOpenGLTexture>
#include <QPropertyAnimation>
#include <qopenglfunctions_4_3_core.h>
#include <QPainter>
#include <QElapsedTimer>

#include "src/core/volumerendercl.h"

class VolumeRenderWidget : public QOpenGLWidget, protected QOpenGLFunctions_4_3_Core
{
    Q_OBJECT

public:
    explicit VolumeRenderWidget(QWidget *parent = nullptr);
    virtual ~VolumeRenderWidget() override;

    void setupVertexAttribs();

    void setVolumeData(const QString &fileName);

    bool hasData() const;

    const QVector4D getVolumeResolution() const;

    void mousePressEvent(QMouseEvent *event) Q_DECL_OVERRIDE;

    void mouseReleaseEvent(QMouseEvent *event) Q_DECL_OVERRIDE;

    void mouseMoveEvent(QMouseEvent *event) Q_DECL_OVERRIDE;

    void wheelEvent(QWheelEvent *event) Q_DECL_OVERRIDE;

    void mouseDoubleClickEvent(QMouseEvent *event) Q_DECL_OVERRIDE;

    void keyReleaseEvent(QKeyEvent *event) Q_DECL_OVERRIDE;

    void updateView(const float dx = 0, const float dy = 0);

    bool getLoadingFinished() const;
    void setLoadingFinished(const bool loadingFinished);

    QVector3D getCamTranslation() const;
    void setCamTranslation(const QVector3D &translation);

    QQuaternion getCamRotation() const;
    void setCamRotation(const QQuaternion &rotQuat);

    void setBBox(QVector3D botLeft, QVector3D topRight);

    void setFilter(int id, int value);
public slots:
    void cleanup();
    void resetCam();

    void updateSamplingRate(double samplingRate);
    void updateTransferFunction(QGradientStops stops, int id = 0);
    std::vector<unsigned char> getRawTransferFunction(QGradientStops stops) const;
    void setRawTransferFunction(std::vector<unsigned char> tff);

#undef Bool
    void setTffInterpolation(const QString method);
    void setCamOrtho(const bool camOrtho);
    void setContRendering(const bool setContRendering);
    void setIllumination(const int illum);
    void setLinearInterpolation(const bool linear);
    void setContours(const bool contours);
    void setAerial(const bool aerial);
    /**
     * @brief Set image order empty space skipping.
     * @param useEss
     */
    void setImgEss(bool useEss);
    /**
     * @brief Set object order empty space skipping.
     * @param useEss
     */
    void setObjEss(bool useEss);
    void setDrawBox(bool box);
    void setBackgroundColor(const QColor col);
    void setImageSamplingRate(const double samplingRate);
    void setShowOverlay(bool showOverlay);

    void saveFrame();
    void toggleVideoRecording();
    void toggleViewRecording();
	void toggleInteractionLogging();
    void setTimeStep(int timestep);
    void setAmbientOcclusion(bool ao);

    void generateLowResVolume();

    void read(const QJsonObject &json);
    void write(QJsonObject &json) const;

    void showSelectOpenCL();
    void reloadKernels();

    const std::array<double, 256> &getHistogram(unsigned int timestep = 0);
    void setStride(const int stride);
    void setDataScaling(const int id, const float scaling);
    const std::vector<float> &getDataRangeMaxs();
    QImage getSliceImage(unsigned int id);
    void clearFrames();
signals:
    void fpsChanged(double);
    void frameSizeChanged(QSize);
    void timeSeriesLoaded(int);
    void pickedTimestepChanged(float, QColor);

protected:
    // Qt specific QOpenGLWidget methods
    void initializeGL() Q_DECL_OVERRIDE;
    void paintGL() Q_DECL_OVERRIDE;
    void resizeGL(const int w, const int h) Q_DECL_OVERRIDE;

private:
    void paintOrientationAxis(QPainter &p);
    void paintFps(QPainter &p, const double fps, const double lastTime);
    double getFps();

    /**
     * @brief Initialize the OpenCL volume renderer.
     * @param useGL use OpenGL context sharing
     * @param useCPU use CPU as OpenCL device for rendering
     */
    void initVolumeRenderer(bool useGL = true, const bool useCPU = false);

	/**
	 * @brief create the OpenGL output texture to display on the screen quad.
	 * @param width Width of the texture in pixels.
	 * @param height Height of the texture in pixels.
	 */
    void generateOutputTextures(const int width, const int height);

	/**
	 * @brief Log camera configurations rotation and zoom) to two files selected by the user.
	 */
    void recordViewConfig() const;

	/**
	 * @brief Log a user interaction to file.
	 * @param str String containing the user interaction in the fowllowing format:
	 *	          time stamp, interaction type, interaction parameters
	 */
	void logInteraction(const QString &str) const;

    // -------Member variables--------
    //
    // OpenGL
    QOpenGLVertexArrayObject _screenQuadVao;
    QOpenGLShaderProgram _spScreenQuad;
    QOpenGLShaderProgram _spOverlaysGL;
    QOpenGLBuffer _quadVbo;
    GLuint _overlayFboId;
    GLuint _overlayTexId;

    QMatrix4x4 _screenQuadProjMX;
    QMatrix4x4 _viewMX;
    QMatrix4x4 _modelMX;
    QMatrix4x4 _coordViewMX;
    QMatrix4x4 _overlayProjMX;
    QMatrix4x4 _overlayModelMX;

    QPoint _tffRange;
    GLuint _outTexId;
    VolumeRenderCL _volumerender;
    QEasingCurve _tffInterpol;
    int _timestep;

    // global rendering flags
    QPoint _lastLocalCursorPos;
    QQuaternion _rotQuat;
    QVector3D _translation;

    bool _noUpdate;
    bool _loadingFinished;
    bool _writeImage;
    bool _recordVideo;
    qint64 _imgCount;
    QVector<double> _times;
    double _imgSamplingRate;       // image oversampling rate
    bool _useGL;
    bool _showOverlay;
    bool _logView;
	bool _logInteraction;
    QString _viewLogFile;
	QString _interactionLogFile;
    bool _contRendering;
    QGradientStops _tffStops;
	QElapsedTimer _timer;
    float _zScale;
    float _pickedZ;
    QVector<QColor> _colorTable;
    int _frameCnt;
};
