/*
 *-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
 *
 * Copyright 2018 Intel Corporation All Rights Reserved.
 *
 *-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
 */

#include <signal.h>
#include <string>
#include <iostream>

#ifdef HAVE_WINDOWS_H
#include <windows.h>
#endif

#include <unistd.h>
#include <pthread.h>

#include "Platform.h"
#include "OCApi.h"
#include "iotivity_config.h"
#include "json/json.h"
#include "Agent.h"


using namespace OC;


int main(void)
{
    Platform platform;
    if (OC_STACK_OK != platform.start())
    {
        std::cerr << "Failed to start the IoTivity platform." << std::endl;
        return 1;
    }
    // initialize "oic/p"
    std::cout << "oic/p" << std::endl;
    if (OC_STACK_OK != platform.registerPlatformInfo())
    {
        std::cerr << "Failed Platform Registration (oic/p)." << std::endl;
    }
    // initialize "oic/d"
    std::cout << "oic/d" << std::endl;
    if (OC_STACK_OK != platform.setDeviceInfo())
    {
        std::cerr << "Failed Device Registration (oic/d)." << std::endl;
    }


    std::cout << "device type: " <<  platform.deviceType << std::endl;
    std::cout << "platformID: " <<  platform.getPlatformInfo()->platformID << std::endl;
    std::cout << "platform independent: " << platform.protocolIndependentID << std::endl;

    
    Agent agent;


    while(1){

    }
    return 0;
}
