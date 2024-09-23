#ifndef ODROIDPOLICY_H_
#define ODROIDPOLICY_H_

#include "Policy.h"
#include "ApplicationMonitor.h"
#include "getNode.h"
#include "utilization.h"
#include "MappingConsts.h"
#include <map>
#include <iostream>
#include <fstream>

class OdroidPolicy: public Policy{
  public:
    typedef struct{
      //Throughput info
      float thr; //last measure of throughput
      float thr_ref; //target throughput
      int mapping; //0=LITTLE, 1=big, 2=GPU
      bool mt;
    }appl_state;

    OdroidPolicy();
    OdroidPolicy(std::string csvSysFilename, std::string csvApplFilename);
    ~OdroidPolicy();
    void run(int cycle);
    
  private:
    monitor_t* appl_monitor;
    GetNode* sensors;
    Utilization* utilization;

    std::ofstream* csvSysFile;
    std::string csvSysFilename;
    std::ofstream* csvApplFile;
    std::string csvApplFilename;

    std::map<pid_t, OdroidPolicy::appl_state> appl_info;
};

#endif
