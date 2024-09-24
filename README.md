# Runtime Resource Controller
This project implements a runtime resource management framework for multicores running a multiprogrammed workload working on Linux, capable of monitoring system status (both running applications attached to the controller and hardware parameters) and actuating on available hardware and software knobs to optimize a given objective function. Indeed the controller here implemented is a skeleton and has to be customized with the desired decision policy and extended to support actual sensors and knobs available on the target architecture; also, software knob control has to be implemented based on the necessities of the working scenario.

For a methodological description of the controller, please refer to:
Anil Kanduri, Antonio Miele, Amir M. Rahmani, Pasi Liljeberg, Cristiana Bolchini, and Nikil Dutt. 2018. Approximation-aware coordinated power/performance management for heterogeneous multi-cores. In Proceedings of the 55th Annual Design Automation Conference (DAC '18). pp. 1–6. https://doi.org/10.1145/3195970.3195994

At present, there is no technical documentation except for the comments within the source code itself.

If you use the controller in your research, we would appreciate a citation to:

>@inproceedings{km+2018,<br>
>  author = {A. Kanduri and A. Miele and A.M. Rahmani and P. Liljeberg and C. Bolchini and N, Dutt},<br>
>  title = {Approximation-aware coordinated power/performance management for heterogeneous multi-cores},<br>
>  booktitle = {Proceedings of the 55th Annual Design Automation Conference},<br>
>  year={2018},<br>
>  pages={1-6},<br>
>  doi={https://doi.org/10.1145/3195970.3195994}<br>
>}


## Copyright & License

Copyright (C) 2024 Politecnico di Milano.

This framework is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

This framework is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the [GNU General Public License](https://www.gnu.org/licenses/) for more details.

Neither the name of Politecnico di Milano nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

## Dependencies and Requirements 
The project runs on Ubuntu Linux (<= 20.04) and requires ``cgmanager libcgroup-dev`` packages to be installed. Indeed, in newer Ubuntu versions, CGroup management has been changed, and the controller does not work properly.

## Compilation 

To install the framework use ``make`` command:
```
cd basic_controller
make
cd ..
cd sha
make
```

## Run a demo of the controller

To run a very simple demo with a controller monitoring the system status:

a. start the controller:
```
cd basic_controller
sudo ./controller -p 0
```

b. start one of the applications:
```
cd sha
./sha input_large.asc 10000
```

Actually the demo just only monitors the status of the system (application performance and system status). To kill it, it is sufficient to kill only the controller with ``ctrl+c``. 


## A more advanced version of the controller

In the ``controller`` folder there is a more advanced version of the controller targeted for Nvidia Jetson Nano board. This version demonstrates how to interact with hardware sensors and knobs and integrates an example of resource management policy.


