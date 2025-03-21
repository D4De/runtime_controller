#ifndef GETNODE_H
#define GETNODE_H

#include <sys/ioctl.h>
#include <string>
#include <vector>
#include "smartgauge.hpp"

/*
 * This class represents the interface to the virtual file system to get information from the Odroid sensors (current, voltage power. temperature and utilization).
 * This code has been taken (and slightly readapted) from the Odroid EnergyMonitor
 */


#define KERNEL_4_14

#ifdef KERNEL_4_14

#define SENSOR_ARM_ENABLE  "/sys/bus/i2c/drivers/INA231/0-0040/enable"
#define SENSOR_MEM_ENABLE  "/sys/bus/i2c/drivers/INA231/0-0041/enable"
#define SENSOR_G3D_ENABLE  "/sys/bus/i2c/drivers/INA231/0-0044/enable"
#define SENSOR_KFC_ENABLE  "/sys/bus/i2c/drivers/INA231/0-0045/enable"

#define SENSOR_ARM_A  "/sys/bus/i2c/drivers/INA231/0-0040/sensor_A"
#define SENSOR_MEM_A  "/sys/bus/i2c/drivers/INA231/0-0041/sensor_A"
#define SENSOR_G3D_A  "/sys/bus/i2c/drivers/INA231/0-0044/sensor_A"
#define SENSOR_KFC_A  "/sys/bus/i2c/drivers/INA231/0-0045/sensor_A"

#define SENSOR_ARM_V  "/sys/bus/i2c/drivers/INA231/0-0040/sensor_V"
#define SENSOR_MEM_V  "/sys/bus/i2c/drivers/INA231/0-0041/sensor_V"
#define SENSOR_G3D_V  "/sys/bus/i2c/drivers/INA231/0-0044/sensor_V"
#define SENSOR_KFC_V  "/sys/bus/i2c/drivers/INA231/0-0045/sensor_V"

#define SENSOR_ARM_W  "/sys/bus/i2c/drivers/INA231/0-0040/sensor_W"
#define SENSOR_MEM_W  "/sys/bus/i2c/drivers/INA231/0-0041/sensor_W"
#define SENSOR_G3D_W  "/sys/bus/i2c/drivers/INA231/0-0044/sensor_W"
#define SENSOR_KFC_W  "/sys/bus/i2c/drivers/INA231/0-0045/sensor_W"

#define GPUFREQ_NODE  "/sys/devices/platform/11800000.mali/devfreq/devfreq0/cur_freq"
#define GPUFREQMAX_NODE  "/sys/devices/platform/11800000.mali/devfreq/devfreq0/max_freq"
#define GPUFREQMIN_NODE  "/sys/devices/platform/11800000.mali/devfreq/devfreq0/min_freq"

#define FAN_NODE "/sys/devices/platform/pwm-fan/hwmon/hwmon0/pwm1"
#define FAN_AUTO "/sys/devices/platform/pwm-fan/hwmon/hwmon0/automatic"

#else

#define SENSOR_ARM_ENABLE  "/sys/bus/i2c/drivers/INA231/2-0040/enable"
#define SENSOR_MEM_ENABLE  "/sys/bus/i2c/drivers/INA231/2-0041/enable"
#define SENSOR_KFC_ENABLE  "/sys/bus/i2c/drivers/INA231/2-0044/enable"
#define SENSOR_G3D_ENABLE  "/sys/bus/i2c/drivers/INA231/2-0045/enable"

#define SENSOR_ARM_A  "/sys/bus/i2c/drivers/INA231/2-0040/sensor_A"
#define SENSOR_MEM_A  "/sys/bus/i2c/drivers/INA231/2-0041/sensor_A"
#define SENSOR_G3D_A  "/sys/bus/i2c/drivers/INA231/2-0044/sensor_A"
#define SENSOR_KFC_A  "/sys/bus/i2c/drivers/INA231/2-0045/sensor_A"

#define SENSOR_ARM_V  "/sys/bus/i2c/drivers/INA231/2-0040/sensor_V"
#define SENSOR_MEM_V  "/sys/bus/i2c/drivers/INA231/2-0041/sensor_V"
#define SENSOR_G3D_V  "/sys/bus/i2c/drivers/INA231/2-0044/sensor_V"
#define SENSOR_KFC_V  "/sys/bus/i2c/drivers/INA231/2-0045/sensor_V"

#define SENSOR_ARM_W  "/sys/bus/i2c/drivers/INA231/2-0040/sensor_W"
#define SENSOR_MEM_W  "/sys/bus/i2c/drivers/INA231/2-0041/sensor_W"
#define SENSOR_G3D_W  "/sys/bus/i2c/drivers/INA231/2-0044/sensor_W"
#define SENSOR_KFC_W  "/sys/bus/i2c/drivers/INA231/2-0045/sensor_W"

#define GPUFREQ_NODE  "/sys/devices/11800000.mali/clock"
#define TEMP_NODE "/sys/devices/10060000.tmu/temp"

#endif

#define POW_WINDOW_SIZE 6


#define ODROID_NUM_OF_CPU_CORES 8

#define ODROID_LITTLE_CORE_ID 0
#define ODROID_LITTLE_MAX_FREQ 1400
#define ODROID_LITTLE_MIN_FREQ 400
#define ODROID_LITTLE_REF_FREQ 1400
#define ODROID_BIG_CORE_ID 4
#define ODROID_BIG_MAX_FREQ 1800
#define ODROID_BIG_MIN_FREQ 200
#define ODROID_BIG_REF_FREQ 1400
#define ODROID_FREQ_STEP 100


enum {
    SENSOR_ARM = 0,
    SENSOR_MEM,
    SENSOR_KFC,
    SENSOR_G3D,
    SENSOR_MAX
};

class GetNode {
public:
  static const bool NO_POWER_SENSORS = true;
  static const bool NO_SMART_POWER = true;

  //Constructor and Destructor
  GetNode(bool noPowerSensors = false, bool noSmartPower = false);
  ~GetNode();

  //getters for frequency and temperature
  int getGPUCurFreq();
  int getCPUCurFreq(int cpuNum);
  int getCPUTemp(int cpuNum);

  //setters for frequency
  int setCPUFreq(int cpuNum, int freq);
  int setGPUFreq(int freq);

  //get fan speed
  int getFanSpeed();
  bool isFanAuto();  
  void setFanSpeed(int pwm);
  void setFanAuto(bool on);

  //getter for current, voltage and power consumption
  float getLittleA();
  float getLittleV();
  float getLittleW();
  float getBigA();
  float getBigV();
  float getBigW();
  float getGpuA();
  float getGpuV();
  float getGpuW();
  float getMemA();
  float getMemV();
  float getMemW();
  
  float getBoardW();

  //getters for average power consumption on a sliding window of POW_WINDOW_SIZE samples
  float getGpuWavg();
  float getBigWavg();
  float getMemWavg();
  float getLittleWavg();
  float getBoardWavg();
  void updateSensing(std::vector<int> utils = std::vector<int>()); //PAY ATTENTION: this function has to be called to update sensed data. therefore BEFORE using above getters

  float estimate_power(std::vector<int> utils, int bigFreq, int littleFreq, int cpu_fan_pwm=0); //0<=pwm<=255

private:
  //state if power sensors are not available and therefore the estimated model is used
  bool noPowerSensors;
  bool noSmartPower;

  SmartGauge* smartGauge;

  //helper attribute
  std::string cpu_node_list[ODROID_NUM_OF_CPU_CORES];

  //variables where last read is stored
  float bigV, bigA, bigW;
  float littleV, littleA, littleW;
  float gpuV, gpuA, gpuW;
  float memV, memA, memW;
  
  float boardW;

  //Attributes used to compute the mean power consumption of the last POW_WINDOW_SIZE periodic invocations of updateWavg() function
  //array of last POW_WINDOW_SIZE samples
  float bigWavg[POW_WINDOW_SIZE];
  float gpuWavg[POW_WINDOW_SIZE];
  float littleWavg[POW_WINDOW_SIZE];
  float memWavg[POW_WINDOW_SIZE];
  float boardWavg[POW_WINDOW_SIZE];
  //sum of last POW_WINDOW_SIZE samples
  float bigWs, gpuWs, littleWs, memWs, boardWs;
  //index of the next position of the above arrays to be written
  int slidingIndex;

  //manage sensor drivers
  void open_sensor(const char *node);
  float read_sensor(const char *node);
  void update_read(std::vector<int> utils = std::vector<int>());

  //open/close sensor drivers
  void openINA231();
  void closeINA231();
};

#endif // GETNODE_H
