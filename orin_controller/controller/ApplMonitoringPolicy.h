#ifndef APPLMONITORINGPOLICY_H_
#define APPLMONITORINGPOLICY_H_

#include "Policy.h"
#include "ApplicationMonitor.h"
#include "utilization.h"

class ApplMonitoringPolicy: public Policy {
  public:
    static const bool enableUtil = true;

    ApplMonitoringPolicy(bool utilEnabled = false);
    ~ApplMonitoringPolicy();
    void run(int cycle);
    
  private:
    bool utilEnabled;
    monitor_t* appl_monitor;
    Utilization* utilization;
};

#endif
