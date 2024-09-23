#ifndef RPI3POLICY_H_
#define RPI3POLICY_H_

#include "Policy.h"
#include "ApplicationMonitor.h"
#include "utilization.h"
#include "MappingConsts.h"
#include "smartgauge.hpp"
#include <map>
#include <iostream>
#include <fstream>

#define RPI3_NUM_OF_CPU_CORES 4

class RPI3Policy: public Policy{
  public:
    typedef struct{
      //Throughput info
      float thr; //last measure of throughput
      float thr_ref; //target throughput
      int mapping; //Actually not used! there is only an homogeneous cluster
      bool mt;
    }appl_state;

    RPI3Policy();
    RPI3Policy(std::string csvSysFilename, std::string csvApplFilename);
    ~RPI3Policy();
    void run(int cycle);
    
  private:
    monitor_t* appl_monitor;
    Utilization* utilization;
    SmartGauge* smartGauge;

    std::ofstream* csvSysFile;
    std::string csvSysFilename;
    std::ofstream* csvApplFile;
    std::string csvApplFilename;

    std::map<pid_t, RPI3Policy::appl_state> appl_info;
};

#endif
