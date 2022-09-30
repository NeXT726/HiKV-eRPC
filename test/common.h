#include <stdio.h>
#include "rpc.h"

#include <getopt.h>

#include "db.hpp"
#include "hybrid_index.hpp"
#include "in-memory_index.hpp"
#include "options.hpp"
#include <thread>

#define random(x) (rand()%x)

static const std::string kServerHostname[4] = {"10.0.0.37", "10.0.0.38"};
static const std::string kClientHostname = "10.0.0.37";

static constexpr size_t numMechine = 1;

static constexpr uint16_t kUDPPort = 31850;

static constexpr size_t kInsertReqType = 1;
static constexpr size_t kSearchReqType = 2;

static constexpr size_t keyMsgSize = 16;
static constexpr size_t valueMsgSize = 64;
static constexpr size_t numBatch = 16;

size_t numThreadClient = 1;
static constexpr size_t numThreadServer = 8;
static constexpr size_t numThreadBack = 8;


size_t numInsert = 0;
size_t numSearch = 0;

static constexpr size_t kAppEvLoopMs = 1000;
static constexpr size_t kServerEvLoopMs = 10000000;
