#ifndef ORINHWLAYER_H
#define ORINHWLAYER_H

#include <sys/ioctl.h>
#include <string>
#include <vector>

/*
 * This class represents the interface to the virtual file system to get information from 
 * board sensors (current, voltage, ...)
 */

//Orin NX 16GB has 8 core while Orin NX 8GB has 6 cores
//do note that the CPU is divided in two clusters with separate frequency tuning
#define ORIN_16GB_NUM_OF_CORES 8
#define ORIN_8GB_NUM_OF_CORES 6
#define ORIN_NUM_OF_CORES ORIN_16GB_NUM_OF_CORES
#define NUM_OF_CPU_CLUSTERS 2
#define CLUSTER0_CORE 0
#define CLUSTER1_CORE (ORIN_NUM_OF_CORES-1)
static int CLUSTER_REF_CORES[] = {CLUSTER0_CORE, CLUSTER1_CORE};

//Paths and descriptions can be get from the following link:
//
// https://docs.nvidia.com/jetson/archives/r35.4.1/DeveloperGuide/text/SD/PlatformPowerAndPerformance/JetsonOrinNanoSeriesJetsonOrinNxSeriesAndJetsonAgxOrinSeries.html

//Sensor to the main power line
#define VDD_IN_A "/sys/bus/i2c/drivers/ina3221/1-0040/hwmon/hwmon4/curr1_input"
#define VDD_IN_V "/sys/bus/i2c/drivers/ina3221/1-0040/hwmon/hwmon4/in1_input"
//NOT USED YET. They refers to the various parts of the system
#define VDD_CPU_GPU_CV_A "/sys/bus/i2c/drivers/ina3221/1-0040/hwmon/hwmon4/curr2_input"
#define VDD_CPU_GPU_CV_V "/sys/bus/i2c/drivers/ina3221/1-0040/hwmon/hwmon4/in2_input"
#define VDD_SOC_A "/sys/bus/i2c/drivers/ina3221/1-0040/hwmon/hwmon4/curr3_input"
#define VDD_SOC_V "/sys/bus/i2c/drivers/ina3221/1-0040/hwmon/hwmon4/in3_input"

//Paths to get and set CPU status and frequency
#define CPU_PATH "/sys/devices/system/cpu/cpu"
#define CPU_ONLINE "/online"
#define CPUINFO_CURR_FREQ "/cpufreq/cpuinfo_cur_freq"
#define CPU_SCALING_CUR_FREQ "/cpufreq/scaling_cur_freq"
#define CPU_SCALING_MAX_FREQ "/cpufreq/scaling_max_freq"
#define CPU_SCALING_MIN_FREQ "/cpufreq/scaling_min_freq"
#define CPU_SCALING_GOVERNOR "/cpufreq/scaling_governor"
#define CPU_SCALING_AVAILABLE_FREQUENCIES "/cpufreq/scaling_available_frequencies"

//Paths to get and set GPU status and frequency
#define GPU_PATH "/sys/devices/gpu.0/devfreq/17000000.ga10b"
#define GPU_CURR_FREQ "/cur_freq"
#define GPU_MAX_FREQ "/max_freq"
#define GPU_MIN_FREQ "/min_freq"
#define GPU_GOVERNOR "/governor"
#define GPU_AVAILABLE_FREQUENCIES "/available_frequencies"

//Starting configuration for frequency control
#define PERFORMANCE_GOVERNOR "performance"
#define INITIAL_CPU_FREQUENCY "729600"
#define INITIAL_GPU_FREQUENCY "306000000"

#define POW_WINDOW_SIZE 5

/* Data structure to save the original status of the CPU and GPU before changing the frequency 
   governor so that it can be restored when terminating the controller */
#define STRLEN 15

typedef struct {
  char scaling_governor[STRLEN+1];
  char scaling_max_freq[STRLEN+1];
  char scaling_min_freq[STRLEN+1];
} original_state_t;

/* functions to read values from the virtual file system */
void readStringFromAFile(const char* filename, char* value);
void writeStringIntoAFile(const char* filename, const char* value);
int readIntFromAFile(const char* filename);
float readFloatFromAFile(const char* filename);
void writeIntIntoAFile(const char* filename, const int value);
std::vector<int> readVectorOfIntFromAFile(const char* filename);

class OrinHWLayer {
public:
  //Constructor and Destructor
  OrinHWLayer();
  ~OrinHWLayer();

  //getters for status and frequency
  //CPU
  bool isCPUActive(int cpuNum);
  //DO NOTE: there are two different getters for the CPU freq since the actual one (getCPUCurFreq) 
  //use to oscillate. So with the second getter (getCPUCurScalFreq) it is possible to check which 
  //is the value that was set
  int getCPUCurFreq(int cpuNum);
  int getCPUCurScalFreq(int cpuNum);
  std::vector<int> getAvailableCPUFreqs(int cpuNum = 0);
  //GPU
  int getGPUCurFreq();
  std::vector<int> getAvailableGPUFreqs();

  //enum type to specify the frequency control configuration  
  typedef enum {
    NO_FREQUENCY_CONTROL = 0,
    CPU_FREQUENCY_CONTROL = 1,
    GPU_FREQUENCY_CONTROL = 2,
    ALL_FREQUENCY_CONTROL = 3
  } frequencyControl_t;

  //enable/disable frequency control on CPU ad GPU (config values are in the above enum)
  void enableFrequencyControl(frequencyControl_t config = OrinHWLayer::ALL_FREQUENCY_CONTROL);
  //used mainly for debugging purposes
  void printFrequencyControlConfig();

  //setters for status and frequency
  //CPU
  void setCPUActive(int cpuNum, bool status);
  void setCPUCoreFreq(int cpuNum, int freq);
  void setCPUClusterFreq(int clusterNum, int freq);
  void setCPUFreq(int freq);
  //GPU
  void setGPUFreq(int freq);

  //getter for current, voltage and power consumption
  float getVddInV();
  float getVddInA();
  float getVddInW();
   
  //getters for average power consumption on a sliding window of POW_WINDOW_SIZE samples
  float getVddInWavg();
    
  //PAY ATTENTION: this function has to be called to update sensed data. therefore BEFORE using above getters
  void updateSensing(); 

private:
  //dump of the frequency control configuration before launching the controller
  original_state_t original_cpu_status[NUM_OF_CPU_CLUSTERS];
  original_state_t original_gpu_status;
  //set frequency control configuration
  frequencyControl_t frequencyControlEnabled;

  //variables where last read is stored
  float vddInV, vddInA, vddInW;
  
  //sum of last POW_WINDOW_SIZE samples
  float vddInWs;

  //Attributes used to compute the mean power consumption of the last POW_WINDOW_SIZE periodic invocations of updateWavg() function
  //array of last POW_WINDOW_SIZE samples
  float vddInWavg[POW_WINDOW_SIZE];
  
  //index of the next position of the above arrays to be written
  int slidingIndex;

  //read values from sensors
  void readSensors();
};

#endif // ORINHWLAYER_H
