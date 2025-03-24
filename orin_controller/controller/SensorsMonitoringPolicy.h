#ifndef SENSORSMONITORINGPOLICY_H_
#define SENSORSMONITORINGPOLICY_H_

#include "Policy.h"
#include "ApplicationMonitor.h"
#include "utilization.h"
#include "OrinHWLayer.h"

class SensorsMonitoringPolicy: public Policy{
  public:
    SensorsMonitoringPolicy();
    ~SensorsMonitoringPolicy();
    void run(int cycle);
    
  private:
    monitor_t* appl_monitor;
    Utilization* utilization;
    OrinHWLayer* sensors;

    //to avoid to ask this piece of info to the driver at each control cycle
    //get them in the constructor and save in class attributes
    std::vector<int> cpu_freqs, gpu_freqs;
};

#endif
