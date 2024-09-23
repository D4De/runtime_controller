#ifndef APPLMONITORINGPOLICY_H_
#define APPLMONITORINGPOLICY_H_

#include "Policy.h"
#include "ApplicationMonitor.h"
#include "getNode.h"
#include "utilization.h"

class ApplMonitoringPolicy: public Policy{
  public:
    ApplMonitoringPolicy(bool sensorsEnabled = false);
    ~ApplMonitoringPolicy();
    void run(int cycle);
    
  private:
    bool sensorsEnabled;
    monitor_t* appl_monitor;
    GetNode* sensors;
    Utilization* utilization;
};

#endif
