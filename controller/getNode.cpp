#include "getNode.h"
#include <sys/ioctl.h>
#include <unistd.h>
#include <cstdio>	// sprintf
#include <cstdlib> // atoi
#include <fcntl.h>
#include <cstring>	// strncmp
#include <string>
#include <iostream>	// cerr
#include <fstream>	// cerr

GetNode::GetNode(bool noPowerSensors, bool noSmartPower) {
  this->noPowerSensors = noPowerSensors;
  this->noSmartPower = noSmartPower;

  //paths to access CPU frequency infos
  for (int i = 0; i < ODROID_NUM_OF_CPU_CORES; i++) {
    std::string temp;
    temp = "/sys/devices/system/cpu/cpu" + std::to_string(i) + "/cpufreq/cpuinfo_cur_freq";
    cpu_node_list[i] = temp;
  }

  //initialize sensor driver
  if(!this->noPowerSensors){
    openINA231();
  }
  if(!this->noSmartPower){
    this->smartGauge = new SmartGauge();
    if(!this->smartGauge->initDevice()){
      exit(1);
    }
    //dummy read. first read returns always -1...
    this->smartGauge->getWatt();
  }

  //read sensors for the first time
  this->update_read();

  //initialize attributes for power consumption
  this->bigWavg[0] = this->bigWs = this->getBigW();
  this->gpuWavg[0] = this->gpuWs = this->getGpuW();
  this->littleWavg[0] = this->littleWs = this->getLittleW();
  this->memWavg[0] = this->memWs = this->getMemW();
  this->boardWavg[0] = this->boardWs = this->getBoardW();

  for(int i = 1; i < POW_WINDOW_SIZE; i++) {
    this->bigWavg[i] = 0;
    this->gpuWavg[i] = 0;
    this->littleWavg[i] = 0;
    this->memWavg[i] = 0;
    this->boardWavg[i] = 0;
  }
  this->slidingIndex = 1;
}

GetNode::~GetNode() {
  //close sensor driver
  if(!this->noPowerSensors)
    closeINA231();
  if(!this->noSmartPower)
    delete this->smartGauge;
}

int GetNode::getGPUCurFreq() {
  FILE *fp = NULL;
  char buf[4] = {'\0',};
  fp = fopen(GPUFREQ_NODE, "r");
  if (fp == NULL) {
    return 0;
  }
  // DO NOTE: this piece of code is really terrible since it assumes each freq
  // to be composed of 3 digits (in Mhz)
  int ret = fread(buf, 1, 3, fp);
  fclose(fp);

  return atoi(buf);
}

int GetNode::getCPUCurFreq(int cpuNum) {
  FILE *fp = NULL;
  char buf[8] = {'\0',};
  fp = fopen(cpu_node_list[cpuNum].c_str(), "r");
  if (fp == NULL) {
    return 0;
  }
  int ret = fread(buf, 1, 8, fp);
  fclose(fp);

  return atoi(buf) / 1000;
}

int GetNode::setCPUFreq(int cpuNum, int freq) {
  char buff[100];
  sprintf(buff, "cpufreq-set --min %dMHz --max %dMHz -c %d -g performance",freq, freq, cpuNum);
  return system(buff);
}

int GetNode::setGPUFreq(int freq) {
#ifdef KERNEL_4_14
  char buff[100];
  int ris1, ris2;
  //f are received in MHz and must be translated in Hz
  sprintf(buff, "echo %d > %s", freq*1000000, GPUFREQMIN_NODE);
  ris1 = system(buff);
  sprintf(buff, "echo %d > %s", freq*1000000, GPUFREQMAX_NODE);
  ris2 = system(buff);
  return (ris1 || ris2);
#else
  std::cout << "NOT IMPLEMENTED" << std::endl;
  exit(1);
#endif
}

int GetNode::getCPUTemp(int cpuNum) { //TODO to be completelly changed by reading all sensors together
  //big5=cpuNum0, big6=cpuNum1, big7=cpuNum2, big8=cpuNum3, gpu=cpuNum4
  //DO NOTE: sensors for big6 and big8 are reversed (i.e. sensor1 is related to big8 and sensor3 to big6)
  if(cpuNum==3) cpuNum=1;
  else if(cpuNum==1) cpuNum=3;

#ifdef KERNEL_4_14
  FILE *fp = NULL;
  std::string temp;
  temp = "/sys/devices/virtual/thermal/thermal_zone" + std::to_string(cpuNum) + "/temp";
  return this->read_sensor(temp.c_str())/1000;
#else
  FILE *fp = NULL;
  fp = fopen(TEMP_NODE, "r");
  char buf[16];
  if (fp == NULL) {
    return 0;
  }

  //big5=cpuNum0, big6=cpuNum1, big7=cpuNum2, big8=cpuNum3, gpu=cpuNum4
  //DO NOTE: sensors for big6 and big8 are reversed (i.e. sensor1 is related to big8 and sensor3 to big6)
  if(cpuNum==3) cpuNum=1;
  else if(cpuNum==1) cpuNum=3;

  for (int i = 0; i < cpuNum + 1; i++)
    int ret = fread(buf, 1, 16, fp);
  fclose(fp);
  buf[12] = '\0';
  return atoi(&buf[9]);
#endif
}

int GetNode::getFanSpeed() {
#ifdef KERNEL_4_14
  FILE *fp = NULL;
  char buf[4] = {'\0',};
  fp = fopen(FAN_NODE, "r");
  if (fp == NULL) {
    return -1;
  }
  int ret = fread(buf, 1, 4, fp);
  fclose(fp);

  return atoi(buf);
#else
  std::cout << "NOT IMPLEMENTED" << std::endl;
  exit(1);
#endif
}

bool GetNode::isFanAuto() {
#ifdef KERNEL_4_14
  FILE *fp = NULL;
  char buf[2] = {'\0',};
  fp = fopen(FAN_AUTO, "r");
  if (fp == NULL) {
    return -1;
  }
  int ret = fread(buf, 1, 2, fp);
  fclose(fp);

  return (atoi(buf)==1?true:false);
#else
  std::cout << "NOT IMPLEMENTED" << std::endl;
  exit(1);
#endif
}

void GetNode::setFanSpeed(int pwm) {
#ifdef KERNEL_4_14
  char buff[100];
  int ris1;
  //f are received in MHz and must be translated in Hz
  sprintf(buff, "echo %d > %s", pwm, FAN_NODE);
  ris1 = system(buff);
#else
  std::cout << "NOT IMPLEMENTED" << std::endl;
  exit(1);
#endif
}

void GetNode::setFanAuto(bool on) {
#ifdef KERNEL_4_14
  char buff[100];
  int ris1;
  //f are received in MHz and must be translated in Hz
  sprintf(buff, "echo %d > %s", (on?1:0), FAN_AUTO);
  ris1 = system(buff);
#else
  std::cout << "NOT IMPLEMENTED" << std::endl;
  exit(1);
#endif
}

void GetNode::open_sensor(const char *node) {
  std::ofstream myfile;
  myfile.open (node);
  if (myfile.is_open()) {
    myfile << "1\n";
    myfile.close();
  } else
    std::cerr << node << "Open Fail" << std::endl;
}

void GetNode::openINA231() {
  open_sensor(SENSOR_ARM_ENABLE);
  open_sensor(SENSOR_MEM_ENABLE);
  open_sensor(SENSOR_KFC_ENABLE);
  open_sensor(SENSOR_G3D_ENABLE);
}

void GetNode::closeINA231(void) {
  //DO NOTE since the sensor enabling is system-wide I decide to do not disable them!
}

float GetNode::read_sensor(const char *node) {
  float value;
  std::ifstream myfile;
  myfile.open(node);
  if (myfile.is_open()) {
    myfile >> value;
    myfile.close();
  }
  return value;
}

float GetNode::getLittleA(){
  return this->littleA;
}

float GetNode::getLittleV(){
  return this->littleV;
}

float GetNode::getLittleW(){
  return this->littleW;
}

float GetNode::getBigA(){
  return this->bigA;
}

float GetNode::getBigV(){
  return this->bigV;
}

float GetNode::getBigW(){
  return this->bigW;
}

float GetNode::getGpuA(){
  return this->gpuA;
}

float GetNode::getGpuV(){
  return this->gpuV;
}

float GetNode::getGpuW(){
  return this->gpuW;
}

float GetNode::getMemA(){
  return this->memA;
}

float GetNode::getMemV(){
  return this->memV;
}

float GetNode::getMemW(){
  return this->memW;
}

float GetNode::getBoardW(){
  return this->boardW;
}


float GetNode::estimate_power(std::vector<int> utils, int bigFreq, int littleFreq, int cpu_fan_pwm) {
  float power = 0;
  //pre-process utilizations of big and little clusters
  int utilB = 0, utilL = 0;
  if(utils.size()==ODROID_NUM_OF_CPU_CORES){
    for(int i=0; i<ODROID_NUM_OF_CPU_CORES/2; i++)
      utilL += utils[i];
    for(int i=4; i<ODROID_NUM_OF_CPU_CORES; i++)
      utilB += utils[i];
  }
  //TODO this code is redundant with the subsequent functions... refactor to avoid repetitions...
  switch(littleFreq){
    case 200:
      power = 0.016 + utilL * 0.013 / 100.0;
      break;
    case 300:
      power = 0.018 + utilL * 0.020 / 100.0;
      break;
    case 400:
      power = 0.021 + utilL * 0.026 / 100.0;
      break;
    case 500:
      power = 0.023 + utilL * 0.033 / 100.0;
      break;
    case 600:
      power = 0.027 + utilL * 0.041 / 100.0;
      break;
    case 700:
      power = 0.033 + utilL * 0.051 / 100.0;
      break;
    case 800:
      power = 0.040 + utilL * 0.064 / 100.0;
      break;
    case 900:
      power = 0.058 + utilL * 0.076 / 100.0;
      break;
    case 1000:
      power = 0.068 + utilL * 0.092 / 100.0;
      break;
    case 1100:
      power = 0.080 + utilL * 0.109 / 100.0;
      break;
    case 1200:
      power = 0.094 + utilL * 0.128 / 100.0;
      break;
    case 1300:
      power = 0.113 + utilL * 0.150 / 100.0;
      break;
    case 1400:
      power = 0.135 + utilL * 0.176 / 100.0;
      break;
    default:
      std::cerr << "unreachable case" << std::endl;
      exit(0);
  }

  switch(bigFreq){
    case 200:
      power += 0.097 + utilB * 0.067 / 100.0;
      break;
    case 300:
      power += 0.112 + utilB * 0.101 / 100.0;
      break;
    case 400:
      power += 0.127 + utilB * 0.135 / 100.0;
      break;
    case 500:
      power += 0.142 + utilB * 0.170 / 100.0;
      break;
    case 600:
      power += 0.156 + utilB * 0.205 / 100.0;
      break;
    case 700:
      power += 0.171 + utilB * 0.237 / 100.0;
      break;
    case 800:
      power += 0.185 + utilB * 0.272 / 100.0;
      break;
    case 900:
      power += 0.206 + utilB * 0.310 / 100.0;
      break;
    case 1000:
      power += 0.240 + utilB * 0.347 / 100.0;
      break;
    case 1100:
      power += 0.275 + utilB * 0.404 / 100.0;
      break;
    case 1200:
      power += 0.315 + utilB * 0.471 / 100.0;
      break;
    case 1300:
      power += 0.358 + utilB * 0.534 / 100.0;
      break;
    case 1400:
      power += 0.390 + utilB * 0.589 / 100.0;
      break;
    case 1500:
      power += 0.421 + utilB * 0.680 / 100.0;
      break;
    case 1600:
      power += 0.491 + utilB * 0.787 / 100.0;
      break;
    case 1700:
      power += 0.572 + utilB * 0.891 / 100.0;
      break;
    case 1800:
      power += 0.662 + utilB * 1.025 / 100.0;
      break;
    case 1900:
      power += 0.79  + utilB * 1.203 / 100.0;
      break;
    case 2000:
      power += 1.02  + utilB * 1.531 / 100.0;
      break;
    default:
      std::cerr << "unreachable case" << std::endl;
      exit(0);
  }

  return power;
}

void GetNode::update_read(std::vector<int> utils){
  //collect instantaneous values (or use the estimation model if no sensor is available)
  if(!this->noPowerSensors){
    this->littleW = this->read_sensor(SENSOR_KFC_W);
    this->littleA = this->read_sensor(SENSOR_KFC_A);
    this->littleV = this->read_sensor(SENSOR_KFC_V);

    this->bigW = this->read_sensor(SENSOR_ARM_W);
    this->bigA = this->read_sensor(SENSOR_ARM_A);
    this->bigV = this->read_sensor(SENSOR_ARM_V);

    this->gpuW = this->read_sensor(SENSOR_G3D_W);
    this->gpuA = this->read_sensor(SENSOR_G3D_A);
    this->gpuV = this->read_sensor(SENSOR_G3D_V);

    this->memW = this->read_sensor(SENSOR_MEM_W);
    this->memA = this->read_sensor(SENSOR_MEM_A);
    this->memV = this->read_sensor(SENSOR_MEM_V);
  } else{
    //pre-process utilizations of big and little clusters
    int utilB = 0, utilL = 0;
    if(utils.size()==ODROID_NUM_OF_CPU_CORES){
      for(int i=0; i<ODROID_NUM_OF_CPU_CORES/2; i++)
        utilL += utils[i];
      for(int i=4; i<ODROID_NUM_OF_CPU_CORES; i++)
        utilB += utils[i];
    }
    float power;
    float volt;

    //power model for little cluster
    int freq = getCPUCurFreq(1);
    switch(freq){
      case 200:
        power = 0.016 + utilL * 0.013 / 100.0;
        volt = 0.9;
        break;
      case 300:
        power = 0.018 + utilL * 0.020 / 100.0;
        volt = 0.9;
        break;
      case 400:
        power = 0.021 + utilL * 0.026 / 100.0;
        volt = 0.9;
        break;
      case 500:
        power = 0.023 + utilL * 0.033 / 100.0;
        volt = 0.9;
        break;
      case 600:
        power = 0.027 + utilL * 0.041 / 100.0;
        volt = 0.9;
        break;
      case 700:
        power = 0.033 + utilL * 0.051 / 100.0;
        volt = 0.9;
        break;
      case 800:
        power = 0.040 + utilL * 0.064 / 100.0;
        volt = 1.0;
        break;
      case 900:
        power = 0.058 + utilL * 0.076 / 100.0;
        volt = 1.0;
        break;
      case 1000:
        power = 0.068 + utilL * 0.092 / 100.0;
        volt = 1.0;
        break;
      case 1100:
        power = 0.080 + utilL * 0.109 / 100.0;
        volt = 1.1;
        break;
      case 1200:
        power = 0.094 + utilL * 0.128 / 100.0;
        volt = 1.1;
        break;
      case 1300:
        power = 0.113 + utilL * 0.150 / 100.0;
        volt = 1.2;
        break;
      case 1400:
        power = 0.135 + utilL * 0.176 / 100.0;
        volt = 1.2;
        break;
      default:
        std::cerr << "unreachable case" << std::endl;
        exit(0);
    }
    this->littleW = power;
    this->littleV = volt;
    this->littleA = power/volt;

    //power model for big cluster
    freq = getCPUCurFreq(5);
    switch(freq){
      case 200:
        power = 0.097 + utilB * 0.067 / 100.0;
        volt = 0.9;
        break;
      case 300:
        power = 0.112 + utilB * 0.101 / 100.0;
        volt = 0.9;
        break;
      case 400:
        power = 0.127 + utilB * 0.135 / 100.0;
        volt = 0.9;
        break;
      case 500:
        power = 0.142 + utilB * 0.170 / 100.0;
        volt = 0.9;
        break;
      case 600:
        power = 0.156 + utilB * 0.205 / 100.0;
        volt = 0.9;
        break;
      case 700:
        power = 0.171 + utilB * 0.237 / 100.0;
        volt = 0.9;
        break;
      case 800:
        power = 0.185 + utilB * 0.272 / 100.0;
        volt = 0.9;
        break;
      case 900:
        power = 0.206 + utilB * 0.310 / 100.0;
        volt = 0.9;
        break;
      case 1000:
        power = 0.240 + utilB * 0.347 / 100.0;
        volt = 0.9;
        break;
      case 1100:
        power = 0.275 + utilB * 0.404 / 100.0;
        volt = 0.9;
        break;
      case 1200:
        power = 0.315 + utilB * 0.471 / 100.0;
        volt = 1.0;
        break;
      case 1300:
        power = 0.358 + utilB * 0.534 / 100.0;
        volt = 1.0;
        break;
      case 1400:
        power = 0.390 + utilB * 0.589 / 100.0;
        volt = 1.0;
        break;
      case 1500:
        power = 0.421 + utilB * 0.680 / 100.0;
        volt = 1.0;
        break;
      case 1600:
        power = 0.491 + utilB * 0.787 / 100.0;
        volt = 1.0;
        break;
      case 1700:
        power = 0.572 + utilB * 0.891 / 100.0;
        volt = 1.1;
        break;
      case 1800:
        power = 0.662 + utilB * 1.025 / 100.0;
        volt = 1.1;
        break;
      case 1900:
        power = 0.79  + utilB * 1.203 / 100.0;
        volt = 1.2;
        break;
      case 2000:
        power = 1.02  + utilB * 1.531 / 100.0;
        volt = 1.2;
        break;
      default:
        std::cerr << "unreachable case" << std::endl;
        exit(0);
    }
    this->bigW = power;
    this->bigV = volt;
    this->bigA = power/volt;

    //TODO implement power models.
    this->gpuW = 0.0;
    this->gpuV = 0.0;
    this->gpuA = 0.0;

    this->memW = 0.0;
    this->memV = 0.0;
    this->memA = 0.0;
  }

  if(!this->noSmartPower)
    this->boardW = this->smartGauge->getWatt();
  else 
    this->boardW = 0;
}

void GetNode::updateSensing(std::vector<int> utils) {
  this->update_read(utils);

  //update average values
  this->bigWs = this->bigWs - this->bigWavg[this->slidingIndex] + this->bigW;
  this->gpuWs = this->gpuWs - this->gpuWavg[this->slidingIndex] + this->gpuW;
  this->littleWs = this->littleWs - this->littleWavg[this->slidingIndex] + this->littleW;
  this->memWs = this->memWs - this->memWavg[this->slidingIndex] + this->memW;
  this->boardWs = this->boardWs - this->boardWavg[this->slidingIndex] + this->boardW;
  
  this->bigWavg[this->slidingIndex] = this->bigW;
  this->gpuWavg[this->slidingIndex] = this->gpuW;
  this->littleWavg[this->slidingIndex] = this->littleW;
  this->memWavg[this->slidingIndex] = this->memW;
  this->boardWavg[this->slidingIndex] = this->boardW;

  this->slidingIndex = (this->slidingIndex+1)%POW_WINDOW_SIZE;
}

float GetNode::getGpuWavg() {
  return this->gpuWs / POW_WINDOW_SIZE;
}

float GetNode::getBigWavg() {
  return this->bigWs / POW_WINDOW_SIZE;
}

float GetNode::getMemWavg() {
  return this->memWs / POW_WINDOW_SIZE;
}

float GetNode::getLittleWavg() {
  return this->littleWs / POW_WINDOW_SIZE;
}

float GetNode::getBoardWavg() {
  return this->boardWs / POW_WINDOW_SIZE;
}

