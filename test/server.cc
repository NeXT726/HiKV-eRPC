#include "common.h"

class ServerContext {
  public:
    erpc::Rpc<erpc::CTransport> *rpc = nullptr;

    erpc::MsgBuffer reqMsgBuf;
    erpc::MsgBuffer respMsgBuf;

    int thread_id;

    int numFinish;
    
    size_t startTsc;
    size_t endTsc;
    erpc::Latency latency; 

    size_t valueSize[numBatch];
};

DB* _db;
size_t numServer = 0;

void sm_handler(int, erpc::SmEventType, erpc::SmErrType, void *) {}


void insert_req_handler(erpc::ReqHandle *req_handle, void *_context) {
  auto *c = static_cast<ServerContext *>(_context);
  c->reqMsgBuf = *req_handle->get_req_msgbuf();
  uint8_t *recv_msg = c->reqMsgBuf.buf;
  for (int i=0; i<numBatch; i++){
    c->valueSize[i] = *recv_msg - keyMsgSize;
    recv_msg++;
  }

  c->startTsc = erpc::rdtsc();

  for(int i=0; i<numBatch; i++){
    char *key = reinterpret_cast<char *>(recv_msg);
    char *value = reinterpret_cast<char *>(key + keyMsgSize);
    dbKey __dbkey(key, keyMsgSize); // make a dbkey copy
    dbValue __dbvalue(value, c->valueSize[i]); // make a dbkey copy
    _db->Put(__dbkey, __dbvalue);
    recv_msg += keyMsgSize + c->valueSize[i];
  }

  c->endTsc = erpc::rdtsc();
  double reqLat =erpc::to_usec(c->endTsc - c->startTsc, c->rpc->get_freq_ghz());
  c->latency.update(static_cast<size_t>(reqLat * 10.0));


  auto &resp = req_handle->pre_resp_msgbuf;  

  c->rpc->enqueue_response(req_handle, &resp);
}

void search_req_handler(erpc::ReqHandle *req_handle, void *_context){
  auto *c = static_cast<ServerContext *>(_context);
  c->reqMsgBuf = *req_handle->get_req_msgbuf();
  req_handle->dyn_resp_msgbuf = c->rpc->alloc_msg_buffer_or_die(valueMsgSize * numBatch + numBatch);
  //c->rpc->resize_msg_buffer(&req_handle->dyn_resp_msgbuf, valueMsgSize*numBatch + numBatch);
  uint8_t* response_size = req_handle->dyn_resp_msgbuf.buf;
  uint8_t* response_value = response_size + numBatch;
  size_t total_size = numBatch;

  for(int i=0; i<numBatch; i++){
    char *key = reinterpret_cast<char *>(c->reqMsgBuf.buf + numBatch + i*keyMsgSize);

    dbKey __dbkey(key, keyMsgSize);
    dbValue __dbvalue;

    Status _status = _db->Get(__dbkey, __dbvalue);

    *(response_size) = __dbvalue.Length();
    memcpy(response_value, __dbvalue.Data(), __dbvalue.Length());
    response_value += __dbvalue.Length();
    total_size += __dbvalue.Length();
    response_size++;
  }

  c->rpc->resize_msg_buffer(&req_handle->dyn_resp_msgbuf, total_size);

  c->rpc->enqueue_response(req_handle, &req_handle->dyn_resp_msgbuf);
}

void server_func(erpc::Nexus *nexus, size_t thread_id){
  ServerContext c;
  c.thread_id = thread_id;
  erpc::Rpc<erpc::CTransport> rpc(nexus, static_cast<void *>(&c), thread_id, sm_handler);
  c.rpc = &rpc;

  c.rpc->run_event_loop(kServerEvLoopMs);

  double lat = (c.latency.avg()/10.0);
  printf("%zu\t%2f\t\n", thread_id, lat);
  //printf("%zu: %d %d %d\n", thread_id, c.latency.perc(.1), c.latency.perc(.99), c.latency.perc(.999));
}

int get_param(int argc, char *argv[])
{
  while (1)
	{
		int c;
		c = getopt(argc, argv, "n:");
		if (c == -1) break;
		switch (c)
		{
    case 'n':
      numServer =  strtoul(optarg, nullptr, 0) - 1;
      if(numServer > numMechine){
        printf("wrong input of mechine number\n");
        return 0;
      }
		default:
			return 1;
		}
	}
}

int main(int argc, char *argv[]) 
{
  get_param(argc, argv);

  Options _options;
  strcpy(_options.pmem_file_path, "/home/pmem0/hikv");
  _options.pmem_file_size = 8UL * 1024 * 1024 * 1024;
  _options.store_size = 5L * 1024 * 1024 * 1024;
  _options.index_size = 2L * 1024 * 1024 * 1024;
  _options.num_server_threads = numThreadServer;
  _options.num_backend_threads = numThreadBack;
  _options.only_test_index = false;

  _db = new HybridIndex(_options);

  std::string server_uri = kServerHostname[numServer] + ":" + std::to_string(kUDPPort);
  erpc::Nexus nexus(server_uri, 0, 0);
  nexus.register_req_func(kInsertReqType, insert_req_handler);
  nexus.register_req_func(kSearchReqType, search_req_handler);

  std::vector<std::thread> threads(numThreadServer);
  for(size_t i = 0; i < numThreadServer; i++){
    threads[i] = std::thread(server_func,
                             &nexus, i);  
  }

  for(size_t i = 0; i< numThreadServer; i++) threads[i].join();  
}

