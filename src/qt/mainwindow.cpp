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

#include "src/qt/mainwindow.h"
#include "ui_mainwindow.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QString>
#include <QUrl>
#include <QFuture>
#include <QtConcurrentRun>
#include <QThread>
#include <QColorDialog>
#include <QtGlobal>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QJsonObject>
#include <QJsonDocument>
#include <QGroupBox>

/**
 * @brief MainWindow::MainWindow
 * @param parent
 */
MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent)
  , ui(new Ui::MainWindow)
  , _fileName("No volume data loaded yet.")
{
    QCoreApplication::setOrganizationName("VISUS");
    QCoreApplication::setOrganizationDomain("www.visus.uni-stuttgart.de");
    QCoreApplication::setApplicationName("VolumeRaycasterCL");
    _settings = new QSettings();

    setAcceptDrops( true );
    ui->setupUi(this);
    ui->gbTimeSeries->setVisible(false);
    connect(ui->volumeRenderWidget, &VolumeRenderWidget::timeSeriesLoaded,
            ui->gbTimeSeries, &QGroupBox::setVisible);
    connect(ui->volumeRenderWidget, &VolumeRenderWidget::timeSeriesLoaded,
            ui->sbTimeStep, &QSpinBox::setMaximum);
    connect(ui->volumeRenderWidget, &VolumeRenderWidget::timeSeriesLoaded,
            ui->sldTimeStep, &QSlider::setMaximum);
    connect(ui->sldTimeStep, &QSlider::valueChanged,
            ui->volumeRenderWidget, &VolumeRenderWidget::setTimeStep);
    connect(ui->pbPlay, &QPushButton::released, this, &MainWindow::setLoopTimesteps);
    connect(ui->sbSpeed, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            this, &MainWindow::setPlaybackSpeed);

    // menu bar actions
    connect(ui->actionOpen, &QAction::triggered, this, &MainWindow::openVolumeFile);
    connect(ui->actionSaveCpTff, &QAction::triggered, this, &MainWindow::saveTff);
    connect(ui->actionSaveRawTff_2, &QAction::triggered, this, &MainWindow::saveRawTff);
    connect(ui->actionLoadCpTff, &QAction::triggered, this, &MainWindow::loadTff);
    connect(ui->actionLoadRawTff, &QAction::triggered, this, &MainWindow::loadRawTff);
    connect(ui->actionScreenshot, &QAction::triggered,
            ui->volumeRenderWidget, &VolumeRenderWidget::saveFrame);
    connect(ui->actionRecord, &QAction::triggered,
            ui->volumeRenderWidget, &VolumeRenderWidget::toggleVideoRecording);
    connect(ui->actionRecordCamera, &QAction::triggered,
            ui->volumeRenderWidget, &VolumeRenderWidget::toggleViewRecording);
	connect(ui->actionLogInteraction, &QAction::triggered,
			ui->volumeRenderWidget, &VolumeRenderWidget::toggleInteractionLogging);
    connect(ui->actionGenerateLowResVo, &QAction::triggered,
            ui->volumeRenderWidget, &VolumeRenderWidget::generateLowResVolume);
    connect(ui->actionResetCam, &QAction::triggered,
            ui->volumeRenderWidget, &VolumeRenderWidget::resetCam);
    connect(ui->actionSaveState, &QAction::triggered, this, &MainWindow::saveCamState);
    connect(ui->actionLoadState, &QAction::triggered, this, &MainWindow::loadCamState);
    connect(ui->actionShowOverlay, &QAction::toggled,
            ui->volumeRenderWidget, &VolumeRenderWidget::setShowOverlay);
    connect(ui->actionSelectOpenCL, &QAction::triggered,
            ui->volumeRenderWidget, &VolumeRenderWidget::showSelectOpenCL);
    connect(ui->actionAbout, &QAction::triggered, this, &MainWindow::showAboutDialog);
    connect(ui->actionRealoadKernel, &QAction::triggered,
            ui->volumeRenderWidget, &VolumeRenderWidget::reloadKernels);
    connect(ui->actionRealoadKernel, &QAction::triggered,
            this, &MainWindow::updateTransferFunctionFromGradientStops);

    connect(ui->chbLog, &QCheckBox::toggled, this, &MainWindow::updateHistograms);

    // future watcher for concurrent data loading
    _watcher = new QFutureWatcher<void>(this);
    connect(_watcher, &QFutureWatcher<void>::finished, this, &MainWindow::finishedLoading);
    // loading progress bar
    _progBar.setRange(0, 100);
    _progBar.setTextVisible(true);
    _progBar.setAlignment(Qt::AlignCenter);
    connect(&_timer, &QTimer::timeout, this, &MainWindow::addProgress);

    // connect settings UI
    connect(ui->dsbSamplingRate, 
            static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
            ui->volumeRenderWidget, &VolumeRenderWidget::updateSamplingRate);
    connect(ui->dsbImgSampling, 
            static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
            ui->volumeRenderWidget, &VolumeRenderWidget::setImageSamplingRate);
    connect(ui->cbIllum, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            ui->volumeRenderWidget, &VolumeRenderWidget::setIllumination);
    connect(ui->pbBgColor, &QPushButton::released, this, &MainWindow::chooseBackgroundColor);
    connect(ui->sldStride, &QSlider::valueChanged,
            ui->volumeRenderWidget, &VolumeRenderWidget::setStride);
    connect(ui->sldScaleGaze, &QSlider::valueChanged, this, &MainWindow::setScaleGaze);
    connect(ui->sldScaleMag, &QSlider::valueChanged, this, &MainWindow::setScaleMag);
    connect(ui->sldScaleAngle, &QSlider::valueChanged, this, &MainWindow::setScaleAngle);
    connect(ui->sldTimeScaling, &QSlider::valueChanged, this, &MainWindow::setScaleTime);

    // check boxes
    connect(ui->chbLinear, &QCheckBox::toggled,
            ui->volumeRenderWidget, &VolumeRenderWidget::setLinearInterpolation);
    connect(ui->chbAmbientOcclusion, &QCheckBox::toggled,
            ui->volumeRenderWidget, &VolumeRenderWidget::setAmbientOcclusion);
    connect(ui->chbContours, &QCheckBox::toggled,
            ui->volumeRenderWidget, &VolumeRenderWidget::setContours);
    connect(ui->chbAerial, &QCheckBox::toggled,
            ui->volumeRenderWidget, &VolumeRenderWidget::setAerial);
    connect(ui->chbImageESS, &QCheckBox::toggled,
            ui->volumeRenderWidget, &VolumeRenderWidget::setImgEss);
    connect(ui->chbObjectESS, &QCheckBox::toggled,
            ui->volumeRenderWidget, &VolumeRenderWidget::setObjEss);
    connect(ui->chbBox, &QCheckBox::toggled,
            ui->volumeRenderWidget, &VolumeRenderWidget::setDrawBox);
    connect(ui->chbOrtho, &QCheckBox::toggled,
            ui->volumeRenderWidget, &VolumeRenderWidget::setCamOrtho);
    connect(ui->chbContRendering, &QCheckBox::toggled,
            ui->volumeRenderWidget, &VolumeRenderWidget::setContRendering);
    // connect tff editor default tf0
    connect(ui->tf0->getEditor(), &TransferFunctionEditor::gradientStopsChanged,
            this, &MainWindow::updateTransferFunctionFromGradientStops0);
    // tf1
    connect(ui->tf1->getEditor(), &TransferFunctionEditor::gradientStopsChanged,
            this, &MainWindow::updateTransferFunctionFromGradientStops1);
    // tf2
    connect(ui->tf2->getEditor(), &TransferFunctionEditor::gradientStopsChanged,
            this, &MainWindow::updateTransferFunctionFromGradientStops2);
    // other TF related controls
    connect(ui->pbResetTff, &QPushButton::clicked,
            ui->tf0, &TransferFunctionWidget::resetTransferFunction);
    connect(ui->cbInterpolation,
            static_cast<void (QComboBox::*)(const QString &)>(&QComboBox::currentIndexChanged),
            ui->tf0, &TransferFunctionWidget::setInterpolation);
    connect(ui->cbInterpolation,
            static_cast<void (QComboBox::*)(const QString &)>(&QComboBox::currentIndexChanged),
            ui->volumeRenderWidget, &VolumeRenderWidget::setTffInterpolation);
//    connect(ui->cbInterpolation, SIGNAL(currentIndexChanged(QString)),
//            ui->tf0, SLOT(setInterpolation(QString)));
//    connect(ui->cbInterpolation, qOverload<const QString &>(&QComboBox::currentIndexChanged),
//            ui->tf0, &TransferFunctionEditor::setInterpolation);

    // color wheel - TF interaction
    connect(ui->tf0->getEditor(), &TransferFunctionEditor::selectedPointChanged,
            ui->colorWheel, &colorwidgets::ColorWheel::setColor);
    connect(ui->colorWheel, &colorwidgets::ColorWheel::colorChanged,
            ui->tf0, &TransferFunctionWidget::setColorSelected);

    // filters
    connect(ui->cbTfColor,
            static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &MainWindow::setFilterColor);
    connect(ui->cbFilter1,
            static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &MainWindow::setFilter1);
    connect(ui->cbFilter2,
            static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &MainWindow::setFilter2);
    connect(ui->cbFilter3,
            static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &MainWindow::setFilter3);

    ui->statusBar->addPermanentWidget(&_statusLabel);
    connect(ui->volumeRenderWidget, &VolumeRenderWidget::frameSizeChanged,
            this, &MainWindow::setStatusText);
    connect(ui->volumeRenderWidget, &VolumeRenderWidget::pickedTimestepChanged,
            this, &MainWindow::setPickedTimestep);

    connect(&_loopTimer, &QTimer::timeout, this, &MainWindow::nextTimestep);

    // clipping sliders
    connect(ui->sldClipBack, &QSlider::valueChanged, this, &MainWindow::updateBBox);
    connect(ui->sldClipBottom, &QSlider::valueChanged, this, &MainWindow::updateBBox);
    connect(ui->sldClipFront, &QSlider::valueChanged, this, &MainWindow::updateBBox);
    connect(ui->sldClipLeft, &QSlider::valueChanged, this, &MainWindow::updateBBox);
    connect(ui->sldClipRight, &QSlider::valueChanged, this, &MainWindow::updateBBox);
    connect(ui->sldClipTop, &QSlider::valueChanged, this, &MainWindow::updateBBox);
    connect(ui->pbResetClipping, &QPushButton::pressed, this, &MainWindow::resetBBox);

    // restore settings
    readSettings();
}


/**
 * @brief MainWindow::~MainWindow
 */
MainWindow::~MainWindow()
{
    delete _watcher;
    delete _settings;
    delete ui;
}


/**
 * @brief MainWindow::closeEvent
 * @param event
 */
void MainWindow::closeEvent(QCloseEvent *event)
{
    writeSettings();
    event->accept();
}


void MainWindow::keyPressEvent(QKeyEvent *event)
{
    float factor = 0.01f;
    if (event->modifiers() & Qt::ShiftModifier)
        factor = 0.001f;

    switch (event->key())
    {
        case Qt::Key_Up:
        case Qt::Key_W: ui->volumeRenderWidget->updateView(+0.000f,-factor); break;
        case Qt::Key_Left:
        case Qt::Key_A: ui->volumeRenderWidget->updateView(-factor, 0.000f); break;
        case Qt::Key_Down:
        case Qt::Key_S: ui->volumeRenderWidget->updateView(+0.000f,+factor); break;
        case Qt::Key_Right:
        case Qt::Key_D: ui->volumeRenderWidget->updateView(+factor, 0.000f); break;
        // TODO: add zoom
        case Qt::Key_Escape: ui->volumeRenderWidget->clearFrames(); break;
    }
    event->accept();
}


/**
 * @brief MainWindow::showEvent
 * @param event
 */
void MainWindow::showEvent(QShowEvent *event)
{
    ui->tf0->resetTransferFunction();
    ui->tf1->resetTransferFunction();
    ui->tf2->resetTransferFunction();
    event->accept();
}


/**
 * @brief MainWindow::writeSettings
 */
void MainWindow::writeSettings()
{
    _settings->beginGroup("MainWindow");
    _settings->setValue("geometry", saveGeometry());
    _settings->setValue("windowState", saveState());
    _settings->endGroup();

    _settings->beginGroup("Settings");
    // TODO
    _settings->endGroup();
}


/**
 * @brief MainWindow::readSettings
 */
void MainWindow::readSettings()
{
    _settings->beginGroup("MainWindow");
    restoreGeometry(_settings->value("geometry").toByteArray());
    restoreState(_settings->value("windowState").toByteArray());
    _settings->endGroup();

    _settings->beginGroup("Settings");
    // todo
    _settings->endGroup();
}


/**
 * @brief MainWindow::setVolumeData
 * @param fileName
 */
void MainWindow::setVolumeData(const QString &fileName)
{
    ui->volumeRenderWidget->setVolumeData(fileName);
    ui->volumeRenderWidget->updateTransferFunction(
                ui->tf0->getEditor()->getGradientStops(), 0);
    ui->volumeRenderWidget->updateTransferFunction(
                ui->tf1->getEditor()->getGradientStops(), 1);
    ui->volumeRenderWidget->updateTransferFunction(
                ui->tf2->getEditor()->getGradientStops(), 2);
    ui->volumeRenderWidget->updateView();
}


/**
 * @brief MainWindow::updateTransferFunction - Default Tf
 */
void MainWindow::updateTransferFunctionFromGradientStops()
{
    ui->volumeRenderWidget->updateTransferFunction(
                ui->tf0->getEditor()->getGradientStops(), 0);
}

/**
 * @brief MainWindow::updateTransferFunctionFromGradientStops0
 * @param stops
 */
void MainWindow::updateTransferFunctionFromGradientStops0(QGradientStops stops)
{
    ui->volumeRenderWidget->updateTransferFunction(stops, 0);
}

/**
 * @brief MainWindow::updateTransferFunctionFromGradientStops0
 */
void MainWindow::updateTransferFunctionFromGradientStops1(QGradientStops stops)
{
    ui->volumeRenderWidget->updateTransferFunction(stops, 1);
}

/**
 * @brief MainWindow::updateTransferFunctionFromGradientStops1
 */
void MainWindow::updateTransferFunctionFromGradientStops2(QGradientStops stops)
{
    ui->volumeRenderWidget->updateTransferFunction(stops, 2);
}

/**
 * @brief MainWindow::setScaleGaze
 * @param scaling
 */
void MainWindow::setScaleGaze(int scaling)
{
    ui->volumeRenderWidget->setDataScaling(0, static_cast<float>(scaling));
}

/**
 * @brief MainWindow::setScaleMag
 * @param scaling
 */
void MainWindow::setScaleMag(int scaling)
{
    ui->volumeRenderWidget->setDataScaling(1, static_cast<float>(scaling));
}

/**
 * @brief MainWindow::setScaleAngle
 * @param scaling
 */
void MainWindow::setScaleAngle(int scaling)
{
    ui->volumeRenderWidget->setDataScaling(2, static_cast<float>(scaling));
}

/**
 * @brief MainWindow::setScaleTime
 * @param scaling
 */
void MainWindow::setScaleTime(int scaling)
{
    float scale = static_cast<float>(scaling);
    if (scale <= 100)
        scale = scale / 100;
    else
        scale = 1.f + (scale - 100) / 10;
    ui->volumeRenderWidget->setDataScaling(3, scale);
}

/**
 * @brief MainWindow::setLoopTimesteps
 */
void MainWindow::setLoopTimesteps()
{
    if (!_loopTimer.isActive())
    {
        _loopTimer.start(ui->sbSpeed->value());
        ui->pbPlay->setIcon(QIcon::fromTheme("media-playback-pause"));
    }
    else
    {
        _loopTimer.stop();
        ui->pbPlay->setIcon(QIcon::fromTheme("media-playback-start"));
    }
}

/**
 * @brief MainWindow::setPlaybackSpeed
 * @param speed
 */
void MainWindow::setPlaybackSpeed(int speed)
{
    _loopTimer.setInterval(speed);
}

/**
 * @brief MainWindow::nextTimestep
 */
void MainWindow::nextTimestep()
{
    int val = ui->sbTimeStep->value() + 1;
    if (val > ui->sbTimeStep->maximum() && ui->chbLoop->isChecked())
        val = 0;
    else if (val > ui->sbTimeStep->maximum())
    {
        _loopTimer.stop();
        val = ui->sbTimeStep->maximum();
    }
    ui->sldTimeStep->setValue(val);
    ui->sbTimeStep->setValue(val);
}

/**
 * @brief MainWindow::loadCamState
 */
void MainWindow::loadCamState()
{
    QFileDialog dialog;
    QString defaultPath = _settings->value( "LastStateFile" ).toString();
    QString pickedFile = dialog.getOpenFileName(this, tr("Save State"),
                                                defaultPath, tr("JSON files (*.json)"));
    if (pickedFile.isEmpty())
        return;
    _settings->setValue( "LastStateFile", pickedFile );

    QFile loadFile(pickedFile);
    if (!loadFile.open(QIODevice::ReadOnly))
    {
        qWarning() << "Couldn't open state file" << pickedFile;
        return;
    }

    QByteArray saveData = loadFile.readAll();
    QJsonDocument loadDoc(QJsonDocument::fromJson(saveData));
    QJsonObject json = loadDoc.object();

    if (json.contains("imgResFactor") && json["imgResFactor"].isDouble())
            ui->dsbImgSampling->setValue(json["imgResFactor"].toDouble());
    if (json.contains("rayStepSize") && json["rayStepSize"].isDouble())
            ui->dsbSamplingRate->setValue(json["rayStepSize"].toDouble());

    if (json.contains("useLerp") && json["useLerp"].isBool())
            ui->chbLinear->setChecked(json["useLerp"].toBool());
    if (json.contains("useAO") && json["useAO"].isBool())
            ui->chbAmbientOcclusion->setChecked(json["useAO"].toBool());
    if (json.contains("showContours") && json["showContours"].isBool())
            ui->chbContours->setChecked(json["showContours"].toBool());
    if (json.contains("useAerial") && json["useAerial"].isBool())
            ui->chbAerial->setChecked(json["useAerial"].toBool());
    if (json.contains("showBox") && json["showBox"].isBool())
            ui->chbBox->setChecked(json["showBox"].toBool());
    if (json.contains("useOrtho") && json["useOrtho"].isBool())
            ui->chbOrtho->setChecked(json["useOrtho"].toBool());
    // camera paramters
    ui->volumeRenderWidget->read(json);
}

/**
 * @brief MainWindow::showAboutDialog
 */
void MainWindow::showAboutDialog()
{
    QMessageBox::about(this, "About Volume Raycaster",
"<b>Volume Raycaster 2018</b><br><br>\
Check out the \
<a href='https://bitbucket.org/theVall/basicvolumeraycaster/overview'>Bitbucket repository</a> \
for more information.<br><br>\
Copyright 2017-2018 Valentin Bruder. All rights reserved. <br><br>\
The program is provided AS IS with NO WARRANTY OF ANY KIND, \
INCLUDING THE WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS \
FOR A PARTICULAR PURPOSE.");
}

/**
 * @brief MainWindow::saveCamState
 */
void MainWindow::saveCamState()
{
    QFileDialog dialog;
    QString defaultPath = _settings->value( "LastStateFile" ).toString();
    QString pickedFile = dialog.getSaveFileName(this, tr("Save State"),
                                                defaultPath, tr("JSON files (*.json)"));
    if (pickedFile.isEmpty())
        return;
    _settings->setValue( "LastStateFile", pickedFile );

    QFile saveFile(pickedFile);
    if (!saveFile.open(QIODevice::WriteOnly))
    {
        qWarning() << "Couldn't open save file" << pickedFile;
        return;
    }

    QJsonObject stateObject;
    // resolution
    stateObject["imgResFactor"] = ui->dsbImgSampling->value();
    stateObject["rayStepSize"] = ui->dsbSamplingRate->value();
    // rendering flags
    stateObject["useLerp"] = ui->chbLinear->isChecked();
    stateObject["useAO"] = ui->chbAmbientOcclusion->isChecked();
    stateObject["showContours"] = ui->chbContours->isChecked();
    stateObject["useAerial"] = ui->chbAerial->isChecked();
    stateObject["showBox"] = ui->chbBox->isChecked();
    stateObject["useOrtho"] = ui->chbOrtho->isChecked();
    // camera parameters
    ui->volumeRenderWidget->write(stateObject);

    QJsonDocument saveDoc(stateObject);
    saveFile.write(saveDoc.toJson());
}


/**
 * @brief MainWindow::readVolumeFile
 * @param fileName
 * @return
 */
bool MainWindow::readVolumeFile(const QString &fileName)
{
    ui->volumeRenderWidget->setLoadingFinished(false);
    _progBar.setFormat("Loading volume file: " + fileName);
    _progBar.setValue(1);
    _progBar.show();
    ui->statusBar->addPermanentWidget(&_progBar, 2);
    ui->statusBar->updateGeometry();
    QApplication::processEvents();

    if (fileName.isEmpty())
    {
        _progBar.deleteLater();
        throw std::invalid_argument("Invalid volume data file name.");
    }
    else
    {
        _fileName = fileName;
        QFuture<void> future = QtConcurrent::run(this, &MainWindow::setVolumeData, fileName);
        _watcher->setFuture(future);
        _timer.start(100);
    }
    return true;
}


/**
 * @brief MainWindow::readTff
 * @param fileName
 * @return
 */
void MainWindow::readTff(const QString &fileName)
{
    if (fileName.isEmpty())
    {
        throw std::invalid_argument("Invalid trtansfer function file name.");
    }
    else
    {
        QFile file(fileName);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            throw std::invalid_argument("Could not open transfer function file "
                                        + fileName.toStdString());
        }
        else
        {
            QTextStream in(&file);
            QGradientStops stops;
            while (!in.atEnd())
            {
                QStringList line = in.readLine().split(QRegExp("\\s"));
                if (line.size() < 5)
                    continue;
                QGradientStop stop(line.at(0).toDouble(),
                                   QColor(line.at(1).toInt(), line.at(2).toInt(),
                                          line.at(3).toInt(), line.at(4).toInt()));
                stops.push_back(stop);
            }
            if (!stops.isEmpty())
            {
                ui->tf0->getEditor()->setGradientStops(stops);
                ui->tf0->getEditor()->pointsUpdated();
            }
            else
                qCritical() << "Empty transfer function file.";
            file.close();
        }
    }
}


/**
 * @brief MainWindow::saveTff
 * @param fileName
 */
void MainWindow::saveTff()
{
    QFileDialog dia;
    QString defaultPath = _settings->value( "LastTffFile" ).toString();
    QString pickedFile = dia.getSaveFileName(
                this, tr("Save Transfer Function"),
                defaultPath, tr("Transfer function files (*.tff)"));

    if (!pickedFile.isEmpty())
    {
        QFile file(pickedFile);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            throw std::invalid_argument("Could not open file "
                                        + pickedFile.toStdString());
        }
        else
        {
            QTextStream out(&file);
            const QGradientStops stops =
                    ui->tf0->getEditor()->getGradientStops();
            foreach (QGradientStop s, stops)
            {
                out << s.first << " " << s.second.red() << " " << s.second.green()
                    << " " << s.second.blue() << " " << s.second.alpha() << "\n";
            }
            file.close();
        }
    }
}

/**
 * @brief MainWindow::saveRawTff
 */
void MainWindow::saveRawTff()
{
    QFileDialog dia;
    QString defaultPath = _settings->value( "LastRawTffFile" ).toString();
    QString pickedFile = dia.getSaveFileName(
                this, tr("Save Transfer Function"),
                defaultPath, tr("Transfer function files (*.tff)"));

    if (!pickedFile.isEmpty())
    {
        QFile file(pickedFile);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            throw std::invalid_argument("Could not open file "
                                        + pickedFile.toStdString());
        }
        else
        {
            QTextStream out(&file);
            const QGradientStops stops =
                    ui->tf0->getEditor()->getGradientStops();
            const std::vector<unsigned char> tff = ui->volumeRenderWidget->getRawTransferFunction(stops);
            foreach (unsigned char c, tff)
            {
                out << static_cast<int>(c) << " ";
            }
            file.close();
        }
    }
}


/**
 * @brief MainWindow::saveRawTff
 */
void MainWindow::loadRawTff()
{
    QFileDialog dia;
    QString defaultPath = _settings->value( "LastRawTffFile" ).toString();
    QString pickedFile = dia.getOpenFileName(
                this, tr("Open Transfer Function"),
                defaultPath, tr("Transfer function files (*.tff)"));
    if (!pickedFile.isEmpty())
    {
        qDebug() << "Loading transfer funtion data defined in" << pickedFile;
        std::ifstream tff_file(pickedFile.toStdString(), std::ios::in);
        float value = 0;
        std::vector<unsigned char> values;

        // read lines from file and split on whitespace
        if (tff_file.is_open())
        {
            while (tff_file >> value)
            {
                values.push_back(static_cast<unsigned char>(value));
            }
            tff_file.close();
            ui->volumeRenderWidget->setRawTransferFunction(values);
        }
        else
        {
            qDebug() << "Could not open transfer function file " + pickedFile;
        }
        _settings->setValue( "LastRawTffFile", pickedFile );
    }
}


/**
 * @brief MainWindow::getStatus
 * @return
 */
void MainWindow::setStatusText()
{
    QString status = "No data loaded yet.";
    if (ui->volumeRenderWidget->hasData())
    {
        QVector4D res = ui->volumeRenderWidget->getVolumeResolution();
        status = "File: ";
        status += _fileName;
        status += " | Volume: ";
        status += QString::number(static_cast<int>(res.x()));
        status += "x";
        status += QString::number(static_cast<int>(res.y()));
        status += "x";
        status += QString::number(static_cast<int>(res.z()));
        status += "x";
        status += QString::number(static_cast<int>(res.w()));
        status += " | Frame: ";
        status += QString::number(ui->volumeRenderWidget->size().width());
        status += "x";
        status += QString::number(ui->volumeRenderWidget->size().height());
        status += " ";
    }
    _statusLabel.setText(status);
}

/**
 * @brief MainWindow::setPickedTimestep
 */
void MainWindow::setPickedTimestep(float timestep, QColor color)
{
    unsigned int id = static_cast<unsigned int>(round(timestep *
                                                      ui->volumeRenderWidget->getVolumeResolution().z()));
    QDockWidget* dock = new QDockWidget("Frame " + QString::number(id), this);
//    QLabel *label = new QLabel("Frame " + QString::number(id), dock);
//    label->setStyleSheet("color: black");
//    dock->setTitleBarWidget(label);
    dock->setStyleSheet("background: " + color.name());
    dock->setAllowedAreas(Qt::BottomDockWidgetArea);
    this->addDockWidget(Qt::BottomDockWidgetArea, dock);
    dock->setFloating(true);

    QImage frame = ui->volumeRenderWidget->getSliceImage(id);
    for (int i = 0; i < frame.width(); ++i)
    {
        frame.setPixelColor(i, 0, color);
        frame.setPixelColor(i, frame.height() - 1, color);
    }
    for (int i = 0; i < frame.height(); ++i)
    {
        frame.setPixelColor(0, i, color);
        frame.setPixelColor(frame.width() - 1, i, color);
    }

    QLabel *imgDisplayLabel = new QLabel("");
    imgDisplayLabel->setPixmap(QPixmap::fromImage(frame));
    imgDisplayLabel->adjustSize();
    dock->setWidget(imgDisplayLabel);
}

void MainWindow::updateHistograms()
{
    // gaze histo
    ui->sldScaleGaze->setMaximum(static_cast<int>(ui->volumeRenderWidget->getDataRangeMaxs().at(1)));
    std::array<double, 256> histo = ui->volumeRenderWidget->getHistogram(1u);
    double minVal = *std::min_element(histo.begin() + 0, histo.end());
    double maxVal = *std::max_element(histo.begin() + 0, histo.end());
//    double elements = std::accumulate(histo.begin(), histo.end(), 0.) / histo.size();
    QVector<qreal> qhisto;
    for (auto &a : histo)
    {
//        qreal val = a / elements;
//        qhisto.push_back(val);   // normalize to range [0,1]
        if (ui->chbLog->isChecked())
            qhisto.push_back(log(a - minVal) / log(maxVal));
        else
            qhisto.push_back(a / maxVal);
    }
    ui->tf0->setHistogram(qhisto);

    // flow direction (angle)
    ui->sldScaleAngle->setMaximum(static_cast<int>(ui->volumeRenderWidget->getDataRangeMaxs().at(2)));
    histo = ui->volumeRenderWidget->getHistogram(2u);
    minVal = *std::min_element(histo.begin() + 0, histo.end());
    maxVal = *std::max_element(histo.begin() + 0, histo.end() - 0);
//    elements = std::accumulate(histo.begin(), histo.end(), 0.) / histo.size();
    qhisto.clear();
    for (auto &a : histo)
    {
//        qhisto.push_back(a / maxVal);   // normalize to range [0,1]
        if (ui->chbLog->isChecked())
            qhisto.push_back(log(a - minVal) / log(maxVal));
        else
            qhisto.push_back(a / maxVal);
    }
    ui->tf2->setHistogram(qhisto);

    // flow magnitude
    ui->sldScaleMag->setMaximum(static_cast<int>(ui->volumeRenderWidget->getDataRangeMaxs().at(3)));
    histo = ui->volumeRenderWidget->getHistogram(3u);
    minVal = *std::min_element(histo.begin() + 0, histo.end());
    maxVal = *std::max_element(histo.begin() + 0, histo.end() - 0);
//    elements = std::accumulate(histo.begin(), histo.end(), 0.) / histo.size();
    qhisto.clear();
    for (auto &a : histo)
    {
//        qhisto.push_back(a / maxVal); // / elements * 100.);   // normalize to range [0,1]
        if (ui->chbLog->isChecked())
            qhisto.push_back(log(a - minVal) / log(maxVal));
        else
            qhisto.push_back(a / maxVal);
    }
    ui->tf1->setHistogram(qhisto);
}

/**
 * @brief MainWindow::finishedLoading
 */
void MainWindow::finishedLoading()
{
    _progBar.setValue(100);
    _progBar.hide();
    _timer.stop();
//    qDebug() << "finished.";
    this->setStatusText();
    ui->volumeRenderWidget->setLoadingFinished(true);
    ui->volumeRenderWidget->updateView();

    updateHistograms();
    updateClippingSliders();
}

/**
 * @brief MainWindow::updateClippingSliders
 */
void MainWindow::updateClippingSliders()
{
    ui->sldClipRight->setMaximum(int(ui->volumeRenderWidget->getVolumeResolution().x()));
    ui->sbClipRight->setMaximum(int(ui->volumeRenderWidget->getVolumeResolution().x()));
    ui->sldClipLeft->setMaximum(int(ui->volumeRenderWidget->getVolumeResolution().x()));
    ui->sbClipLeft->setMaximum(int(ui->volumeRenderWidget->getVolumeResolution().x()));
    ui->sldClipFront->setMaximum(int(ui->volumeRenderWidget->getVolumeResolution().z()));
    ui->sbClipFront->setMaximum(int(ui->volumeRenderWidget->getVolumeResolution().z()));
    ui->sldClipBack->setMaximum(int(ui->volumeRenderWidget->getVolumeResolution().z()));
    ui->sbClipBack->setMaximum(int(ui->volumeRenderWidget->getVolumeResolution().z()));
    ui->sldClipBottom->setMaximum(int(ui->volumeRenderWidget->getVolumeResolution().y()));
    ui->sbClipBottom->setMaximum(int(ui->volumeRenderWidget->getVolumeResolution().y()));
    ui->sldClipTop->setMaximum(int(ui->volumeRenderWidget->getVolumeResolution().y()));
    ui->sbClipTop->setMaximum(int(ui->volumeRenderWidget->getVolumeResolution().y()));

    ui->sldClipRight->setValue(ui->sldClipRight->maximum());
    ui->sldClipBack->setValue(ui->sldClipBack->maximum());
    ui->sldClipTop->setValue(ui->sldClipTop->maximum());
}

/**
 * @brief MainWindow::addProgress
 */
void MainWindow::addProgress()
{
    if (_progBar.value() < _progBar.maximum() - 5)
    {
        _progBar.setValue(_progBar.value() + 1);
    }
}


/**
 * @brief MainWindow::loadTff
 */
void MainWindow::loadTff()
{
    QFileDialog dia;
    QString defaultPath = _settings->value( "LastTffFile" ).toString();
    QString pickedFile = dia.getOpenFileName(
                this, tr("Open Transfer Function"),
                defaultPath, tr("Transfer function files (*.tff)"));
    if (!pickedFile.isEmpty())
    {
        readTff(pickedFile);
        _settings->setValue( "LastTffFile", pickedFile );
    }
}


/**
 * @brief MainWindow::openVolumeFile
 */
void MainWindow::openVolumeFile()
{
    QFileDialog dialog;
    QString defaultPath = _settings->value( "LastVolumeFile" ).toString();
    QString pickedFile = dialog.getOpenFileName(
                this, tr("Open Volume Data"), defaultPath,
                tr("Volume data files (*.dat);; All files (*)"));
    if (!pickedFile.isEmpty())
    {
        if (!readVolumeFile(pickedFile))
        {
            QMessageBox msgBox;
            msgBox.setIcon( QMessageBox::Critical );
            msgBox.setText( "Error while trying to create OpenCL memory objects." );
            msgBox.exec();
        }
        else
        {
            _settings->setValue( "LastVolumeFile", pickedFile );
        }
    }
}


/**
 * @brief MainWindow::dragEnterEvent
 * @param ev
 */
void MainWindow::dragEnterEvent(QDragEnterEvent *ev)
{
    if (ev->mimeData()->hasUrls())
    {
        bool valid = false;
        foreach(QUrl url, ev->mimeData()->urls())
        {
            if (!url.fileName().isEmpty())
            {
                QFileInfo finf(url.fileName());
                if (finf.suffix() == "dat")
                    valid = true;
            }
        }
        if (valid)
        {
            ev->acceptProposedAction();
        }
    }
}


/**
 * @brief MainWindow::dropEvent
 * @param ev
 */
void MainWindow::dropEvent(QDropEvent *ev)
{
    foreach(QUrl url, ev->mimeData()->urls())
    {
        QFileInfo finf(url.fileName());
        if (finf.suffix() == "dat")
        {
            // extract path
            QString fileName = url.path();
            qDebug() << "Loading volume data file" << fileName;
            readVolumeFile(fileName);
        }
    }
}


/**
 * @brief MainWindow::chooseBackgroundColor
 */
void MainWindow::chooseBackgroundColor()
{
    QColorDialog dia;
    QColor col = dia.getColor();
    if (col.isValid())
        ui->volumeRenderWidget->setBackgroundColor(col);
}

/**
 * @brief MainWindow::updateBBox
 */
void MainWindow::updateBBox()
{
    if (ui->chbClipping->isChecked())
    {
        QVector3D botLeft(ui->sbClipLeft->value(), ui->sbClipBottom->value(),
                          ui->sbClipFront->value());
        QVector3D topRight(ui->sbClipRight->value(), ui->sbClipTop->value(),
                           ui->sbClipBack->value());
        ui->volumeRenderWidget->setBBox(botLeft, topRight);
    }
}

/**
 * @brief MainWindow::resetBBox
 */
void MainWindow::resetBBox()
{
    ui->sldClipLeft->setValue(0);
    ui->sldClipFront->setValue(0);
    ui->sldClipBottom->setValue(0);
    ui->sldClipRight->setValue(ui->sldClipRight->maximum());
    ui->sldClipBack->setValue(ui->sldClipBack->maximum());
    ui->sldClipTop->setValue(ui->sldClipTop->maximum());
    updateBBox();
}

/**
 * @brief MainWindow::setFilterColor
 * @param value
 */
void MainWindow::setFilterColor(int value)
{
    ui->volumeRenderWidget->setFilter(0, value);
}

void MainWindow::setFilter1(int value)
{
    ui->volumeRenderWidget->setFilter(1, value);
}

void MainWindow::setFilter2(int value)
{
    ui->volumeRenderWidget->setFilter(2, value);
}

void MainWindow::setFilter3(int value)
{
    ui->volumeRenderWidget->setFilter(3, value);
}
