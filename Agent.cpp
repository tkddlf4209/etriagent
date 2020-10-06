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

#include <iostream>
#include <pthread.h>
#include <string>
#include <map>
#include <cstdlib>
#include <mutex>
#include <condition_variable>
#include <time.h>
#include <set>
#include <vector>
#include <unistd.h>
#include <queue>
#include <algorithm>

// IPC
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>


#include "json/json.h"
#include "Agent.h"
#include "OCPlatform.h"
#include "OCApi.h"
#include "iotivity_config.h"

#define SCAN_RESOURCE_TYPE "core.light"


// IPC PROP
#define PID_AGENT 1
#define PID_ETRI 2
#define PID_OCF 3

#define MSG_SIZE 1024
#define ETRI_QUEUE_KEY 1
#define OCF_QUEUE_KEY 2

#define MY_PID PID_AGENT

typedef struct{
    long mtype;
    char data[MSG_SIZE];
} msg_data;


// IPC REQ_TYPE
#define GET_REQ  "GET_REQ"
#define GET_RSP "GET_RSP"
#define POST_REQ "POST_REQ"
#define POST_RSP "POST_RSP"
#define CREATE_REQ  "CREATE_REQ"
#define CREATE_RSP "CREATE_RSP"

#define OCF_DC_RT_NAME "oic.r.dc"

// IPC JSON KEY
const  std::string KEY_REQ_TYPE= "req_type";
const  std::string KEY_RESOURCES= "resources";
const  std::string KEY_RESULT= "result";
const  std::string KEY_ID ="id";
const  std::string KEY_URI ="uri";

int  DEVICE_TYPE_OCF =  1;
int  DEVICE_TYPE_ETRI = 2;
int  COLLECT_PERIOD = 3;
int  OCF_SCAN_PERIOD = 5;


class ContentResource;
class SubjectResource;

std::set<std::string> uri_filter;
std::map<std::string,SubjectResource*> subject_map;
std::mutex mutex;

void startRecvQueue();
void startScan();
void startCollect();
void create(Json::Value value);
long now();
void sendMsg(Json::Value value,long msg_id, long dst_pid);
using namespace OC;

class ContentResource{
    public:
	std::string id;
	int device_type;
	std::shared_ptr<OCResource> ocf_resource;
	Json::Value etri_resource;
	
	// data update time
	long timestamp;

	// resource register time
	long register_timestamp;
	
	// dc info
	bool dc;
	


	ContentResource(std::string id, std::shared_ptr<OCResource> &ocf_resource,bool dc){
	    this->device_type = DEVICE_TYPE_OCF;
	    this->id = id;
	    this->ocf_resource = ocf_resource;
	    this->dc = dc; 
	    
	    this->register_timestamp = now();
	    
	}

	ContentResource(std::string id, Json::Value etri_resource,bool dc){
	    device_type = DEVICE_TYPE_ETRI;
	    this->id = id;
	    this->etri_resource = etri_resource;
	    this->dc = dc;
	    
	    this->register_timestamp = now();
	}

	void updateTime(){
	    timestamp = now();
	}
};

class SubjectResource{
    public:
	std::vector<ContentResource*> list;
	Json::Value value;
	
	bool isAdded(std::string id){
	    if(list.size() == 0){
		return false;
	    }else{
		for(auto cr : list){
		   if(cr->id.compare(id)==0){
			return true;
		   } 
		}
		return false;
	    }
	
	}


	// 1. priority high if dc property is false
	// 2. already dc = false resource added list.at(0) , priority high register_timestamp more than older 
	static bool compare_list(const ContentResource *a, const ContentResource *b){
	    if(a->dc &&  b->dc){
		return a->register_timestamp <  b->register_timestamp;
	    }else if (a->dc || b->dc ){
		if(!a->dc){
		    return true;
		}else{
		    return false;
		}	    
	    }else{
		return a->register_timestamp <  b->register_timestamp;
	    } 
	}

	void list_sort(){
	    sort(list.begin(),list.end(),compare_list);
	    //std::cout << "@@ SORT @@ first :  "<< list.at(0) -> id<< std::endl;
	}


};


long now(){
    auto timeInMilis = std::time(nullptr);
    return timeInMilis;
}


long etri_msgid; // queue_id
long ocf_msgid; // queue_id
void * recv_queue(void * arg){
    msg_data msg;
    int msg_size = sizeof(msg) - sizeof(msg.mtype);
    // etri message queue init 
    etri_msgid = msgget((key_t)ETRI_QUEUE_KEY,IPC_CREAT|0666);
    if(etri_msgid == -1){
	std::cout << "etri_msgid msgget() Error" << std::endl;
    }else{
	//etri queue clear
	while(msgrcv(etri_msgid,&msg,msg_size,MY_PID,IPC_NOWAIT) > 0){
	
	}
    }

    std::cout << "etri_msgid = "<< etri_msgid << std::endl;

    // ocf client message queue init
    ocf_msgid = msgget((key_t)OCF_QUEUE_KEY,IPC_CREAT|0666);
    if(ocf_msgid == -1){
	std::cout << "ocf_msgid msgget() Error" << std::endl;
    }else{
	//ocf queue clear
	while(msgrcv(ocf_msgid,&msg,msg_size,MY_PID,IPC_NOWAIT) > 0){
	
	}

    }

    std::cout << "ocf_msgid = "<< ocf_msgid << std::endl;


    Json::Value value;
    Json::Reader reader;
   

    for(;;){


	//if (msgrcv(msgid,&msg,msg_size,7777,0) > 0){
	
	if (msgrcv(etri_msgid,&msg,msg_size,MY_PID,IPC_NOWAIT) > 0){

	    //std::cout << "@@@ PID :"<< msg.mtype << std::endl;
	    
	    // char[] to String
	    std::string data(msg.data);

	    // parse String to JSON
	    bool success = reader.parse(data,value);
	    if(success){

		// Check JSON has Key 'req_type'
		if(value.isMember(KEY_REQ_TYPE)){
		    std::string req_type = value["req_type"].asString();
		    
		    // Check REQ_TYPE 
		    if(req_type.compare(CREATE_REQ)==0){
			// if not added , create ContentResource
			create(value);
		    }else if(req_type.compare(GET_RSP) == 0){ 

			
			
			// update info
			// 1. update first ContentResource list data in same uri SubjectResource
			// 2. update same uri SubjectResource Json::Value
			std::lock_guard<std::mutex> lock{mutex};
			for(auto it = subject_map.begin();it!=subject_map.end();it++){ 
			    std::string uri = it -> first;
				     
			    if(value.isMember(uri)){
				auto sr = it -> second;
			    
				std::cout << "* IPC GET OK  : "<< uri << " " << data  << std::endl;
				if(sr -> list.size() > 0 ){
				    auto cr = sr->list.at(0);
				    cr -> updateTime(); // 1
				}
				//sr -> value = value[uri]; // 2
				sr -> value = value; // 2
			    }		
				
			}
				
		    }else if(req_type.compare(POST_RSP) == 0){
			//1. dc update response	
		    }
		
		}
	    }else{
		std::cout << "json parse Error"<< data << std::endl;
	    }

	    
	    //std::cout << "data : "<< data<< std::endl;
	    

	}

	
	if (msgrcv(ocf_msgid,&msg,msg_size,MY_PID,IPC_NOWAIT) > 0){
	    //std::cout << "@@@ PID :"<< msg.mtype << std::endl;
	    
	    std::string data(msg.data);
	    
	    bool success = reader.parse(data,value);
	    if(success){
		if(value.isMember(KEY_REQ_TYPE)){
		    std::string req_type = value["req_type"].asString();

		    if(req_type.compare(CREATE_REQ)==0){
		    }else if(req_type.compare(CREATE_RSP) == 0){
		    }else if(req_type.compare(GET_REQ) == 0){
			
			for(auto it = subject_map.begin();it!=subject_map.end();it++){
			    auto sr = it -> second; // Subject Resource
			    
			    if(value.isMember(KEY_URI)){
				
				std::string key = it ->first; //uri

				if(key.compare(value[KEY_URI].asString())==0){
				    std::cout << "*** OCF GET REQ *** :: value :: "<< sr -> value << std::endl; 
				    sendMsg(sr->value,ocf_msgid,PID_OCF);
				}
			    }
			}


		    }else if(req_type.compare(GET_RSP) == 0){
		    }else if(req_type.compare(POST_REQ) == 0){
		    }else if(req_type.compare(POST_RSP) == 0){
		    }
		
		}
	    }else{
		std::cout << "json parse Error"<< data << std::endl;
	    }

	    
	    //std::cout << "data : "<< data<< std::endl;
	  
	}	
	//sleep(1);
    }
}


void initUriFilter(){
    uri_filter.insert("/temperature");
    uri_filter.insert("/humidity");
}

void onPost(const HeaderOptions& /*headerOptions*/, const OCRepresentation& rep, const int eCode)
{


}
// Callback handler on GET request
void onGet(const HeaderOptions& headerOptions, const OCRepresentation& rep,int eCode)
{
    try
    {
	if(eCode == OC_STACK_OK)

	{
	    //std::cout << "GET server request was successful" << std::endl;


	    std::string uri = rep.getUri();
	    std::string id = rep.getHost(); 

	    std::cout << "* OCF GET OK: " << rep.getUri() << " " << id << " if size : " << rep.getResourceInterfaces().size() << " rt size : " << rep.getResourceTypes().size() <<std::endl;
	    for(auto it=rep.begin();it!=rep.end();it++){
		   std::cout <<  it -> attrname() <<std::endl;
	    }
	    
	    // update content resource in subject_map
	    if(subject_map.find(uri) != subject_map.end()){
	
		//std::cout << "ID : " << id << std::endl;
	   	auto subject = subject_map.find(uri)-> second;
		if(subject ->list.size() > 0){
			subject	-> list.at(0) -> updateTime();

			Json::Value value;
			value["req_type"] = GET_RSP;
			Json::Value data;


			for(auto it=rep.begin();it!=rep.end();it++){
	    		    std::string key = it -> attrname();
			    //auto dd = it-> getValue();
			    

			    switch(it->type()){
				case OC::AttributeType::Null:{
				    data[key] = NULL;
				    break;
							     }
				case OC::AttributeType::Integer:
				    int var1;
				    rep.getValue(key,var1);
				    data[key] = var1;
				    break;
				case OC::AttributeType::Double:
				    double var2;
				    rep.getValue(key,var2);
				    data[key] = var2;
				    break;
				case OC::AttributeType::Boolean:
				    bool var3;
				    rep.getValue(key,var3);
				    data[key] = var3;

				    break;
				case OC::AttributeType::String:{
				    std::string var4;
				    rep.getValue(key,var4);
				    data[key] = var4;
				    break;
				} 
				case OC::AttributeType::OCRepresentation:
				case OC::AttributeType::Vector:
				case OC::AttributeType::Binary:
				case OC::AttributeType::OCByteString:
				    data[key] = it -> getValueToString();
				    break;

			    }
			    
			    //std::cout << "@ key : " << key << "  value : " << value<< std::endl;
			}

			value[uri] = data;
			subject -> value = value; 
			//std::cout << "### OCF SubjectResource Value : " << subject -> value << std::endl;
		}
	    
	    }

	    //std::cout << "HOST : " << rep.getHost() << std::endl;
	    // Get resource header options
	}
	else
	{
	    std::cout << "onGET Response error: " << eCode << std::endl;
	    //std::exit(-1);
	}
    }
    catch(std::exception& e)
    {
	std::cout << "Exception: " << e.what() << " in onGet" << std::endl;
    }
}


void create(Json::Value value){

    if(value.isMember(KEY_RESOURCES)){
	Json::Value resources = value[KEY_RESOURCES];

	for(auto v : resources){
	    std::string uri;
	    std::string id;
    
	    if(!v.isMember(KEY_URI)){
	    
		std::cout << "### CREATE FAIL : need 'URI' Key ###  " << std::endl;
		return;
	    }else{
		uri = v["uri"].asString();
	    }

 
	    if(!v.isMember(KEY_ID)){
		std::cout << "### CREATE FAIL : need 'ID' Key ###  " << std::endl;
		return;
	    }else{
		id = v["id"].asString();
	    }




	    //URI Filter
	    //if(uri_filter.find(uri) != uri_filter.end()){
    	
		std::lock_guard<std::mutex> lock{mutex};
		if(subject_map.find(uri) == subject_map.end()){
		    SubjectResource* sr= new SubjectResource();
		    subject_map.insert({uri, sr});
		    std::cout << "### SUBJECT RESORUCE CREATED : " << uri << std::endl;
		}else{
		    std::cout << "### SUBJECT ALREADY CREATED!! : " << uri << std::endl;
		}
	    
    		Json::Value value;
		if(!subject_map.find(uri)->second->isAdded(id)){
		    subject_map.find(uri)->second-> list.push_back(new ContentResource(id,v,true));
		    subject_map.find(uri)->second-> list_sort();
		    std::cout << "*** CONTENT RESORUCE CREATED : " << uri <<" JSON(ID) : " << id << std::endl;	
		    value["req_type"] = CREATE_RSP;
		    value["result"] = true;
	
		}else{
		    std::cout << "*** CONTENT ALREADY CREATED!! : " << uri <<" JSON(ID) : " << id << std::endl;
		    value["req_type"] = CREATE_RSP;
		    value["result"] = false;
		}

		sendMsg(value,etri_msgid,PID_ETRI);		

	    //}
	
	    }
    }

    
}


void foundResource(std::shared_ptr<OCResource> resource){
    //std::string address;
    if(resource){
	//address = resource ->host();
	/*
	std::cout << "\tList of resource endpoints: " << std::endl;
	for(auto &resourceEndpoints : resource->getAllHosts())
	{
	    std::cout << "\t\t" << resourceEndpoints << std::endl;
	}
	*/


	std::string uri = resource->uri();
	//std::cout << "URI : " << resource->uri() << std::endl;
	if(uri_filter.find(uri) != uri_filter.end()){
	    std::lock_guard<std::mutex> lock{mutex};
	    
	    
	    std::string id = resource ->host();	
	    // CREATE SUBJECT RESOURCE // ex) /temperature, /humidity   
	    if(subject_map.find(uri) == subject_map.end()){
		SubjectResource* sr= new SubjectResource();
		subject_map.insert({uri, sr});
		std::cout << "### SUBJECT RESORUCE CREATED : " << uri <<" host(ID) : " << id << std::endl;
	    }

	    
	    // CREATE CONTENT RESOURCE
	    if(!subject_map.find(uri)->second->isAdded(id)){


		bool dc = false;
		
		//check DC Resource Type 
		for(auto &resourceTypes : resource->getResourceTypes())
		{
		    if(resourceTypes.compare(OCF_DC_RT_NAME) == 0){
			dc= true;
		    }
		    std::cout << "\t\t" << resourceTypes << std::endl;
		}

		subject_map.find(uri)->second->	list.push_back(new ContentResource(id,resource,dc));
		subject_map.find(uri)->second-> list_sort();	
		if(dc){
		    std::cout << "*** DC CONTENT RESORUCE CREATED : " << uri <<" host(ID) : " << id << std::endl;
		}else{
		    std::cout << "*** Non DC CONTENT RESORUCE CREATED : " << uri <<" host(ID) : " << id << std::endl;
		}
	    }

	    //QueryParamsMap q;
	    //resource -> get(q,&onGet,OC::QualityOfService::LowQos);
	}
    }
}


void * resourceScan(void * p){
    // start ocf resource scan
    std::ostringstream requestURI;
    requestURI << OC_RSRVD_WELL_KNOWN_URI; //<< "?rt=" << SCAN_RESOURCE_TYPE;
    
    while(1){
	std::cout << "************ OCF resource Scan *************\n";
	OCPlatform::findResource("",requestURI.str(),CT_DEFAULT,&foundResource);
	sleep(OCF_SCAN_PERIOD);
    }
}

Json::FastWriter fw;
void sendMsg(Json::Value value,long msg_id ,long dst_pid){
    msg_data send_msg;
    send_msg.mtype = dst_pid;
    std::string out = fw.write(value);
    strcpy(send_msg.data,out.c_str());
    int msg_size = sizeof(send_msg) - sizeof(send_msg.mtype);
    int rtn = msgsnd(msg_id,&send_msg,msg_size,0);
}


void * collect(void * p){
    while(1){
	sleep(COLLECT_PERIOD);

	for(auto it = subject_map.begin();it!=subject_map.end();it++){
	    auto sr = it -> second; // Subject Resource

	    //std::cout << " DEBUG :: URI" << it -> first << "// list.size() = " << sr -> list.size() << std::endl;
	    if(sr -> list.size() >0){
		// get the first resource in the list ;
		auto cr = sr -> list.at(0); // get the first resource in the list;



		bool init = false;
		if(cr -> timestamp == 0 ){
		    // update timestamp for init
		    cr -> updateTime();

		    // flag for set dc resource ...
		    init = true;
		}else{

		    // called when cr endpoint is not response
		    //
		    //std::cout << now() << " ::  " << cr-> timestamp << " :: " << COLLECT_PERIOD * 2  << std::endl;
		    if(now() - cr -> timestamp > (COLLECT_PERIOD * 2)){

			//sync
			std::lock_guard<std::mutex> lock{mutex};
		
			// delete unresponsive CR(ContentResource) 
			delete cr;
			sr -> list.erase(sr->list.begin());
			std::cout << "* DELETE Content Resource (RESPONSE_TIME_OUT) URI :" << it-> first << ",  list.size() = " << sr -> list.size() << std::endl;

			// replace with the first cr resource (if the list size is more than one)
			if(sr -> list.size() > 0 ){
			    cr = sr -> list.at(0); 
			    cr -> updateTime();
			}else{
			    cr = NULL;
			}
			
		    }

		}

		if(cr){
		    if(cr -> device_type == 1){// OCF
			if(init){
			    OCRepresentation rep; 
			    QueryParamsMap q;
			    rep.setValue("dc", true);
			    cr -> ocf_resource -> post(rep,q,&onPost,OC::QualityOfService::LowQos);
			}else{
			    //GET REQUEST 
			    QueryParamsMap q;
			    //cr -> ocf_resource -> get(q,&onGet,OC::QualityOfService::HighQos);
			    cr -> ocf_resource -> get(q,&onGet,OC::QualityOfService::LowQos);
			}
		    }else{ // ETRI MCMSG/DCMSG

			Json::Value value;
			if(init){
			    Json::Value data;
			    data["dc"] = true;
			    value["req_type"] = POST_REQ;
			    value[ cr -> etri_resource["uri"].asString()] = data;
			    sendMsg(value,etri_msgid,PID_ETRI);

			}else{
			    value["req_type"] = GET_REQ;
			    value["uri"] = cr -> etri_resource["uri"].asString();
			    value["id"] = cr -> etri_resource["id"].asString();
			    sendMsg(value,etri_msgid,PID_ETRI);
			}
				
		    }
		}
		
	    }
	}

    }
}


Agent::Agent(void)
{
    std::cout << "Running: Agent constructor" << std::endl;
    initUriFilter();
    startScan();
    startCollect(); 
    startRecvQueue();
}

Agent::~Agent(void)
{
    std::cout << "Running: Agent destructor" << std::endl;
}

void startScan(){
    pthread_t id;
    pthread_create(&id,NULL,resourceScan,(void *)0);      
}

void startRecvQueue(){
    pthread_t threadId;
    pthread_create(&threadId,NULL,recv_queue,(void *)0);
 
}

void startCollect(){
    pthread_t id;
    pthread_create(&id,NULL,collect,(void *)0);      
}
