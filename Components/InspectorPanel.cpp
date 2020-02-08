//
//    InspectorPanel.cpp: Dockable inspector panel
//    Copyright (C) 2019 Gonzalo José Carracedo Carballal
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU Lesser General Public License as
//    published by the Free Software Foundation, either version 3 of the
//    License, or (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful, but
//    WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU Lesser General Public License for more details.
//
//    You should have received a copy of the GNU Lesser General Public
//    License along with this program.  If not, see
//    <http://www.gnu.org/licenses/>
//
#include "InspectorPanel.h"
#include "ui_InspectorPanel.h"

// FIXME!! THESE INCLUDES ARE AWFUL. CREATE SIGDIGGER UTILITY CLASS
#include "DataSaverUI.h"
#include <Waveform.h>
#include "SourcePanel.h"

using namespace SigDigger;

///////////////////////////// Inspector panel cnfig ////////////////////////////
#define STRINGFY(x) #x
#define STORE(field) obj.set(STRINGFY(field), this->field)
#define LOAD(field) this->field = conf.get(STRINGFY(field), this->field)

void
InspectorPanelConfig::deserialize(Suscan::Object const &conf)
{
  LOAD(inspectorClass);
  LOAD(precise);
  LOAD(palette);
  LOAD(paletteOffset);
}

Suscan::Object &&
InspectorPanelConfig::serialize(void)
{
  Suscan::Object obj(SUSCAN_OBJECT_TYPE_OBJECT);

  obj.setClass("InspectorPanelConfig");

  STORE(inspectorClass);
  STORE(precise);
  STORE(palette);
  STORE(paletteOffset);

  return this->persist(obj);
}

////////////////////////// Inspector panel widget //////////////////////////////
Suscan::Serializable *
InspectorPanel::allocConfig(void)
{
  return this->panelConfig = new InspectorPanelConfig();
}

void
InspectorPanel::applyConfig(void)
{
  this->setInspectorClass(this->panelConfig->inspectorClass);
  this->setPrecise(this->panelConfig->precise);
  this->timeWindow->setPalette(this->panelConfig->palette);
  this->timeWindow->setPaletteOffset(this->panelConfig->paletteOffset);

  // Track changes now
  connect(
        this->timeWindow,
        SIGNAL(configChanged(void)),
        this,
        SLOT(onTimeWindowConfigChanged(void)));
}

void
InspectorPanel::connectAll(void)
{
  connect(
        this->ui->bandwidthSpin,
        SIGNAL(valueChanged(int)),
        this,
        SLOT(onBandwidthChanged(int)));

  connect(
        this->ui->openInspectorButton,
        SIGNAL(clicked(bool)),
        this,
        SLOT(onOpenInspector(void)));

  connect(
        this->ui->preciseCheck,
        SIGNAL(stateChanged(int)),
        this,
        SLOT(onPreciseChanged(void)));

  connect(
        this->ui->captureButton,
        SIGNAL(pressed(void)),
        this,
        SLOT(onPressHold(void)));

  connect(
        this->ui->captureButton,
        SIGNAL(released(void)),
        this,
        SLOT(onReleaseHold(void)));

  connect(
        this->ui->autoSquelchButton,
        SIGNAL(pressed(void)),
        this,
        SLOT(onPressAutoSquelch(void)));

  connect(
        this->ui->autoSquelchButton,
        SIGNAL(released(void)),
        this,
        SLOT(onReleaseAutoSquelch(void)));

  connect(
        this->ui->autoSquelchButton,
        SIGNAL(toggled(bool)),
        this,
        SLOT(onToggleAutoSquelch(void)));
}

void
InspectorPanel::refreshUi(void)
{
  switch (this->state) {
    case DETACHED:
      this->ui->openInspectorButton->setEnabled(false);
      this->ui->bandwidthSpin->setEnabled(false);
      break;

    case ATTACHED:
      this->ui->openInspectorButton->setEnabled(true);
      this->ui->bandwidthSpin->setEnabled(true);
      break;
  }
}

void
InspectorPanel::postLoadInit(void)
{
  this->timeWindow = new TimeWindow(this);
}

void
InspectorPanel::setDemodFrequency(qint64 freq)
{
  this->ui->inspectorChannelLabel->setText(QString::number(freq) + " Hz");
  this->demodFreq = freq;
}

void
InspectorPanel::setColorConfig(ColorConfig const &config)
{
  this->timeWindow->setColorConfig(config);
}

void
InspectorPanel::setBandwidthLimits(unsigned int min, unsigned int max)
{
  this->ui->bandwidthSpin->setMinimum(static_cast<int>(min));
  this->ui->bandwidthSpin->setMaximum(static_cast<int>(max));
}

void
InspectorPanel::setBandwidth(unsigned int freq)
{
  this->ui->bandwidthSpin->setValue(static_cast<int>(freq));
}

void
InspectorPanel::setPrecise(bool precise)
{
  this->ui->preciseCheck->setChecked(precise);
}

void
InspectorPanel::setState(enum State state)
{
  if (this->state != state) {
    this->state = state;
    this->refreshUi();
  }
}

enum InspectorPanel::State
InspectorPanel::getState(void) const
{
  return this->state;
}

bool
InspectorPanel::getPrecise(void) const
{
  return this->ui->preciseCheck->isChecked();
}

void
InspectorPanel::setInspectorClass(std::string const &cls)
{
  if (cls == "psk")
    this->ui->pskRadio->setChecked(true);
  else if (cls == "fsk")
    this->ui->pskRadio->setChecked(true);
  else if (cls == "ask")
    this->ui->pskRadio->setChecked(true);
}

std::string
InspectorPanel::getInspectorClass(void) const
{
  if (this->ui->pskRadio->isChecked())
    return "psk";
  else if (this->ui->fskRadio->isChecked())
    return "fsk";
  else if (this->ui->askRadio->isChecked())
    return "ask";

  return "";
}

unsigned int
InspectorPanel::getBandwidth(void) const
{
  return static_cast<unsigned int>(this->ui->bandwidthSpin->value());
}

void
InspectorPanel::resetRawInspector(qreal fs)
{
  this->timeWindowFs = fs;
  this->uiRefreshSamples =
      SIGDIGGER_DEFAULT_UPDATEUI_PERIOD_MS * 1e-3 / this->timeWindowFs;
  this->maxSamples = this->ui->maxMemSpin->value() * (1 << 20) / sizeof(SUCOMPLEX);
  this->ui->hangTimeSpin->setMinimum(std::ceil(1e3 / fs));
  this->data.resize(0);
  this->ui->sampleRateLabel->setText(
        SourcePanel::formatSampleRate(static_cast<unsigned>(fs)));
  this->ui->durationLabel->setText(Waveform::formatLabel(0, "s"));
  this->ui->memoryLabel->setText(DataSaverUI::formatCaptureSize(0));
}

void
InspectorPanel::refreshCaptureInfo(void)
{
  this->ui->durationLabel->setText(
        Waveform::formatLabel(this->data.size() / this->timeWindowFs, "s"));
  this->ui->memoryLabel->setText(
        DataSaverUI::formatCaptureSize(this->data.size() * sizeof(SUCOMPLEX)));
}

SUFLOAT
InspectorPanel::getHistoryPower(void) const
{
  SUFLOAT accum = 0;

  for (auto p : this->powerHistory)
    accum += p;

  return SU_POWER_DB(
        this->timeWindowFs * accum
        / (this->powerHistory.size()));
}

void
InspectorPanel::transferHistory(void)
{
  // Insert older samples
  this->data.insert(
        this->data.end(),
        this->powerHistory.begin() + this->powerHistoryPtr,
        this->powerHistory.end());

  // Insert newer samples
  this->data.insert(
        this->data.end(),
        this->powerHistory.begin(),
        this->powerHistory.begin() + this->powerHistoryPtr);
}

void
InspectorPanel::feedRawInspector(const SUCOMPLEX *data, size_t size)
{
  this->totalSamples += size;
  bool refreshUi =
      this->totalSamples >= this->uiRefreshSamples;

  if (refreshUi)
    this->totalSamples -= this->uiRefreshSamples;

  if (this->ui->captureButton->isDown()) {
    // Manual capture
    this->data.insert(this->data.end(), data, data + size);
    if (refreshUi)
      this->refreshCaptureInfo();
  } else if (this->autoSquelch) {
    SUFLOAT level;

    // Power measure.
    if (this->ui->autoSquelchButton->isDown()) {
      SUFLOAT sum = this->powerAccum;
      SUFLOAT y = 0;
      SUFLOAT t;
      SUFLOAT err = this->powerError;


      for (size_t i = 0; i < size; ++i) {
        y = SU_C_REAL(data[i] * SU_C_CONJ(data[i])) - err;
        t = sum + y;
        err = (t - sum) - y;
        sum = t;
      }

      this->powerAccum = sum;
      this->powerError = err;
      this->powerSamples += size;

      level = SU_POWER_DB(this->timeWindowFs * this->powerAccum / (this->powerSamples));
    } else {
      for (size_t i = 0; i < size; ++i) {
        this->powerHistory[this->powerHistoryPtr++]
            = SU_C_REAL(data[i] * SU_C_CONJ(data[i]));
        if (this->powerHistoryPtr == this->powerHistory.size())
          this->powerHistoryPtr = 0;
      }

      level = this->getHistoryPower();
    }

    // NOT TRIGGERED: Sensing the channel
    if (!this->autoSquelchTriggered) {
      if (refreshUi)
        this->ui->powerLabel->setText(
            QString::number(.1 * std::floor(10 * level)) + " dB");
      if (this->ui->autoSquelchButton->isDown()) {
        // SQUELCH BUTTON DOWN: Measure noise
        this->squelch = level + SIGDIGGER_DEFAULT_SQUELCH_THRESHOLD;
        this->hangLevel = level + .5 * SIGDIGGER_DEFAULT_SQUELCH_THRESHOLD;
        if (refreshUi)
          this->ui->squelchLevelLabel->setText(
              QString::number(.1 * std::floor(10 * this->squelch)) + " dB");
      } else {
        // SQUELCH BUTTON UP: Wait for signal
        if (level >= this->squelch) {
          this->transferHistory();
          this->autoSquelchTriggered = true;
          this->ui->autoSquelchButton->setText("Triggered!");
        }
      }
    }

    // TRIGGERED: Recording the channel
    if (this->autoSquelchTriggered) {
      this->data.insert(this->data.end(), data, data + size);
      this->refreshCaptureInfo();
      if (this->data.size() > this->measureSamples) {
        if (level < this->hangLevel || this->data.size() > this->maxSamples) { // Hang!
          this->cancelAutoSquelch();
          this->openTimeWindow();
        }
      }
    }
  }
}

void
InspectorPanel::openTimeWindow(void)
{
  this->timeWindow->setData(this->data, this->timeWindowFs);
  this->timeWindow->setCenterFreq(this->demodFreq);
  this->timeWindow->show();
  this->timeWindow->raise();
  this->timeWindow->activateWindow();
  this->timeWindow->setWindowState(Qt::WindowState::WindowActive);
  this->timeWindow->onFit();

  this->ui->sampleRateLabel->setText(
        SourcePanel::formatSampleRate(static_cast<unsigned>(0)));
  this->ui->durationLabel->setText(Waveform::formatLabel(0, "s"));
  this->ui->memoryLabel->setText(DataSaverUI::formatCaptureSize(0));
}

void
InspectorPanel::enableAutoSquelch(void)
{
  // Enable autoSquelch
  this->autoSquelch = true;
  this->powerAccum = this->powerError = this->powerSamples = 0;
  this->powerHistoryPtr = 0;
  this->ui->squelchLevelLabel->setEnabled(true);
  this->ui->powerLabel->setEnabled(true);
  this->ui->captureButton->setEnabled(false);
  this->ui->autoSquelchButton->setChecked(this->autoSquelch);
  this->ui->hangTimeSpin->setEnabled(false);
  this->ui->maxMemSpin->setEnabled(false);
  emit startRawCapture();
}

void
InspectorPanel::cancelAutoSquelch(void)
{
  // Cancel autoSquelch
  this->autoSquelch = false;
  this->autoSquelchTriggered = false;
  this->ui->squelchLevelLabel->setText("N/A");
  this->ui->powerLabel->setText("N/A");
  this->ui->squelchLevelLabel->setEnabled(false);
  this->ui->powerLabel->setEnabled(false);
  this->ui->captureButton->setEnabled(true);
  this->ui->autoSquelchButton->setChecked(this->autoSquelch);
  this->ui->hangTimeSpin->setEnabled(true);
  this->ui->maxMemSpin->setEnabled(true);
  this->ui->autoSquelchButton->setText("Autosquelch");
  emit stopRawCapture();
}

InspectorPanel::InspectorPanel(QWidget *parent) :
  PersistentWidget(parent),
  ui(new Ui::InspectorPanel)
{
  ui->setupUi(this);

  this->assertConfig();

  this->connectAll();
}

InspectorPanel::~InspectorPanel()
{
  delete ui;
}

/////////////////////////////////// Slots /////////////////////////////////////
void
InspectorPanel::onOpenInspector(void)
{
  this->panelConfig->inspectorClass = this->getInspectorClass();
  emit requestOpenInspector(QString::fromStdString(this->getInspectorClass()));
}

void
InspectorPanel::onBandwidthChanged(int bw)
{
  /* this->mainWindow->mainSpectrum->setHiLowCutFrequencies(-bw / 2, bw / 2); */
  emit bandwidthChanged(bw);
}

void
InspectorPanel::onPreciseChanged(void)
{
  this->panelConfig->precise = this->ui->preciseCheck->isChecked();
}

void
InspectorPanel::onPressAutoSquelch(void)
{
  if (!this->autoSquelch) {
    this->enableAutoSquelch();
    this->ui->autoSquelchButton->setText("Measuring...");
  } else {
    this->cancelAutoSquelch();
    if (this->data.size() > 0)
      this->openTimeWindow();
  }
}

void
InspectorPanel::onReleaseAutoSquelch(void)
{
  if (this->autoSquelch) {
    if (this->powerSamples == 0) {
      cancelAutoSquelch();
    } else {
      this->measureSamples =
          1e-3 * this->ui->hangTimeSpin->value() * this->timeWindowFs;
      this->powerHistory.resize(this->measureSamples);
      std::fill(this->powerHistory.begin(), this->powerHistory.end(), 0);
      this->powerAccum /= this->powerSamples;
      this->powerSamples = 1;
      this->ui->autoSquelchButton->setText("Waiting...");
    }
  }
}

void
InspectorPanel::onToggleAutoSquelch(void)
{
  this->ui->autoSquelchButton->setChecked(this->autoSquelch);
}

void
InspectorPanel::onPressHold(void)
{
  emit startRawCapture();
}

void
InspectorPanel::onReleaseHold(void)
{
  emit stopRawCapture();

  if (this->data.size() > 0)
    this->openTimeWindow();
}

void
InspectorPanel::onTimeWindowConfigChanged(void)
{
  this->panelConfig->palette = this->timeWindow->getPalette();
  this->panelConfig->paletteOffset = this->timeWindow->getPaletteOffset();
}
