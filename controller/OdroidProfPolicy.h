#ifndef ODROIDPROFPOLICY_H_
#define ODROIDPROFPOLICY_H_

#include "Policy.h"
#include "ApplicationMonitor.h"
#include "getNode.h"
#include "utilization.h"
#include "MappingConsts.h"
#include <map>
#include <iostream>
#include <fstream>

class OdroidProfPolicy: public Policy{
  public:
    typedef struct{
      //Throughput info
      float thr; //last measure of throughput
      float thr_ref; //target throughput
      int mapping; //0=LITTLE, 1=big, 2=GPU
      bool mt;
    }appl_state;

    OdroidProfPolicy();
    OdroidProfPolicy(std::string csvSysFilename, std::string csvApplFilename);
    ~OdroidProfPolicy();
    void run(int cycle);
    
  private:
    monitor_t* appl_monitor;
    GetNode* sensors;
    Utilization* utilization;

    std::ofstream* csvSysFile;
    std::string csvSysFilename;
    std::ofstream* csvApplFile;
    std::string csvApplFilename;

    std::map<pid_t, OdroidProfPolicy::appl_state> appl_info;
};

#endif
