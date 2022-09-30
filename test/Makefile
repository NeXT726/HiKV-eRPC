LIBS = -lpthread -lnuma -ldl -lrt -luuid -lm -libverbs 

PMEMKV_LINK_FLAGS := -Wl,--no-whole-archive -L../PmemKV2/build -lpmemkv2 

PMDK_LINK_FLAGS := -Wl,--no-whole-archive -L../PmemKV2/third-party/pmdk -lpmem 

TBB_LINK_FLAGS := -Wl,--no-whole-archive -L../PmemKV2/third-party/tbb -ltbb 

ERPC_LINK_FLAGS := -Wl,--no-whole-archive -L../eRPC/build -lerpc 

INC := -I../PmemKV2/include -I../eRPC/src -I../PmemKV2/lib 

infiniband: clean client server 

client: 
	g++ -std=c++11 -o client client.cc -DERPC_INFINIBAND=true -DROCE=true $(INC) $(PMEMKV_LINK_FLAGS) $(PMDK_LINK_FLAGS) $(TBB_LINK_FLAGS) $(ERPC_LINK_FLAGS) $(LIBS) 

server: 
	g++ -std=c++11 -o server server.cc -DERPC_INFINIBAND=true -DROCE=true $(INC) $(PMEMKV_LINK_FLAGS) $(PMDK_LINK_FLAGS) $(TBB_LINK_FLAGS) $(ERPC_LINK_FLAGS) $(LIBS) 

clean: 
	rm -rf server client 

export:  
	export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/root/djl/code/PmemKV2/third-party/tbb/:/root/djl/code/PmemKV2/third-party/pmdk
