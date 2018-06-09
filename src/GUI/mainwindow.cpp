/*	Copyright (c) 2003-2017 Xsens Technologies B.V. or subsidiaries
   worldwide. All rights reserved.

        Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are met:

        1.	Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

        2.	Redistributions in binary form must reproduce the above
   copyright notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

        3.	Neither the names of the copyright holders nor the names of
   their contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

        THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
   IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE
   LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY OR
   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
   CONTRACT, STRICT LIABILITY OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
   ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
   POSSIBILITY OF SUCH DAMAGE.
*/

//------------------------------------------------------------------------------
// Application
//------------------------------------------------------------------------------
#include "mainwindow.h"
#include "ui_mainwindow.h"

//------------------------------------------------------------------------------
// XDA
//------------------------------------------------------------------------------
#include "connectedmtwdata.h"

//------------------------------------------------------------------------------
// Qt stuff
//------------------------------------------------------------------------------
#include <QDebug>
#include <QFile>
#include <QFileDialog>
#include <QImage>
#include <QMessageBox>

//------------------------------------------------------------------------------
// Misc.
//------------------------------------------------------------------------------
#include <math.h>

//------------------------------------------------------------------------------
// Main window constructor.
//------------------------------------------------------------------------------
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_ui(new Ui::MainWindow),
      m_myWirelessMasterDevice(0), m_nextBatteryRequest(m_measuringMtws.end()) {
  m_ui->setupUi(this);
  setFixedSize(width(), height());

  // m_ui->logoLabel->setPixmap(QPixmap(":xsens_logo.png"));

  m_myXda = new MyXda;

  // Start logging timestamp.
  m_timestamp.start();

  // Make timers to update the time on the GUI
  elapsed = new QTime();
  updateElapsedTimer = new QTimer();
  connect(updateElapsedTimer, SIGNAL(timeout()), this,
          SLOT(updateDisplayTime()));
  updateElapsedTimer->start(100);

  // Connect signals to slots.
  // User --> GUI
  connect(m_ui->enableButton, SIGNAL(clicked()), this,
          SLOT(toggleRadioEnabled()));
  connect(m_ui->startMeasurementButton, SIGNAL(clicked()), this,
          SLOT(toggleMeasurement()));
  connect(m_ui->recordingButton, SIGNAL(clicked()), this,
          SLOT(toggleRecording()));
  connect(m_ui->connectedMtwList, SIGNAL(itemClicked(QListWidgetItem *)), this,
          SLOT(clearConnectedMtwDataLabels()));
  connect(m_ui->clearLogPushButton, SIGNAL(clicked()), this,
          SLOT(clearLogWindow()));
  connect(m_ui->clearLogPushButton2, SIGNAL(clicked()), this,
          SLOT(clearLogWindow()));
  connect(m_ui->connectedMtwList,
          SIGNAL(currentItemChanged(QListWidgetItem *, QListWidgetItem *)),
          this, SLOT(clearConnectedMtwDataLabels()));

  connect(m_ui->selectDestButton, SIGNAL(clicked()), this,
          SLOT(selectDestFolder()));
  connect(m_ui->selectButton, SIGNAL(clicked()), this, SLOT(selectFile()));
  connect(m_ui->convertButton, SIGNAL(clicked()), this, SLOT(convertFile()));
  connect(m_ui->selectFolderButton, SIGNAL(clicked()), this,
          SLOT(selectFolder()));

  connect(m_ui->linkButton, SIGNAL(clicked()), this, SLOT(linkSensors()));
  connect(m_ui->startComboBox, SIGNAL(currentIndexChanged(int)), this,
          SLOT(handleStartComboBox(int)));
  /*connect(m_ui->stopComboBox, SIGNAL(currentIndexChanged(int)), this,
          SLOT(handleStopComboBox(int)));*/
  connect(m_ui->selectTriggerBox, SIGNAL(stateChanged(int)), this,
          SLOT(toggleTrigger()));

  // XDA --> GUI
  connect(m_myXda, SIGNAL(wirelessMasterDetected(XsPortInfo)), this,
          SLOT(handleWirelessMasterDetected(XsPortInfo)));
  connect(m_myXda, SIGNAL(dockedMtwDetected(XsPortInfo)), this,
          SLOT(handleDockedMtwDetected(XsPortInfo)));
  connect(m_myXda, SIGNAL(mtwUndocked(XsPortInfo)), this,
          SLOT(handleMtwUndocked(XsPortInfo)));
  connect(m_myXda, SIGNAL(openPortSuccessful(XsPortInfo const &)), this,
          SLOT(handleOpenPortSuccessful(XsPortInfo const &)));
  connect(m_myXda, SIGNAL(openPortFailed(XsPortInfo const &)), this,
          SLOT(handleOpenPortFailed(XsPortInfo const &)));

  // XsCallback -> GUI
  connect(&m_myWirelessMasterCallback, SIGNAL(mtwWireless(XsDeviceId)), this,
          SLOT(handleMtwWireless(XsDeviceId)));
  connect(&m_myWirelessMasterCallback, SIGNAL(mtwDisconnected(XsDeviceId)),
          this, SLOT(handleMtwDisconnected(XsDeviceId)));
  connect(&m_myWirelessMasterCallback, SIGNAL(measurementStarted(XsDeviceId)),
          this, SLOT(handleMeasurementStarted(XsDeviceId)));
  connect(&m_myWirelessMasterCallback, SIGNAL(measurementStopped(XsDeviceId)),
          this, SLOT(handleMeasurementStopped(XsDeviceId)));
  connect(&m_myWirelessMasterCallback,
          SIGNAL(deviceError(XsDeviceId, XsResultValue)), this,
          SLOT(handleError(XsDeviceId, XsResultValue)));
  connect(&m_myWirelessMasterCallback,
          SIGNAL(waitingForRecordingStart(XsDeviceId)), this,
          SLOT(handleWaitingForRecordingStart(XsDeviceId)));
  connect(&m_myWirelessMasterCallback, SIGNAL(recordingStarted(XsDeviceId)),
          this, SLOT(handleRecordingStarted(XsDeviceId)));
  connect(&m_myWirelessMasterCallback,
          SIGNAL(progressUpdate(XsDeviceId, int, int, QString)), this,
          SLOT(handleProgressUpdate(XsDeviceId, int, int, QString)));

  // Create thread for XDA
  m_xdaThread = new QThread(this);
  m_myXda->moveToThread(m_xdaThread);

  // Create the port scan timer.
  m_portScanTimer = new QTimer(this);
  m_portScanTimer->setInterval(1000);
  connect(m_portScanTimer, SIGNAL(timeout()), m_myXda, SLOT(scanPorts()));

  // Create the batterycheck timer.
  m_batteryLevelRequestTimer = new QTimer(this);
  m_batteryLevelRequestTimer->setInterval(4000);
  connect(m_batteryLevelRequestTimer, SIGNAL(timeout()), this,
          SLOT(requestBatteryLevels()));

  // Start m_state.
  m_state = DETECTING;
  log("Detecting...");
  setWidgetsStates();

  // Start XDA thread
  m_xdaThread->start();

  // Start Timers
  m_portScanTimer->start();
  m_batteryLevelRequestTimer->start();

  // Set standard paths
  const QString documentsFolder =
      QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
          .append("/Awinda Monitor");
  m_ui->destPathEdit->setText(documentsFolder);
  m_ui->folderEdit->setText(documentsFolder);

  m_ui->destNameEdit->setText(findNextFolder(documentsFolder));
  m_ui->measureEdit->setText(findNextFolder(documentsFolder));

  hash = new QHash<QString, QString>;

  readConfig();
}

//------------------------------------------------------------------------------
MainWindow::~MainWindow() {
  // Stop the extra thread.
  m_xdaThread->exit();
  m_xdaThread->wait();

  // Cleanup XDA an GUI
  delete m_myXda;
  delete m_ui;
}

//------------------------------------------------------------------------------
void MainWindow::log(const QString &message) {
  m_ui->logWindow->moveCursor(QTextCursor::End);
  m_ui->logWindow->insertPlainText(
      QString("[%1] %2\n").arg(m_timestamp.elapsed()).arg(message));
}

//------------------------------------------------------------------------------
void MainWindow::err(const QString &message) {
  m_ui->errorWindow->moveCursor(QTextCursor::End);
  m_ui->errorWindow->insertPlainText(
      QString("[%1] %2\n").arg(m_timestamp.elapsed()).arg(message));
}

//------------------------------------------------------------------------------
void MainWindow::readConfig() {
    QFile file("config.csv");
       if (!file.open(QIODevice::ReadOnly)) {
           log("Could not read config");
       }
       while (!file.atEnd()) {
              QByteArray line = file.readLine();
              QList<QByteArray> vals = line.split(',');
              hash->insert(QString(vals.first()),QString(vals.last()));

       }




}


//------------------------------------------------------------------------------
void MainWindow::clearLogWindow() {
    m_ui->logWindow->clear();
    m_ui->errorWindow->clear();
}

//------------------------------------------------------------------------------
// Set the states of widgets (labels, buttons and images) depending on the
// m_state. This is basically a m_state machine with only 'm_state entry code'.
//------------------------------------------------------------------------------
void MainWindow::setWidgetsStates() {

  switch (m_state) {
  case DETECTING: {
    m_ui->stateDiagramLabel->setPixmap(QPixmap(":connecting.png"));
    m_ui->stateLabel->setText("DETECTING");

  } break;

  case CONNECTING: {
    m_ui->enableButton->setEnabled(false);
    m_ui->stateDiagramLabel->setPixmap(QPixmap(":connecting.png"));
    log("Scanning for station.");
    m_ui->recordingButton->setEnabled(false);
    m_ui->stateLabel->setText("CONNECTING");
    m_ui->settingsGroup->setEnabled(true);

    m_ui->selectTriggerBox->setEnabled(false);
  } break;

  case CONNECTED: {
    m_ui->startMeasurementButton->setEnabled(false);
    m_ui->channelCaptionLabel->setEnabled(true);
    m_ui->channelComboBox->setEnabled(true);
    m_ui->enableButton->setEnabled(true);
    m_ui->enableButton->setText(QString("Enable"));
    m_ui->updateRateCaptionLabel->setEnabled(false);
    m_ui->allowedUpdateRatesComboBox->setEnabled(false);
    m_ui->stateDiagramLabel->setPixmap(QPixmap(":connected.png"));
    m_ui->recordingButton->setEnabled(false);
    m_ui->stateLabel->setText("CONNECTED");
    m_ui->selectTriggerBox->setEnabled(true);
    m_ui->settingsGroup->setEnabled(true);
  } break;

  case ENABLED: {
    m_ui->startMeasurementButton->setEnabled(m_ui->connectedMtwList->count() >
                                             0);
    m_ui->startMeasurementButton->setText(QString("Start Measurement"));
    m_ui->enableButton->setText(QString("Disable"));
    m_ui->enableButton->setEnabled(true);
    m_ui->channelCaptionLabel->setEnabled(false);
    m_ui->channelComboBox->setEnabled(false);
    m_ui->updateRateCaptionLabel->setEnabled(true);
    m_ui->allowedUpdateRatesComboBox->setEnabled(true);
    m_ui->stateDiagramLabel->setPixmap(QPixmap(":enabled.png"));
    m_ui->recordingButton->setEnabled(false);
    m_ui->stateLabel->setText("ENABLED");

    m_ui->settingsGroup->setEnabled(true);

    m_ui->selectTriggerBox->setEnabled(false);
  } break;

  case OPERATIONAL: {
    m_ui->startMeasurementButton->setEnabled(m_ui->connectedMtwList->count() >
                                             0);
    m_ui->startMeasurementButton->setText(QString("Start Measurement"));
    m_ui->enableButton->setText(QString("Disable"));
    m_ui->enableButton->setEnabled(true);
    m_ui->channelCaptionLabel->setEnabled(false);
    m_ui->channelComboBox->setEnabled(false);
    m_ui->updateRateCaptionLabel->setEnabled(true);
    m_ui->allowedUpdateRatesComboBox->setEnabled(true);
    m_ui->stateDiagramLabel->setPixmap(QPixmap(":operational.png"));
    m_ui->recordingButton->setEnabled(false);
    m_ui->stateLabel->setText("OPERATIONAL");

    m_ui->settingsGroup->setEnabled(true);
  } break;

  case AWAIT_MEASUREMENT_START:
  case AWAIT_RECORDING_START: {
    m_ui->enableButton->setEnabled(false);
    m_ui->startMeasurementButton->setText(QString("Waiting for start..."));
    m_ui->startMeasurementButton->setEnabled(false);
    m_ui->updateRateCaptionLabel->setEnabled(false);
    m_ui->allowedUpdateRatesComboBox->setEnabled(false);
    m_ui->stateDiagramLabel->setPixmap(QPixmap(":await_measurement_start.png"));
    m_ui->recordingButton->setEnabled(false);
    m_ui->stateLabel->setText("WAIT RECORD");
    m_ui->settingsGroup->setEnabled(false);
  } break;

  case MEASURING: {
    m_ui->enableButton->setEnabled(false);
    m_ui->startMeasurementButton->setText(QString("Stop measuring"));
    m_ui->startMeasurementButton->setEnabled(true);
    m_ui->updateRateCaptionLabel->setEnabled(false);
    m_ui->allowedUpdateRatesComboBox->setEnabled(false);
    m_ui->recordingButton->setText(QString("Start recording"));
    m_ui->recordingButton->setEnabled(true);
    m_ui->recordingButton->setText(QString("Start recording"));
    m_ui->flushingCaptionLabel->setEnabled(false);
    m_ui->flushingProgressBar->setEnabled(false);
    m_ui->flushingProgressBar->setValue(0);
    m_ui->stateDiagramLabel->setPixmap(QPixmap(":measuring.png"));
    m_ui->stateLabel->setText("MEASURING");

    m_ui->settingsGroup->setEnabled(true);
  } break;

  case RECORDING: {
    m_ui->recordingButton->setEnabled(true);
    m_ui->recordingButton->setText(QString("Stop recording"));
    m_ui->startMeasurementButton->setEnabled(false);
    m_ui->stateDiagramLabel->setPixmap(QPixmap(":recording.png"));
    m_ui->stateLabel->setText("RECORDING");
    m_ui->settingsGroup->setEnabled(false);
  } break;

  case FLUSHING: {
    m_ui->recordingButton->setEnabled(false);
    m_ui->recordingButton->setText(QString("Flushing"));
    m_ui->flushingCaptionLabel->setEnabled(true);
    m_ui->flushingProgressBar->setEnabled(true);
    m_ui->stateDiagramLabel->setPixmap(QPixmap(":flushing.png"));
    m_ui->stateLabel->setText("FLUSHING");
  } break;

  default:
    break;
  }
}

/*****************************************************************************
 *****************************************************************************
 ******************         EVENT  HANDLERS                 ******************
 *****************************************************************************
 *****************************************************************************/

void MainWindow::handleWirelessMasterDetected(XsPortInfo port) {
  switch (m_state) {
  case DETECTING: {
    log(QString("Master Detected. Port: %1, ID: %2")
            .arg(port.portName().toStdString().c_str())
            .arg(port.deviceId().toString().toStdString().c_str()));
    m_state = CONNECTING;
    m_myXda->openPort(port);
  } break;

  default:
    break;
  }

  setWidgetsStates();
}

void MainWindow::handleDockedMtwDetected(XsPortInfo port) {
  log(QString("MTw Docked. Port: %1, ID: %2")
          .arg(port.portName().toStdString().c_str())
          .arg(port.deviceId().toString().toStdString().c_str()));

  QString mtwId = port.deviceId().toString().toStdString().c_str();
  if (m_ui->dockedMtwList->findItems(mtwId, Qt::MatchFixedString).isEmpty()) {
    m_ui->dockedMtwList->addItem(mtwId);
  }
  m_ui->dockedMtwListGroupBox->setTitle(
      QString("Docked MTw list (%1):").arg(m_ui->dockedMtwList->count()));
}

void MainWindow::handleMtwUndocked(XsPortInfo port) {
  log(QString("MTw Undocked. Port: %1, ID: %2")
          .arg(port.portName().toStdString().c_str())
          .arg(port.deviceId().toString().toStdString().c_str()));

  QString mtwId = port.deviceId().toString().toStdString().c_str();
  QListWidgetItem *mtw;
  for (int row = 0; row < m_ui->dockedMtwList->count(); row++) {
    mtw = m_ui->dockedMtwList->item(row);

    if (mtw->text() == mtwId) {
      delete m_ui->dockedMtwList->item(row);
      break;
    }
  }
  m_ui->dockedMtwListGroupBox->setTitle(
      QString("Docked MTw list (%1):").arg(m_ui->dockedMtwList->count()));
}

void MainWindow::handleOpenPortSuccessful(XsPortInfo const &port) {
  switch (m_state) {
  case CONNECTING:
    if (port.deviceId().isWirelessMaster()) {
      // Set the label to indicate the ID of the station.
      m_ui->stationIdLabel->setText(
          port.deviceId().toString().toStdString().c_str());
      m_myWirelessMasterDevice = m_myXda->getDevice(port.deviceId());
      assert(m_myWirelessMasterDevice != 0);

      // Attach the callback handler. This causes events to arrive in
      // m_myWirelessMasterCallback.
      m_myWirelessMasterDevice->addCallbackHandler(&m_myWirelessMasterCallback);

      m_state = CONNECTED;
      log(QString("Master Connected. Port: %1, ID: %2")
              .arg(port.portName().toStdString().c_str())
              .arg(port.deviceId().toString().toStdString().c_str()));

      // Be sure to start with radio disabled
      if (m_myWirelessMasterDevice->isRadioEnabled()) {
        setRadioChannel(-1);
      }

      setWidgetsStates();
    }
    break;
  default:
    break;
  }
}

void MainWindow::handleOpenPortFailed(XsPortInfo const &port) {
  if (port.deviceId().isWirelessMaster()) {
    log(QString("Connect to wireless master failed. Port: %1")
            .arg(port.portName().toStdString().c_str()));
  } else {
    log(QString("Connect to device failed. Port: %1")
            .arg(port.portName().toStdString().c_str()));
  }

  switch (m_state) {
  case CONNECTING:
    log(QString("Closing XDA"));
    m_myXda->reset();
    m_state = DETECTING;
    setWidgetsStates();
    break;
  default:
    break;
  }
}

void MainWindow::handleMtwWireless(XsDeviceId device) {
  QString mtwIdStr = QString(device.toString().toStdString().c_str());
  log(QString("MTw Connected. ID: %1").arg(mtwIdStr));

  QList<QListWidgetItem *> foundItems =
      m_ui->connectedMtwList->findItems(mtwIdStr, Qt::MatchFixedString);
  ConnectedMTwData connectedMtwData;
  if (foundItems.isEmpty()) {
    // This is a new MTw, add it.
    connectedMtwData.setRssi(0);

   if (hash->value(mtwIdStr) == NULL)
    hash->insert(mtwIdStr, "");
    QVariant qv;
    qv.setValue(connectedMtwData);
    QListWidgetItem *mtwListItem = new QListWidgetItem();

    mtwListItem->setText(mtwIdStr);
    mtwListItem->setData(Qt::UserRole, qv);
    m_ui->connectedMtwList->addItem(mtwListItem);
    m_ui->connectedMtwList->setCurrentItem(mtwListItem);

    m_ui->connectedMtwListGroupBox->setTitle(
        QString("Connected MTw list (%1):")
            .arg(m_ui->connectedMtwList->count()));
    m_ui->connectedLabel->setText(
        QString("%1").arg(m_ui->connectedMtwList->count()));
  }
  m_ui->startMeasurementButton->setEnabled(m_state == ENABLED &&
                                           m_ui->connectedMtwList->count() > 0);  
}

void MainWindow::handleMtwDisconnected(XsDeviceId device) {
  QString mtwIdStr = QString(device.toString().toStdString().c_str());
  log(QString("MTw Disconnected. ID: %1").arg(mtwIdStr));

  QList<QListWidgetItem *> foundItems =
      m_ui->connectedMtwList->findItems(mtwIdStr, Qt::MatchFixedString);
  if (!foundItems.isEmpty()) {
    // Found --> delete
    delete m_ui->connectedMtwList->item(
        m_ui->connectedMtwList->row(foundItems.first()));

    m_ui->connectedMtwListGroupBox->setTitle(
        QString("Connected MTw list (%1):")
            .arg(m_ui->connectedMtwList->count()));
    m_ui->connectedLabel->setText(
        QString("%1").arg(m_ui->connectedMtwList->count()));
    m_ui->startMeasurementButton->setEnabled(
        m_state == ENABLED && m_ui->connectedMtwList->count() > 0);
  }
}

void MainWindow::handleMeasurementStarted(XsDeviceId device) {
  log(QString("Measurement Started. ID: %1")
          .arg(device.toString().toStdString().c_str()));

  if (m_myXda->getDevice(device)->deviceId() ==
      m_myWirelessMasterDevice->deviceId()) {
    switch (m_state) {
    case AWAIT_MEASUREMENT_START: {
      // Get the MTws that are measuring and attach callback handlers
      clearMeasuringMtws();
      std::set<XsDeviceId> deviceIds =
          m_myWirelessMasterCallback.getConnectedMTws();
      for (std::set<XsDeviceId>::const_iterator i = deviceIds.begin();
           i != deviceIds.end(); ++i) {
        XsDevicePtr mtw = m_myXda->getDevice(*i);
        QSharedPointer<MyMtwCallback> callback =
            QSharedPointer<MyMtwCallback>(new MyMtwCallback);

        // connect signals
        connect(callback.data(),
                SIGNAL(dataAvailable(XsDeviceId, XsDataPacket)), this,
                SLOT(handleDataAvailable(XsDeviceId, XsDataPacket)));
        connect(callback.data(), SIGNAL(batteryLevelChanged(XsDeviceId, int)),
                this, SLOT(handleBatteryLevelChanged(XsDeviceId, int)));

        assert(mtw != 0);
        mtw->clearCallbackHandlers();
        mtw->addCallbackHandler(callback.data());
        m_measuringMtws.insert(std::make_pair(mtw, callback));
      }

      m_state = MEASURING;
      setWidgetsStates();
    } break;

    case RECORDING:
    case FLUSHING: {
      log(QString("Recording Finished. ID: %1")
              .arg(device.toString().toStdString().c_str()));

      m_myWirelessMasterDevice->closeLogFile();
      // Convert
      QString destFolder = m_ui->folderEdit->text();
      QString destFile = m_ui->measureEdit->text();

      // create new XDA to convert
      MyXda *cXda = new MyXda;
      QString logFilename= QString("%1/%2/logfile.mtb").arg(destFolder).arg(destFile);
      printf("test");
      if (cXda->loadFile(logFilename.toStdString().c_str())) {
        cXda->convert(destFolder.toStdString().c_str(), destFile.toStdString().c_str(),hash);
        printf("test2");
        log(QString("Conversion of log file complete"));
      }else{
        err(QString("Conversion of log file not complete\r\n Please press 'start recording' before activating a trigger"));
      }


      delete cXda;
      QString documentsFolder = m_ui->folderEdit->text();

      //Set next folder
      m_ui->destNameEdit->setText(findNextFolder(documentsFolder));
      m_ui->measureEdit->setText(findNextFolder(documentsFolder));

      m_state = MEASURING;

      setWidgetsStates();

      //TODO AUTONOMOUS
      if (m_ui->autonomousBox->isEnabled() && m_state != FLUSHING){
          toggleRecording();
      }

      break;
    }
    default:
      break;
    }
  }
}

void MainWindow::handleMeasurementStopped(XsDeviceId device) {
  log(QString("Measurement Stopped. ID: %1")
          .arg(device.toString().toStdString().c_str()));
  if (m_myXda->getDevice(device) == m_myWirelessMasterDevice) {
    clearMeasuringMtws();
    m_state = OPERATIONAL;
    setWidgetsStates();
  }
}

void MainWindow::handleError(XsDeviceId device, XsResultValue errorVal) {
  QString msg(XsResultValue_toString(errorVal));
  log(QString("ERROR. ID: %1, Msg: %2")
          .arg(device.toString().toStdString().c_str())
          .arg(msg));
  err(QString("A sensor disconnected, check sensors"));
  switch (m_state) {
  case AWAIT_MEASUREMENT_START:
    m_state = ENABLED;
    setWidgetsStates();
    break;
  default:
    break;
  }
}

void MainWindow::handleDataAvailable(XsDeviceId deviceId, XsDataPacket packet) {
  QString mtwIdStr = QString(deviceId.toString().toStdString().c_str());
  QList<QListWidgetItem *> foundItems =
      m_ui->connectedMtwList->findItems(mtwIdStr, Qt::MatchFixedString);

  if (foundItems.isEmpty()) {
    log(QString(
            "Obsolete data received of an MTw %1 that's no longer in the list.")
            .arg(mtwIdStr));
    return;
  }

  if (!packet.containsSdiData()) {
    log(QString("Packet received of an MTw %1 not containing data.")
            .arg(mtwIdStr));
    return;
  }

  // Getting SDI data.
  XsSdiData sdiData = packet.sdiData();

  QVariant qv = foundItems.first()->data(Qt::UserRole);
  ConnectedMTwData mtwData = qv.value<ConnectedMTwData>();

  mtwData.setRssi(packet.rssi());

  if (packet.containsOrientation()) {
    // Getting Euler angles.
    XsEuler oriEuler = packet.orientationEuler();


    mtwData.setOrientation(oriEuler);
  }

  // -- Determine effective update rate percentage --

  // Determine the number of frames over which the SDI data in this
  // packet was determined.
  int frameSkips;

  if (packet.frameRange().last() > packet.frameRange().first()) {
    frameSkips = packet.frameRange().last() - packet.frameRange().first() - 1;
  } else {
    // Rollover (internal framecounter is unsigned 16 bits integer)
    frameSkips =
        65535 + packet.frameRange().last() - packet.frameRange().first() - 1;
  }

  mtwData.frameSkipsList()->append(frameSkips);
  mtwData.setSumFrameSkips(mtwData.sumFrameSkips() + frameSkips);
  mtwData.setEffectiveUpdateRate(
      100 * (1 - (float)mtwData.sumFrameSkips() /
                     (float)(mtwData.frameSkipsList()->count() +
                             mtwData.sumFrameSkips())));

  while (mtwData.frameSkipsList()->count() + mtwData.sumFrameSkips() > 99 &&
         mtwData.frameSkipsList()->count() > 0) {
    mtwData.setSumFrameSkips(mtwData.sumFrameSkips() -
                             mtwData.frameSkipsList()->first());
    mtwData.frameSkipsList()->removeFirst();
  }

  // Store data.
  qv.setValue(mtwData);
  foundItems.first()->setData(Qt::UserRole, qv);

  if (m_ui->connectedMtwList->currentItem() == foundItems.first()) {
    // Display data when MTw selected.
    displayMtwData(&mtwData);
  }
}

void MainWindow::handleBatteryLevelChanged(XsDeviceId deviceId,
                                           int batteryLevel) {
  QString mtwIdStr = QString(deviceId.toString().toStdString().c_str());
  QList<QListWidgetItem *> foundItems =
      m_ui->connectedMtwList->findItems(mtwIdStr, Qt::MatchFixedString);

  if (foundItems.isEmpty()) {
    log(QString(
            "Obsolete data received of an MTw %1 that's no longer in the list.")
            .arg(mtwIdStr));
    err(QString(
                "Obsolete data received of an MTw %1 that's no longer in the list.")
                .arg(mtwIdStr));
    return;
  }

  QVariant qv = foundItems.first()->data(Qt::UserRole);
  ConnectedMTwData mtwData = qv.value<ConnectedMTwData>();

  mtwData.setBatteryLevel(batteryLevel);
  if (batteryLevel < 15){
      foundItems.first()->setForeground(Qt::red);
  }else if (batteryLevel < 30){
      foundItems.first()->setForeground(QColor(255,140,0));
  }

  // Store data.
  qv.setValue(mtwData);
  foundItems.first()->setData(Qt::UserRole, qv);
}

void MainWindow::handleWaitingForRecordingStart(XsDeviceId) {
  log(QString("Waiting for recording start. ID: %1")
          .arg(m_myWirelessMasterDevice->deviceId()
                   .toString()
                   .toStdString()
                   .c_str()));
  m_state = AWAIT_RECORDING_START;
  setWidgetsStates();
}

void MainWindow::handleRecordingStarted(XsDeviceId) {
  log(QString("Recording started"));
  elapsed->start();
  m_state = RECORDING;
  setWidgetsStates();
  if (m_state == AWAIT_RECORDING_START) {
    log(QString("Waiting for recording start. ID: %1")
            .arg(m_myWirelessMasterDevice->deviceId()
                     .toString()
                     .toStdString()
                     .c_str()));
    // Keep the recording data saved


  }
}

void MainWindow::handleProgressUpdate(XsDeviceId, int current, int total,
                                      QString identifier) {
  if (m_state == FLUSHING && identifier == "Flushing") {
    if (m_ui->allowedUpdateRatesComboBox->currentIndex() == 0) {
      // Nothing to flush when at the highest update rate.
      m_myWirelessMasterDevice->abortFlushing();
      log(QString("Flushing aborted. ID: %1")
              .arg(m_myWirelessMasterDevice->deviceId()
                       .toString()
                       .toStdString()
                       .c_str()));
    }

    if (total != 0 && m_ui->allowedUpdateRatesComboBox->currentIndex() != 0) {
      // Only do this when there is still data to be flushed
      // and not the highest update rate was selected.
      m_ui->flushingProgressBar->setMaximum(total);
      m_ui->flushingProgressBar->setValue(current);
    }
  }
}

/*****************************************************************************
 *****************************************************************************
 ******************            USER ACTIONS                 ******************
 *****************************************************************************
 *****************************************************************************/
void MainWindow::toggleRadioEnabled() {

  // Empty setting array, if filled sync settings will be changed
  // if left empty the settings will be cleared
  XsSyncSettingArray settings;
  // If trigger enabled put sync settings in the array to use in the master
  if (m_ui->selectTriggerBox->isChecked()) {
    XsSyncSetting in1;
    XsSyncSetting in2;

    XsSyncLine input =
        m_ui->inputComboBox->currentIndex() == 0 ? XSL_In1 : XSL_In2;
    XsSyncPolarity startFunc =
        m_ui->startComboBox->currentIndex() == 0 ? XSP_RisingEdge : XSP_FallingEdge;
    XsSyncPolarity stopFunc =
        m_ui->startComboBox->currentIndex() == 1 ? XSP_RisingEdge : XSP_FallingEdge;

    in1 = XsSyncSetting(input, XSF_StartRecordingIn, startFunc, 100, 0, 0, 0,
                        0, 0);

    in2 = XsSyncSetting(input, XSF_StopRecordingIn, stopFunc, 100, 0, 0, 0,
                        0, 0);

    // Add settings to setting list
    settings.push_back(in1);
    settings.push_back(in2);
  }
  // If selected put sync settings in master device, else settings will be
  // cleared
  m_myWirelessMasterDevice->setSyncSettings(settings);
  setRadioChannel(
      m_state == CONNECTED ? m_ui->channelComboBox->currentText().toInt() : -1);
}

void MainWindow::setRadioChannel(int channel) {
  assert(m_myWirelessMasterDevice != 0);
  if (m_myWirelessMasterDevice->enableRadio(channel)) {
    if (channel != -1) {
      log(QString("Master Enabled. ID: %1, Channel: %2")
              .arg(m_myWirelessMasterDevice->deviceId()
                       .toString()
                       .toStdString()
                       .c_str())
              .arg(channel));

      // Supported update rates and maximum available from xda
      XsIntArray supportedRates =
          m_myWirelessMasterDevice->supportedUpdateRates();
      int maxUpdateRate = m_myWirelessMasterDevice->maximumUpdateRate();

      // -- Put the allowed update rates in the combobox for the user to choose
      // from --
      m_ui->allowedUpdateRatesComboBox->clear();
      for (XsIntArray::const_iterator i = supportedRates.begin();
           i != supportedRates.end() && *i <= maxUpdateRate; ++i) {
        // This is an allowed update rate, so add it to the list.
        m_ui->allowedUpdateRatesComboBox->addItem(QString("%1").arg(*i));
      }

      // Select the current update rate of the station.
      int updateRateIndex = m_ui->allowedUpdateRatesComboBox->findText(
          QString("%1").arg(m_myWirelessMasterDevice->updateRate()));
      m_ui->allowedUpdateRatesComboBox->setCurrentIndex(updateRateIndex);

      m_state = ENABLED;

      // Set a default update rate of 75 (if available) when we set the radio
      // channel
      updateRateIndex =
          m_ui->allowedUpdateRatesComboBox->findText(QString("%1").arg(75));

      if (updateRateIndex != -1) {
        m_ui->allowedUpdateRatesComboBox->setCurrentIndex(updateRateIndex);
      }
    } else {
      log(QString("Master Disabled. ID: %1")
              .arg(m_myWirelessMasterDevice->deviceId()
                       .toString()
                       .toStdString()
                       .c_str()));
      m_ui->connectedMtwList->clear();
      m_state = CONNECTED;
    }
    setWidgetsStates();
  } else {
    if (channel != -1) {
      log(QString("Failed to enable wireless master. ID: %1, Channel: %2")
              .arg(m_myWirelessMasterDevice->deviceId()
                       .toString()
                       .toStdString()
                       .c_str())
              .arg(channel));
    } else {
      log(QString("Failed to disable wireless master. ID: %1")
              .arg(m_myWirelessMasterDevice->deviceId()
                       .toString()
                       .toStdString()
                       .c_str()));
    }
  }
}

void MainWindow::toggleMeasurement() {
  assert(m_myWirelessMasterDevice != 0);

  switch (m_state) {
  case ENABLED:
  case OPERATIONAL: {
    // First set the update rate
    int desiredUpdateRate =
        m_ui->allowedUpdateRatesComboBox->currentText().toInt();
    if (desiredUpdateRate != -1 &&
        desiredUpdateRate != m_myWirelessMasterDevice->updateRate()) {
      if (m_myWirelessMasterDevice->setUpdateRate(desiredUpdateRate)) {
        // TODO
        /* Check if update rate realy changed */
        if (m_myXda->checkUpdateRate(desiredUpdateRate)) {
          log("All update rates correct");
        } else {
          log(QString("Update rate not correct. ID: %1, Rate: %2")
                  .arg(m_myWirelessMasterDevice->deviceId()
                           .toString()
                           .toStdString()
                           .c_str())
                  .arg(desiredUpdateRate));
          err(QString("Update rate not correct. ID: %1, Rate: %2")
                  .arg(m_myWirelessMasterDevice->deviceId()
                           .toString()
                           .toStdString()
                           .c_str())
                  .arg(desiredUpdateRate));
        }
      } else {
        log(QString("Failed to set update rate. ID: %1, Rate: %2")
                .arg(m_myWirelessMasterDevice->deviceId()
                         .toString()
                         .toStdString()
                         .c_str())
                .arg(desiredUpdateRate));
        err(QString("Failed to set update rate. ID: %1, Rate: %2")
                .arg(m_myWirelessMasterDevice->deviceId()
                         .toString()
                         .toStdString()
                         .c_str())
                .arg(desiredUpdateRate));
      }
    } else {
      if (m_myXda->checkUpdateRate(desiredUpdateRate)) {
         log("All update rates correct");
      } else {
        log(QString("Update rate not correct. ID: %1, Rate: %2")
                .arg(m_myWirelessMasterDevice->deviceId()
                         .toString()
                         .toStdString()
                         .c_str())
                .arg(desiredUpdateRate));
        err(QString("Update rate not correct. ID: %1, Rate: %2")
                .arg(m_myWirelessMasterDevice->deviceId()
                         .toString()
                         .toStdString()
                         .c_str())
                .arg(desiredUpdateRate));
      }
    }

    if (m_ui->allowedUpdateRatesComboBox->currentIndex() == 0) {
      QMessageBox msgBox;
      msgBox.setText("Note: at the highest update rate\nrecording will be at "
                     "effective update rate.");
      msgBox.exec();
    }
    const States bkpState = m_state;
    // Set the state to wait measurement start
    m_state = AWAIT_MEASUREMENT_START;
    if (m_myWirelessMasterDevice->gotoMeasurement()) {
      log(QString("Waiting for measurement start. ID: %1")
              .arg(m_myWirelessMasterDevice->deviceId()
                       .toString()
                       .toStdString()
                       .c_str()));
    } else {
      // GotoMeasurement failed, restore the previous state
      m_state = bkpState;
      log(QString("Failed to start measurement. ID: %1")
              .arg(m_myWirelessMasterDevice->deviceId()
                       .toString()
                       .toStdString()
                       .c_str()));
    }
  } break;

  case MEASURING: {
    if (m_myWirelessMasterDevice->gotoConfig()) {
      log(QString("Stopping measurement. ID: %1")
              .arg(m_myWirelessMasterDevice->deviceId()
                       .toString()
                       .toStdString()
                       .c_str()));
    } else {
      log(QString("Failed to stop measurement. ID: %1")
              .arg(m_myWirelessMasterDevice->deviceId()
                       .toString()
                       .toStdString()
                       .c_str()));
    }
  } break;
  default:
    break;
  }
  setWidgetsStates();
}

void MainWindow::toggleRecording() {
  switch (m_state) {
  case MEASURING: {
    // -- Start the recording --
      QString foldername = m_ui->folderEdit->text();
      QString measurename = m_ui->measureEdit->text();
      QString folder = QString("%1/%2").arg(foldername).arg(measurename);
      QDir dir(folder);
      if (!dir.exists()) {
        dir.mkpath(".");
      }
      QString logFilename = QString("%1/logfile.mtb").arg(folder);
    if (m_myWirelessMasterDevice->createLogFile(
            logFilename.toStdString().c_str()) == XRV_OK) {
      if (m_myWirelessMasterDevice->startRecording()) {
          //Set state to recording as it else wont go to recording state while waiting for a trigger
          if (m_ui->selectTriggerBox->isChecked()){
              m_state = RECORDING;
          }
      } else {
        log(QString("Failed to start re cording. ID: %1")
                .arg(m_myWirelessMasterDevice->deviceId()
                         .toString()
                         .toStdString()
                         .c_str()));
      }
    } else {
      log(QString("Failed to create log file: %1").arg(logFilename));
    }
  } break;

  case RECORDING: {
    // -- Stop the recording --

    // Write to CSV

   /*   QString foldername = m_ui->folderEdit->text();
      QString measurename = m_ui->measureEdit->text();
    m_myXda->convert(foldername.toStdString().c_str(), measurename.toStdString().c_str(), hash);
*/
    // TODO convert to file
    m_state = FLUSHING;
    m_myWirelessMasterDevice->stopRecording();
    //m_myWirelessMasterDevice->setOptions(XsOption::XSO_None, XsOption::XSO_RetainRecordingData);

    log(QString("Stopping recording. ID: %1")
            .arg(m_myWirelessMasterDevice->deviceId()
                     .toString()
                     .toStdString()
                     .c_str()));


    // Reset the time
    elapsed = new QTime();

  } break;

  default:
    break;
  }

  setWidgetsStates();
}

void MainWindow::toggleTrigger() {
  log("toggle enable");
  if (!m_ui->selectTriggerBox->isChecked()) {
    //TODO AUTONOMOUS
    m_ui->autonomousBox->setEnabled(false);
    m_ui->intputLabel->setEnabled(false);
    m_ui->intputTriggerLabel->setEnabled(false);
    m_ui->intput2TriggerLabel->setEnabled(false);
    m_ui->startComboBox->setEnabled(false);
    m_ui->stopLabel->setEnabled(false);
    m_ui->inputComboBox->setEnabled(false);
  } else {
    //TODO AUTONOMOUS
    m_ui->autonomousBox->setEnabled(true);
    m_ui->intputLabel->setEnabled(true);
    m_ui->intputTriggerLabel->setEnabled(true);
    m_ui->intput2TriggerLabel->setEnabled(true);
    m_ui->startComboBox->setEnabled(true);
    m_ui->stopLabel->setEnabled(true);
    m_ui->inputComboBox->setEnabled(true);
  }
}

void MainWindow::clearConnectedMtwDataLabels() {
  // Clean the data labels.
  m_ui->batteryLevelLabel->setText("-");
  m_ui->rssiLabel->setText("-");
  m_ui->effUpdateRateLabel->setText("-");
  m_ui->rollLabel->setText("-");
  m_ui->pitchLabel->setText("-");
  m_ui->yawLabel->setText("-");
}

void MainWindow::displayMtwData(ConnectedMTwData *mtwData) {
  // Last known battery level.
  m_ui->batteryLevelLabel->setText(
      QString("%1 [%]").arg(mtwData->batteryLevel()));

  // RSSI (received signals strenght indicator) of this packet.
  m_ui->rssiLabel->setText(QString("%1 [dBm]").arg(mtwData->rssi()));

  if (mtwData->containsOrientation()) {
    // Display Euler angles.
    m_ui->rollLabel->setText(
        QString("%1 [deg]").arg(mtwData->orientation().x(), 5, 'f', 2));
    m_ui->pitchLabel->setText(
        QString("%1 [deg]").arg(mtwData->orientation().y(), 5, 'f', 2));
    m_ui->yawLabel->setText(
        QString("%1 [deg]").arg(mtwData->orientation().z(), 5, 'f', 2));
  }

  // Display effective update rate.
  m_ui->effUpdateRateLabel->setText(
      QString("%1 [%]").arg(mtwData->effectiveUpdateRate()));
}

void MainWindow::clearMeasuringMtws() {
  QMutexLocker locker(&m_mutex);
  // Detach callback handlers
  for (std::map<XsDevicePtr, QSharedPointer<MyMtwCallback>>::iterator i =
           m_measuringMtws.begin();
       i != m_measuringMtws.end(); ++i) {
    i->first->clearCallbackHandlers();
  }
  m_measuringMtws.clear();
  m_nextBatteryRequest = m_measuringMtws.end();
}

void MainWindow::requestBatteryLevels() {
  QMutexLocker locker(&m_mutex);

  // It is impossible to request battery status for all MTWs at once. So cycle
  // between them

  if (m_nextBatteryRequest == m_measuringMtws.end()) {
    m_nextBatteryRequest = m_measuringMtws.begin();
  }
  if (m_nextBatteryRequest != m_measuringMtws.end()) {
    m_nextBatteryRequest->first->requestBatteryLevel();
    ++m_nextBatteryRequest;
  }
}

/*****************************************************************************
 *****************************************************************************
 ******************         FILE CONVERSION                 ******************
 *****************************************************************************
 *****************************************************************************/

void MainWindow::selectFile() {
  QString fileName = QFileDialog::getOpenFileName(
      this, tr("Open mtb file"), "", tr("MT binary(*.mtb);;All Files (*)"));
  if (!fileName.isEmpty()) {
    m_ui->logSourceEdit->setText(fileName);
  }
}

void MainWindow::selectFolder() {
  const QString documentsFolder =
      QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
          .append("/Awinda Monitor");
  QString dir = QFileDialog::getExistingDirectory(
      this, tr("Open Directory"), documentsFolder,
      QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
  if (!dir.isEmpty()) {
    m_ui->folderEdit->setText(dir);
    m_ui->measureEdit->setText(findNextFolder(dir));
  }
}

//------------------------------------------------------------------------------
void MainWindow::selectDestFolder() {
  const QString documentsFolder =
      QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
  QString dir = QFileDialog::getExistingDirectory(
      this, tr("Open Directory"), documentsFolder,
      QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
  if (!dir.isEmpty()) {
    m_ui->destPathEdit->setText(dir);

    m_ui->destNameEdit->setText(findNextFolder(dir));
  }
}

void MainWindow::linkSensors() {

  sensorScreen = new LinkSensorsScreen(hash);

  if (sensorScreen->exec() == QDialog::Accepted) {
    return;
  }
}

QString MainWindow::findNextFolder(QString folderName) {
  int record_int = 1;
  QString folder =
      QString("%1/%2 %3").arg(folderName).arg("Recording").arg(record_int);
  QDir dir(QString("%1").arg(folder));
  while (dir.exists()) {
    record_int++;
    folder =
        QString("%1/%2 %3").arg(folderName).arg("Recording").arg(record_int);
    dir = QDir(QString("%1").arg(folder));
  }
  QString name = QString("%1 %2").arg("Recording").arg(record_int);
  return name;
}

//------------------------------------------------------------------------------
void MainWindow::convertFile() {
  QString logFilename = m_ui->logSourceEdit->text();
  QString destFolder = m_ui->destPathEdit->text();
  QString destFile = m_ui->destNameEdit->text();

  // create new XDA to convert
  MyXda *cXda = new MyXda;
  if (cXda->loadFile(logFilename.toStdString().c_str())) {
    /* Check if folder exists, else popup overwrite */
    QString folder = QString("%1/%2").arg(destFolder).arg(destFile);
    QDir dir(QString("%1").arg(folder));
    if (dir.exists()) {
      QMessageBox::StandardButton reply;
      reply = QMessageBox::question(this, "Folder exists",
                                    "Folder already exists, overwrite?",
                                    QMessageBox::Yes | QMessageBox::No);
      if (reply == QMessageBox::Yes) {
        cXda->convert(destFolder.toStdString().c_str(),
                                   destFile.toStdString().c_str(),hash);
      } else {
        log(QString("Canceled the conversion"));
      }
    } else {
        cXda->convert(destFolder.toStdString().c_str(),
                                 destFile.toStdString().c_str(),hash);
    }
  }
  delete cXda;
}

/*
void MainWindow::handleStopComboBox(int test) {
  int start = m_ui->startComboBox->currentIndex();
  //int stop = m_ui->stopComboBox->currentIndex();
  if (start == stop) {
    m_ui->startComboBox->setCurrentIndex(stop == 0 ? 1 : 0);
  }
}
*/


void MainWindow::handleStartComboBox(int test) {
  int start = m_ui->startComboBox->currentIndex();
  //int stop = m_ui->stopComboBox->currentIndex();
    //m_ui->stopComboBox->setCurrentIndex(start == 0 ? 1 : 0);
  m_ui->stopLabel->setText(start == 1 ? " Rising edge" : " Falling edge");
}

//------------------------------------------------------------------------------
void MainWindow::updateDisplayTime() {
  if (elapsed->isValid()) {

    QString formattedTime;
    qint64 msecs = elapsed->elapsed();
    int hours = msecs / (1000 * 60 * 60);
    int minutes = (msecs - (hours * 1000 * 60 * 60)) / (1000 * 60);
    int seconds =
        (msecs - (minutes * 1000 * 60) - (hours * 1000 * 60 * 60)) / 1000;
    int milliseconds = msecs - (seconds * 1000) - (minutes * 1000 * 60) -
                       (hours * 1000 * 60 * 60);

    formattedTime.append(
        QString("%1").arg(hours, 2, 10, QLatin1Char('0')) + " : " +
        QString("%1").arg(minutes, 2, 10, QLatin1Char('0')) + " : " +
        QString("%1").arg(seconds, 2, 10, QLatin1Char('0')) + " : " +
        QString("%1").arg(milliseconds, 2, 10, QLatin1Char('0')));

    m_ui->elapsedTimeLabel->setText(formattedTime);
  }
}
