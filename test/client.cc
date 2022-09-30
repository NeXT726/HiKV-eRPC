#include "common.h"

class ClientContext {
  public:
    int session[numMechine];
    erpc::Rpc<erpc::CTransport> *rpc = nullptr;

    char* reqBuff[numBatch];
    size_t reqBuffSize[numBatch];
    int reqBatch;

    erpc::MsgBuffer reqMsgBuf;
    erpc::MsgBuffer respMsgBuf;
    size_t oneMsgSize;
    size_t msgSize;

    size_t type;

    size_t numReq = 0;
    size_t numSucc = 0;

    size_t thread_id;
    uint64_t seed;

    size_t startTsc;
    size_t endTsc;
    erpc::Latency latency;

    struct timespec start;
    struct timespec end;

    bool complete = 0;
};

class PrintOption {
  public:
    bool typeCal = 1;
    bool typeBW = 0;
    bool allThread = 0;

    uint64_t putReq = 0;
    uint64_t getReq = 0;
    double putIops = 0;
    double getIops = 0;
    double bandWide = 0;
    double putLat = 0;
    double getLat = 0;

    struct timespec putStart;
    struct timespec putEnd;
    struct timespec getStart;
    struct timespec getEnd;    
};

PrintOption op;

void sm_handler(int, erpc::SmEventType, erpc::SmErrType, void *) {}

void kv_cont_func(void *, void *);
void kv_batch_req(ClientContext &c);

int kv_send_req(ClientContext &c, char* buf, size_t bufSize)
{
  c.reqBuff[c.reqBatch] = buf;
  c.reqBuffSize[c.reqBatch] = bufSize;
  c.reqBatch++;
  if(c.reqBatch == numBatch) {
    c.complete = 0;
    kv_batch_req(c);
    while(1){
      c.rpc->run_event_loop_once();
      if(c.complete==1) break;
    }
    c.reqBatch = 0;   
  }
  else if(c.reqBatch > numBatch) printf("Wrong Batch num\n");
}

void kv_batch_req(ClientContext &c)
{
  //memset(c.reqMsgBuf.buf, 0, c.msgSize);
  c.startTsc = erpc::rdtsc();
  
  uint8_t *destion = reinterpret_cast<uint8_t *>(c.reqMsgBuf.buf);
  for (int i=0; i<numBatch; i++){
    *destion = c.reqBuffSize[i];
    destion++;
  }

  size_t totalBuffSize = numBatch;
  for(int i=0; i<numBatch; i++){
    memcpy(destion, c.reqBuff[i], c.reqBuffSize[i]);
    destion += c.reqBuffSize[i];
    free(c.reqBuff[i]);
    totalBuffSize += c.reqBuffSize[i];
  }

  c.rpc->resize_msg_buffer(&c.reqMsgBuf, totalBuffSize);
  size_t mNum = (c.seed/numBatch)%numMechine;
//start record lat
  c.startTsc = erpc::rdtsc();
//send request
  c.rpc->enqueue_request(c.session[mNum], c.type, &c.reqMsgBuf, &c.respMsgBuf, kv_cont_func, nullptr);
  //std::this_thread::yield();
}


void kv_cont_func(void *context, void *)
{
//check search result
  auto *c = static_cast<ClientContext *>(context);
  if(c->type==kSearchReqType){
    if(c->seed == *(uint64_t*)(c->respMsgBuf.buf + numBatch)) c->numSucc = c->numSucc + numBatch;
    else printf("get wrong data\n");
  }
  else c->numSucc = c->numSucc + numBatch;
//end record lat
  c->endTsc = erpc::rdtsc();
//updata lat
  double reqLat = erpc::to_usec(c->endTsc - c->startTsc, c->rpc->get_freq_ghz());
  c->latency.update(static_cast<size_t>(reqLat * 10.0));
//change seed for next request
  c->seed = c->seed + numBatch;
  c->numReq = c->numReq + numBatch;
  c->complete = 1;
}

void cal_print(ClientContext &c);
void client_req(ClientContext &c, size_t type, size_t thread_id){
//start record thread time
  clock_gettime(CLOCK_REALTIME, &c.start);
//start send request
  c.seed = thread_id * numInsert;
  c.type = type;

  size_t numALLReq;
  switch (type) 
  {
    case kInsertReqType:
      numALLReq = numInsert;
      break;
    case kSearchReqType:
      numALLReq = numSearch;
      break;
    default:
      printf("wrong input req type:%d\n", type);
      break;
  }
  size_t sendSize;
  while(c.numReq < numALLReq) {
    if(type==kInsertReqType) sendSize = keyMsgSize + valueMsgSize; //定长value
    //if(type==kInsertReqType) sendSize = keyMsgSize + ((c.numReq/numBatch)%4+1)*16; //变长value
    else if(type==kSearchReqType) sendSize = keyMsgSize;

    char *sendBUff = (char *)malloc(sendSize);
    *((uint64_t *)sendBUff) = c.seed + c.reqBatch;
    if(type==kInsertReqType) *((uint64_t *)(sendBUff+keyMsgSize)) = c.seed + c.reqBatch;

    kv_send_req(c, sendBUff, sendSize);
  }
  clock_gettime(CLOCK_REALTIME, &c.end);  /* end record threat time */
  c.complete = 1;

  cal_print(c);
}

void cal_print(ClientContext &c)
{
//cal iops
  uint64_t time = (c.end.tv_sec-c.start.tv_sec)*1000000000 + (c.end.tv_nsec - c.start.tv_nsec);
  double iops = (c.numReq*1000000)/time;
  if(c.type == kInsertReqType) op.putIops = op.putIops + iops;
  else if(c.type == kSearchReqType) op.getIops = op.getIops + iops;
  else ;
  double lat = (c.latency.avg()/10.0)/numBatch;
  if(c.type==kInsertReqType) op.putLat = op.putLat + lat;
  else if(c.type==kSearchReqType) op.getLat = op.getLat + lat;
  else ;
//print all thread message
  if(op.allThread){
    char typeChar[16];
    if(c.type==kInsertReqType) memcpy(typeChar, "insert", 16);
    else if(c.type==kSearchReqType) memcpy(typeChar, "search", 16);
    else ;

    printf("[%s][%zu][iops:%2f][lat:%2f][%d/%d]\n",
            typeChar, c.thread_id, iops, lat, c.numSucc, c.numReq);  
    //printf("\ntime:\tsec:%d\tnsec:%d\n", end.tv_sec-start.tv_sec, end.tv_nsec-start.tv_nsec);
    //printf("%d\n",c.numReq);
    //printf("avg:%.2f\n", c.latency.avg());
    //printf("%zu: %d %d %d\n", thread_id,
      //c.latency.perc(.5), c.latency.perc(.99), c.latency.perc(.999));
  }
}

void client_func(erpc::Nexus *nexus, size_t thread_id, size_t opType){
  srand(thread_id);
  
  ClientContext c;
  c.thread_id = thread_id;
  erpc::Rpc<erpc::CTransport> rpc(nexus, static_cast<void *>(&c), thread_id, sm_handler);
  c.rpc = &rpc;
  c.oneMsgSize = (opType==kInsertReqType) ? (keyMsgSize + valueMsgSize) : 
              (opType==kSearchReqType) ? (keyMsgSize) :
              0;
  c.msgSize = c.oneMsgSize * numBatch;

  for(size_t j = 0; j<numMechine; j++){
   std::string server_uri = kServerHostname[j] + ":" + std::to_string(kUDPPort);    
   //printf("%d\t\n", j);   
   size_t i = thread_id % numThreadServer;
   //printf("\n%s\n", server_uri.c_str());
   int session_num = rpc.create_session(server_uri, i);
   while (!rpc.is_connected(session_num)) rpc.run_event_loop_once();
   c.session[j] = session_num;
  }

  c.reqMsgBuf = rpc.alloc_msg_buffer_or_die(c.msgSize + numBatch);
  c.respMsgBuf = rpc.alloc_msg_buffer_or_die(valueMsgSize * numBatch + numBatch);

  client_req(c, opType, thread_id);

  if(opType==kInsertReqType) op.putReq = op.putReq + c.numReq;
  else if(opType==kSearchReqType) op.getReq = op.getReq + c.numReq;
  else ;
}

int get_param(int argc, char *argv[])
{
  while (1)
	{
		int c;
		c = getopt(argc, argv, "t:abc");
		if (c == -1) break;
		switch (c)
		{
    case 'a':
      op.allThread = 1;
      break;
		case 'b':
			op.typeBW = 1;
			break;
		case 'c':
			op.typeCal = 1;
			break;
    case 't':
      numThreadClient =  strtoul(optarg, nullptr, 0);
      numSearch = 5000000/numThreadClient;
      numInsert = 5000000/numThreadClient;
      if(numThreadClient < 1){
        printf("wrong input of client thread\n");
        return 0;
      }

		default:
			return 1;
		}
	}
}

void result_print()
{
  double allPutIops = op.putIops/10;
  double allGetIops = op.getIops/10;
  double allBandWide = 0;
  double avgPutLat = op.putLat/numThreadClient;
  double avgGetLat = op.getLat/numThreadClient;

  printf("%s", kClientHostname.c_str());  
  printf("final result\n");
  printf("[put]\t%2f\t%2f\t%2f\n", allPutIops, avgPutLat, (allPutIops*(keyMsgSize+valueMsgSize)*8*1000)/(1024*1024));
  printf("[get]\t%2f\t%2f\t%2f\n", allGetIops, avgGetLat, (allGetIops*(valueMsgSize)*8*1000)/(1024*1024)); 

  if(op.typeCal){
    uint64_t time = (op.putEnd.tv_sec-op.putStart.tv_sec)*1000000000 + (op.putEnd.tv_nsec - op.putStart.tv_nsec);
    allPutIops = (op.putReq*100000)/time;
    time = (op.getEnd.tv_sec-op.getStart.tv_sec)*1000000000 + (op.getEnd.tv_nsec - op.getStart.tv_nsec);
    allGetIops = (op.getReq*100000)/time;
    printf("%s\n", kClientHostname.c_str());
    printf("All Calculate result\n");
    printf("[put]\t%2f\t%2f\t%2f\n", allPutIops, avgPutLat, (allPutIops*(keyMsgSize+valueMsgSize)*8*1000)/(1024*1024));
    printf("[get]\t%2f\t%2f\t%2f\n", allGetIops, avgGetLat, (allGetIops*(valueMsgSize)*8*1000)/(1024*1024)); 
  } 
}

int main(int argc, char *argv[]) 
{
  get_param(argc, argv);

  std::string client_uri = kClientHostname + ":" + std::to_string(kUDPPort);
  erpc::Nexus nexus(client_uri, 0, 0);
//put
  clock_gettime(CLOCK_REALTIME, &op.putStart);
  std::vector<std::thread> threads(numThreadClient);
  for(size_t i = 0; i < numThreadClient; i++){
    threads[i] = std::thread(client_func,
                             &nexus, i, kInsertReqType);  
  }
  for(size_t i = 0; i< numThreadClient; i++) threads[i].join();
  clock_gettime(CLOCK_REALTIME, &op.putEnd);
//get
  clock_gettime(CLOCK_REALTIME, &op.getStart);
  for(size_t i = 0; i < numThreadClient; i++){
    threads[i] = std::thread(client_func,
                             &nexus, i, kSearchReqType);  
  }
  for(size_t i = 0; i< numThreadClient; i++) threads[i].join();
  clock_gettime(CLOCK_REALTIME, &op.getEnd);
//print final result
  result_print();
}

