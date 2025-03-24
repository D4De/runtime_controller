#ifndef TEGRANANOPOLICY_H_
#define TEGRANANOPOLICY_H_

#include "Policy.h"
#include "ApplicationMonitor.h"
#include "getNode_TegraNano.h"
#include "MappingConsts.h"
#include "utilization.h"
#include <map>
#include <iostream>
#include <fstream>

class TegraNanoPolicy: public Policy{
  public:
    typedef struct{
      //Throughput info
      float thr; //last measure of throughput
      float thr_ref; //target throughput
      int mapping; //0=LITTLE, 1=big, 2=GPU
      bool mt;
    }appl_state;

    TegraNanoPolicy();
    TegraNanoPolicy(std::string csvSysFilename, std::string csvApplFilename);
    ~TegraNanoPolicy();
    void run(int cycle);
    
  private:
    monitor_t* appl_monitor;
    GetNode_TegraNano* sensors;
    Utilization* utilization;

    std::ofstream* csvSysFile;
    std::string csvSysFilename;
    std::ofstream* csvApplFile;
    std::string csvApplFilename;

    std::map<pid_t, TegraNanoPolicy::appl_state> appl_info;
    std::vector<int> availablecpufreqs;
};

#endif
