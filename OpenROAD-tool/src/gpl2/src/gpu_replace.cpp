///////////////////////////////////////////////////////////////////////////////
// BSD 3-Clause License
//
// Copyright (c) 2018-2023, The Regents of the University of California
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice, this
//   list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
//
// * Neither the name of the copyright holder nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
///////////////////////////////////////////////////////////////////////////////

#include "gpl2/GpuReplace.h"

// for test only
#include <iostream>

#include "initialPlace.h"
#include "nesterovPlace.h"
#include "placerBase.h"
#include "gpuRouteBase.h"
#include "gpuTimingBase.h"
#include "util.h"
#include "utl/Logger.h"
#include "rsz/Resizer.hh"
#include "odb/db.h"

namespace gpl2 {

using namespace std;
using utl::GPL2;

GpuReplace::GpuReplace()
    : db_(nullptr),
      rs_(nullptr),
      fr_(nullptr),
      log_(nullptr),
      pbc_(nullptr),
      rb_(nullptr),
      tb_(nullptr),
      ip_(nullptr),
      np_(nullptr),
      initialPlaceMaxIter_(20),
      initialPlaceMinDiffLength_(1500),
      initialPlaceMaxSolverIter_(100),
      initialPlaceMaxFanout_(200),
      initialPlaceNetWeightScale_(800),
      nesterovPlaceMaxIter_(5000),
      binGridCntX_(0),
      binGridCntY_(0),
      overflow_(0.1),
      density_(1.0),
      initDensityPenalityFactor_(0.00008),
      initWireLengthCoef_(0.25),
      minPhiCoef_(0.95),
      maxPhiCoef_(1.05),
      referenceHpwl_(446000000),
      routabilityCheckOverflow_(0.20),
      routabilityMaxDensity_(0.99),
      routabilityTargetRcMetric_(1.25),
      routabilityInflationRatioCoef_(2.5),
      routabilityMaxInflationRatio_(2.5),
      routabilityRcK1_(1.0),
      routabilityRcK2_(1.0),
      routabilityRcK3_(0.0),
      routabilityRcK4_(0.0),
      routabilityMaxBloatIter_(1),
      routabilityMaxInflationIter_(4),
      timingNetWeightMax_(1.9),
      timingDrivenMode_(true),
      routabilityDrivenMode_(true),
      uniformTargetDensityMode_(false),
      skipIoMode_(false),
      dataflowFlag_(false),
      DREAMPlaceFlag_(false),
      clusterConstraintFlag_(false),
      padLeft_(0),
      padRight_(0) {  };

GpuReplace::~GpuReplace()
{
}

void GpuReplace::init(sta::dbNetwork* network, 
                      odb::dbDatabase* odb,
                      rsz::Resizer* resizer,
                      grt::GlobalRouter* router,
                      utl::Logger* logger)
{
  network_ = network;
  db_ = odb;
  rs_ = resizer;
  fr_ = router;
  log_ = logger;
}

void GpuReplace::reset()
{
  ip_.reset();
  np_.reset();

  pbc_.reset();
  pbVec_.clear();
  pbVec_.shrink_to_fit();

  tb_.reset();
  rb_.reset();

  initialPlaceMaxIter_ = 20;
  initialPlaceMinDiffLength_ = 1500;
  initialPlaceMaxSolverIter_ = 100;
  initialPlaceMaxFanout_ = 200;
  initialPlaceNetWeightScale_ = 800;

  nesterovPlaceMaxIter_ = 5000;
  binGridCntX_ = binGridCntY_ = 0;
  overflow_ = 0.1;
  density_ = 1.0;
  initDensityPenalityFactor_ = 0.00008;
  initWireLengthCoef_ = 0.25;
  minPhiCoef_ = 0.95;
  maxPhiCoef_ = 1.05;
  referenceHpwl_ = 446000000;

  routabilityCheckOverflow_ = 0.20;
  routabilityMaxDensity_ = 0.99;
  routabilityTargetRcMetric_ = 1.25;
  routabilityInflationRatioCoef_ = 2.5;
  routabilityMaxInflationRatio_ = 2.5;
  routabilityRcK1_ = routabilityRcK2_ = 1.0;
  routabilityRcK3_ = routabilityRcK4_ = 0.0;
  routabilityMaxBloatIter_ = 1;
  routabilityMaxInflationIter_ = 4;

  timingDrivenMode_ = true;
  routabilityDrivenMode_ = true;
  uniformTargetDensityMode_ = false;
  skipIoMode_ = false;

  padLeft_ = padRight_ = 0;

  timingNetWeightOverflows_.clear();
  timingNetWeightOverflows_.shrink_to_fit();
  timingNetWeightMax_ = 1.9;
}

// TODO: dummy function now
void GpuReplace::doIncrementalPlace()
{
  std::cout << "GpuReplace :: doIncrementalPlace" << std::endl;
}


/*
// TODO: dummy function now
void GpuReplace::doInitialPlace()
{
  std::cout << "GpuReplace :: doInitialPlace" << std::endl;
  // If the netlist is loaded into the placer database,
  // we need to create the PlacerBaseCommon
  if (pbc_ == nullptr) {
    PlacerBaseVars pbVars;
    pbVars.padLeft = padLeft_;
    pbVars.padRight = padRight_;
    pbVars.skipIoMode = skipIoMode_;
    std::cout << "padLeft  = " << padLeft_ << "  "
              << "padRight = " << padRight_ << "  "
              << "skipIoMode = " << skipIoMode_ << std::endl;

    pbc_ = std::make_shared<PlacerBaseCommon>(db_, pbVars, log_);
    //std::cout << "hpwl = " << pbc_->hpwl() << std::endl;
    //pbc_->unlockAll();
    //pbc_->SyncInstLocH2D();
    //pbc_->SyncInstLocD2H();
    //pbc_->printInstInfo();
    NesterovBaseVars nbVars;
    nbVars.targetDensity = density_;
    nbVars.useUniformTargetDensity = uniformTargetDensityMode_;
    pbVec_.push_back(std::make_shared<PlacerBase>(nbVars, db_, pbc_, log_));
    // Each power domain is represented by a group
    for (auto pd : db_->getChip()->getBlock()->getPowerDomains()) {
      if (pd->getGroup()) {
        pbVec_.push_back(
            std::make_shared<PlacerBase>(nbVars, db_, pbc_, log_, pd->getGroup()));
      }
    }
    std::cout << "Finish creating placer database" << std::endl;
    //total_placeable_insts_ = 0;
    //for (const auto& pb : pbVec_) {
    //  total_placeable_insts_ += pb->placeInsts().size();
    //}
  }

  InitialPlaceVars ipVars;
  ipVars.maxIter = initialPlaceMaxIter_;
  ipVars.minDiffLength = initialPlaceMinDiffLength_;
  ipVars.maxSolverIter = initialPlaceMaxSolverIter_;
  ipVars.maxFanout = initialPlaceMaxFanout_;
  ipVars.netWeightScale = initialPlaceNetWeightScale_;
  std::unique_ptr<InitialPlace> ip(new InitialPlace(ipVars, pbc_, pbVec_, log_));
  ip_ = std::move(ip);
  auto start_timestamp_global = std::chrono::high_resolution_clock::now();
  ip_->doBicgstabPlace();
  auto end_timestamp_global = std::chrono::high_resolution_clock::now();
  double total_global_time
      = std::chrono::duration_cast<std::chrono::nanoseconds>(
            end_timestamp_global - start_timestamp_global)
            .count();
  total_global_time *= 1e-9;
  std::cout << "the initial placement runtime is " << total_global_time << std::endl;
}
*/

void GpuReplace::doInitialPlace()
{
  std::cout << "GpuReplace :: doInitialPlace" << std::endl;
  PlacerBaseVars pbVars;
  pbVars.padLeft = padLeft_;
  pbVars.padRight = padRight_;
  pbVars.skipIoMode = skipIoMode_;

  NesterovBaseVars nbVars;
  nbVars.targetDensity = density_;
  nbVars.useUniformTargetDensity = uniformTargetDensityMode_;

  InitialPlaceVars ipVars;
  ipVars.maxIter = initialPlaceMaxIter_;
  ipVars.minDiffLength = initialPlaceMinDiffLength_;
  ipVars.maxSolverIter = initialPlaceMaxSolverIter_;
  ipVars.maxFanout = initialPlaceMaxFanout_;
  ipVars.netWeightScale = initialPlaceNetWeightScale_;
  

  NesterovPlaceVars npVars;
  npVars.minPhiCoef = minPhiCoef_;
  npVars.maxPhiCoef = maxPhiCoef_;
  npVars.referenceHpwl = referenceHpwl_;
  npVars.routabilityCheckOverflow = routabilityCheckOverflow_;
  npVars.initDensityPenalty = initDensityPenalityFactor_;
  npVars.initWireLengthCoef = initWireLengthCoef_;
  npVars.targetOverflow = overflow_;
  npVars.maxNesterovIter = nesterovPlaceMaxIter_;
  npVars.timingDrivenMode = timingDrivenMode_;
  npVars.routabilityDrivenMode = routabilityDrivenMode_;

  std::unique_ptr<InitialPlace> ip(new InitialPlace(ipVars, haloWidth_,
          numHops_, dataflowFlag_, network_, db_, log_));
  ip_ = std::move(ip);
  ip_->setPlacerBaseVars(pbVars);
  ip_->setNesterovBaseVars(nbVars);
  ip_->setNesterovPlaceVars(npVars);
  ip_->doInitialPlace();
  // for test
  //ip_->doClusterNesterovPlace(); 
}


// TODO: dummy function now
bool GpuReplace::initNesterovPlace()
{
  std::cout << "GpuReplace :: initNesterovPlace " << std::endl;
  if (pbc_ == nullptr) {
    PlacerBaseVars pbVars;
    pbVars.padLeft = padLeft_;
    pbVars.padRight = padRight_;
    pbVars.skipIoMode = skipIoMode_;

    NesterovBaseVars nbVars;
    nbVars.targetDensity = density_;
    nbVars.useUniformTargetDensity = uniformTargetDensityMode_;
    if (binGridCntX_ != 0 && binGridCntY_ != 0) {
      nbVars.isSetBinCnt = 1;
      nbVars.binCntX = binGridCntX_;
      nbVars.binCntY = binGridCntY_;
    }
    pbc_ = std::make_shared<PlacerBaseCommon>(network_, db_, pbVars, log_, 
                                              haloWidth_,
                                              virtualIter_,
                                              0,
                                              1.0,
                                              false,
                                              false,
                                              datapathFlag_,
                                              clusterConstraintFlag_);
    pbVec_.push_back(std::make_shared<PlacerBase>(nbVars, db_, pbc_, log_));
    // Each power domain is represented by a group
    for (auto pd : db_->getChip()->getBlock()->getPowerDomains()) {
      if (pd->getGroup()) {
        pbVec_.push_back(
            std::make_shared<PlacerBase>(nbVars, db_, pbc_, log_, pd->getGroup()));
      }
    }

    // check how many place instances 
    totalPlaceableInsts_ = 0;
    for (const auto& pb : pbVec_) {
      totalPlaceableInsts_ += pb->numPlaceInsts();
    }

    if (totalPlaceableInsts_ == 0) {
      log_->warn(GPL2, 136, "No placeable instances - skipping placement.");
      return false;
    }
    std::cout << "Finish creating placer database" << std::endl;
  }


  if (!np_) {
    NesterovPlaceVars npVars;

    npVars.minPhiCoef = minPhiCoef_;
    npVars.maxPhiCoef = maxPhiCoef_;
    npVars.referenceHpwl = referenceHpwl_;
    npVars.routabilityCheckOverflow = routabilityCheckOverflow_;
    npVars.initDensityPenalty = initDensityPenalityFactor_;
    npVars.initWireLengthCoef = initWireLengthCoef_;
    npVars.targetOverflow = overflow_;
    npVars.maxNesterovIter = nesterovPlaceMaxIter_;
    npVars.timingDrivenMode = timingDrivenMode_;
    npVars.routabilityDrivenMode = routabilityDrivenMode_;

    for (const auto& pb : pbVec_) {
      pb->setNpVars(npVars);
    }

    // TODO:  we do not have timing-driven or routability-driven mode
    rb_ = nullptr;
    tb_ = nullptr;

    std::unique_ptr<NesterovPlace> np(
        new NesterovPlace(npVars, DREAMPlaceFlag_, pbc_, pbVec_, rb_, tb_, log_));
    np_ = std::move(np);
  }

  std::cout << "Finish initNesterovPlace()" << std::endl;
  return true;
}

// TODO: dummy function now
int GpuReplace::doNesterovPlace(int start_iter)
{
  std::cout << "GpuReplace :: doNesterovPlace" << std::endl;
  auto start_timestamp_global = std::chrono::high_resolution_clock::now();
  
  if (!initNesterovPlace()) {
    return 0;
  }
  int val =  np_->doNesterovPlace(start_iter);

  auto end_timestamp_global = std::chrono::high_resolution_clock::now();
  double total_global_time
      = std::chrono::duration_cast<std::chrono::nanoseconds>(
            end_timestamp_global - start_timestamp_global)
            .count();
  total_global_time *= 1e-9;
  std::cout << "the nesterov placement runtime is " << total_global_time << std::endl;

  auto block = db_->getChip()->getBlock();
  for (odb::dbBTerm* bterm : block->getBTerms()) {
    odb::dbProperty::destroyProperties(bterm);
  }

  // Fix the macro locations
  fixedMacroLoc();

  return val;
}


Snapper::Snapper() : inst_(nullptr)
{
}

Snapper::Snapper(odb::dbInst* inst) : inst_(inst)
{
}

void Snapper::snapMacro()
{
  const odb::Point snap_origin = computeSnapOrigin();
  inst_->setOrigin(snap_origin.x(), snap_origin.y());
}

odb::Point Snapper::computeSnapOrigin()
{
  SnapParameters x, y;

  std::set<odb::dbTechLayer*> snap_layers;

  odb::dbMaster* master = inst_->getMaster();
  for (odb::dbMTerm* mterm : master->getMTerms()) {
    if (mterm->getSigType() != odb::dbSigType::SIGNAL) {
      continue;
    }

    for (odb::dbMPin* mpin : mterm->getMPins()) {
      for (odb::dbBox* box : mpin->getGeometry()) {
        odb::dbTechLayer* layer = box->getTechLayer();

        if (snap_layers.find(layer) != snap_layers.end()) {
          continue;
        }

        snap_layers.insert(layer);

        if (layer->getDirection() == odb::dbTechLayerDir::HORIZONTAL) {
          y = computeSnapParameters(layer, box, false);
        } else {
          x = computeSnapParameters(layer, box, true);
        }
      }
    }
  }

  // The distance between the pins and the lower-left corner of the master of
  // a macro instance may not be a multiple of the track-grid, in these cases,
  // we need to compensate a small offset.
  const int mterm_offset_x
      = x.pitch != 0
            ? x.lower_left_to_first_pin
                  - std::floor(x.lower_left_to_first_pin / x.pitch) * x.pitch
            : 0;
  const int mterm_offset_y
      = y.pitch != 0
            ? y.lower_left_to_first_pin
                  - std::floor(y.lower_left_to_first_pin / y.pitch) * y.pitch
            : 0;

  int pin_offset_x = x.pin_width / 2 + mterm_offset_x;
  int pin_offset_y = y.pin_width / 2 + mterm_offset_y;

  odb::dbOrientType orientation = inst_->getOrient();

  if (orientation == odb::dbOrientType::MX) {
    pin_offset_y = -pin_offset_y;
  } else if (orientation == odb::dbOrientType::MY) {
    pin_offset_x = -pin_offset_x;
  } else if (orientation == odb::dbOrientType::R180) {
    pin_offset_x = -pin_offset_x;
    pin_offset_y = -pin_offset_y;
  }

  // This may NOT be the lower-left corner.
  int origin_x;
  int origin_y;

  inst_->getOrigin(origin_x, origin_y);

  // Compute trackgrid alignment only if there are pins in the grid's direction.
  // Note that we first align the macro origin with the track grid and then, we
  // compensate the necessary offset.
  if (x.pin_width != 0) {
    origin_x = std::round(origin_x / x.pitch) * x.pitch + x.offset;
    origin_x -= pin_offset_x;
  }
  if (y.pin_width != 0) {
    origin_y = std::round(origin_y / y.pitch) * y.pitch + y.offset;
    origin_y -= pin_offset_y;
  }

  const int manufacturing_grid
      = inst_->getDb()->getTech()->getManufacturingGrid();

  origin_x = std::round(origin_x / manufacturing_grid) * manufacturing_grid;
  origin_y = std::round(origin_y / manufacturing_grid) * manufacturing_grid;

  const odb::Point snap_origin(origin_x, origin_y);

  return snap_origin;
}

SnapParameters Snapper::computeSnapParameters(odb::dbTechLayer* layer,
                                              odb::dbBox* box,
                                              const bool vertical_layer)
{
  SnapParameters params;

  odb::dbBlock* block = inst_->getBlock();
  odb::dbTrackGrid* track_grid = block->findTrackGrid(layer);

  if (track_grid) {
    std::vector<int> coordinate_grid;
    getTrackGrid(track_grid, coordinate_grid, vertical_layer);

    params.offset = coordinate_grid[0];
    params.pitch = coordinate_grid[1] - coordinate_grid[0];
  } else {
    params.pitch = getPitch(layer, vertical_layer);
    params.offset = getOffset(layer, vertical_layer);
  }

  params.pin_width = getPinWidth(box, vertical_layer);
  params.lower_left_to_first_pin
      = getPinToLowerLeftDistance(box, vertical_layer);

  return params;
}

int Snapper::getPitch(odb::dbTechLayer* layer, const bool vertical_layer)
{
  int pitch = 0;
  if (vertical_layer) {
    pitch = layer->getPitchX();
  } else {
    pitch = layer->getPitchY();
  }
  return pitch;
}

int Snapper::getOffset(odb::dbTechLayer* layer, const bool vertical_layer)
{
  int offset = 0;
  if (vertical_layer) {
    offset = layer->getOffsetX();
  } else {
    offset = layer->getOffsetY();
  }
  return offset;
}

int Snapper::getPinWidth(odb::dbBox* box, const bool vertical_layer)
{
  int pin_width = 0;
  if (vertical_layer) {
    pin_width = box->getDX();
  } else {
    pin_width = box->getDY();
  }
  return pin_width;
}

void Snapper::getTrackGrid(odb::dbTrackGrid* track_grid,
                           std::vector<int>& coordinate_grid,
                           const bool vertical_layer)
{
  if (vertical_layer) {
    track_grid->getGridX(coordinate_grid);
  } else {
    track_grid->getGridY(coordinate_grid);
  }
}

int Snapper::getPinToLowerLeftDistance(odb::dbBox* box, bool vertical_layer)
{
  int pin_to_origin = 0;
  if (vertical_layer) {
    pin_to_origin = box->getBox().xMin();
  } else {
    pin_to_origin = box->getBox().yMin();
  }
  return pin_to_origin;
}



void GpuReplace::fixedMacroLoc()
{
  Snapper snapper; 
  odb::dbBlock* block = db_->getChip()->getBlock();
  auto insts = block->getInsts();
  for (odb::dbInst* inst : insts) {
    auto type = inst->getMaster()->getType();
    if (!type.isCore() && !type.isBlock()) {
      continue;
    }
    if (inst->getMaster()->isBlock()) {
      snapper.setMacro(inst);
      snapper.snapMacro();
      inst->setPlacementStatus(odb::dbPlacementStatus::LOCKED);
    }
  }
}

void GpuReplace::setInitialPlaceMaxIter(int iter)
{
  initialPlaceMaxIter_ = iter;
}

void GpuReplace::setInitialPlaceMinDiffLength(int length)
{
  initialPlaceMinDiffLength_ = length;
}

void GpuReplace::setInitialPlaceMaxSolverIter(int iter)
{
  initialPlaceMaxSolverIter_ = iter;
}

void GpuReplace::setInitialPlaceMaxFanout(int fanout)
{
  initialPlaceMaxFanout_ = fanout;
}

void GpuReplace::setInitialPlaceNetWeightScale(float scale)
{
  initialPlaceNetWeightScale_ = scale;
}

// TODO:  Enable np_ for setMaxIters
void GpuReplace::setNesterovPlaceMaxIter(int iter)
{
  nesterovPlaceMaxIter_ = iter;
  // TODO: we need to enable this
  //if (np_) {
  //  np_->setMaxIters(iter);
  //}
}

void GpuReplace::setBinGridCnt(int binGridCntX, int binGridCntY)
{
  binGridCntX_ = binGridCntX;
  binGridCntY_ = binGridCntY;
}

// TODO: Enable np_ for setTargetOverflow
void GpuReplace::setTargetOverflow(float overflow)
{
  overflow_ = overflow;
  // TODO: we need to enable this
  //if (np_) {
  //  np_->setTargetOverflow(overflow);
  //}
}

void GpuReplace::setTargetDensity(float density)
{
  density_ = density;
}

void GpuReplace::setUniformTargetDensityMode(bool mode)
{
  uniformTargetDensityMode_ = mode;
}

// TODO: update to be compatible with multiple target densities
float GpuReplace::getUniformTargetDensity()
{
  return 0.0;
  // TODO: update to be compatible with multiple target densities
  //initNesterovPlace();
  //return nbVec_[0]->uniformTargetDensity();
}

void GpuReplace::setInitDensityPenalityFactor(float penaltyFactor)
{
  initDensityPenalityFactor_ = penaltyFactor;
}

void GpuReplace::setInitWireLengthCoef(float coef)
{
  initWireLengthCoef_ = coef;
}

void GpuReplace::setMinPhiCoef(float minPhiCoef)
{
  minPhiCoef_ = minPhiCoef;
}

void GpuReplace::setMaxPhiCoef(float maxPhiCoef)
{
  maxPhiCoef_ = maxPhiCoef;
}

void GpuReplace::setReferenceHpwl(float refHpwl)
{
  referenceHpwl_ = refHpwl;
}

void GpuReplace::setSkipIoMode(bool mode)
{
  skipIoMode_ = mode;
}

void GpuReplace::setTimingDrivenMode(bool mode)
{
  timingDrivenMode_ = mode;
}

void GpuReplace::setRoutabilityDrivenMode(bool mode)
{
  routabilityDrivenMode_ = mode;
}

void GpuReplace::setRoutabilityCheckOverflow(float overflow)
{
  routabilityCheckOverflow_ = overflow;
}

void GpuReplace::setRoutabilityMaxDensity(float density)
{
  routabilityMaxDensity_ = density;
}

void GpuReplace::setRoutabilityMaxBloatIter(int iter)
{
  routabilityMaxBloatIter_ = iter;
}

void GpuReplace::setRoutabilityMaxInflationIter(int iter)
{
  routabilityMaxInflationIter_ = iter;
}

void GpuReplace::setRoutabilityTargetRcMetric(float rc)
{
  routabilityTargetRcMetric_ = rc;
}

void GpuReplace::setRoutabilityInflationRatioCoef(float coef)
{
  routabilityInflationRatioCoef_ = coef;
}

void GpuReplace::setRoutabilityMaxInflationRatio(float ratio)
{
  routabilityMaxInflationRatio_ = ratio;
}

void GpuReplace::setRoutabilityRcCoefficients(float k1,
                                           float k2,
                                           float k3,
                                           float k4)
{
  routabilityRcK1_ = k1;
  routabilityRcK2_ = k2;
  routabilityRcK3_ = k3;
  routabilityRcK4_ = k4;
}

void GpuReplace::setPadLeft(int pad)
{
  padLeft_ = pad;
}

void GpuReplace::setPadRight(int pad)
{
  padRight_ = pad;
}

void GpuReplace::setHaloWidth(float haloWidth)
{
  haloWidth_ = haloWidth;
}

void GpuReplace::setVirtualIter(int iter)
{
  virtualIter_ = iter;
}

void GpuReplace::setNumHops(int numHops)
{
  numHops_ = numHops;
}

void GpuReplace::setDataflowFlag(bool flag)
{
  dataflowFlag_ = flag;
}

void GpuReplace::setDatapathFlag(bool flag) 
{
  datapathFlag_ = flag;
}

void GpuReplace::setClusterConstraintFlag(bool flag)
{
  clusterConstraintFlag_ = flag;
}

void GpuReplace::setDREAMPlaceFlag(bool flag)
{
  DREAMPlaceFlag_ = flag;
}

void GpuReplace::addTimingNetWeightOverflow(int overflow)
{
  timingNetWeightOverflows_.push_back(overflow);
}

void GpuReplace::setTimingNetWeightMax(float max)
{
  timingNetWeightMax_ = max;
}

}  // namespace gpl2
