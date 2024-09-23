#ifndef TEGRAPROFPOLICY_H_
#define TEGRAPROFPOLICY_H_

#include "Policy.h"
#include "ApplicationMonitor.h"
#include "getNode_Tegra.h"
#include "MappingConsts.h"
#include "utilization.h"
#include <map>
#include <iostream>
#include <fstream>

class TegraProfPolicy: public Policy{
  public:
    typedef struct{
      //Throughput info
      float thr; //last measure of throughput
      float thr_ref; //target throughput
      int mapping; //0=LITTLE, 1=big, 2=GPU
      bool mt;
    }appl_state;

    TegraProfPolicy();
    TegraProfPolicy(std::string csvSysFilename, std::string csvApplFilename);
    ~TegraProfPolicy();
    void run(int cycle);
    
  private:
    monitor_t* appl_monitor;
    GetNode_Tegra* sensors;
    Utilization* utilization;

    std::ofstream* csvSysFile;
    std::string csvSysFilename;
    std::ofstream* csvApplFile;
    std::string csvApplFilename;

    std::map<pid_t, TegraProfPolicy::appl_state> appl_info;
};

#endif
