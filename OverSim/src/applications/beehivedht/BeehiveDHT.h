//
// Copyright (C) 2007 Institut fuer Telematik, Universitaet Karlsruhe (TH)
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//

/**
 * @file BeehiveDHT.h
 * @author Gregoire Menuel, Ingmar Baumgart
 */

#ifndef __BEEHIVEDHT_H_
#define __BEEHIVEDHT_H_

#include <omnetpp.h>

#include <OverlayKey.h>
#include <SHA1.h>
#include <CommonMessages_m.h>

#include "BeehiveDHTMessage_m.h"
#include "BeehiveDHTDataStorage.h"

#include "BaseApp.h"
#include <RpcMacros.h>

/**
 * A Distributed Hash Table (BeehiveDHT) for KBR protocols
 *
 * A Distributed Hash Table (BeehiveDHT) for KBR protocols
 */
class BeehiveDHT : public BaseApp
{
public:
    BeehiveDHT();
    virtual ~BeehiveDHT();

private:
    enum PendingRpcsStates {
        INIT = 0,
        LOOKUP_STARTED = 1,
        GET_HASH_SENT = 2,
        GET_VALUE_SENT = 3,
        PUT_SENT = 4
    };

    class PendingRpcsEntry
    {
    public:
        PendingRpcsEntry()
        {
            getCallMsg = NULL;
            putCallMsg = NULL;
            state = INIT;
            hashVector = NULL;
            numSent = 0;
            numAvailableReplica = 0;
            numFailed = 0;
            numResponses = 0;
        };

        DHTgetCAPICall* getCallMsg;
        DHTputCAPICall* putCallMsg;
        PendingRpcsStates state;
        NodeVector replica;
        NodeVector* hashVector;
        std::map<BinaryValue, NodeVector> hashes;
        int numSent;
        int numAvailableReplica;
        int numFailed;
        int numResponses;
    };

    friend std::ostream& operator<<(std::ostream& Stream,
                                            const PendingRpcsEntry& entry);

    void initializeApp(int stage);
    void finishApp();
    void handleTimerEvent(cMessage* msg);
    void handleReplicateTimerExpired(cMessage* msg);

    bool handleRpcCall(BaseCallMessage* msg);
    void handleRpcResponse(BaseResponseMessage* msg, cPolymorphic *context,
                           int rpcId, simtime_t rtt);
    void handleRpcTimeout(BaseCallMessage* msg, const TransportAddress& dest,
                          cPolymorphic* context, int rpcId,
                          const OverlayKey& destKey);
    void handlePutRequest(BeehiveDHTPutCall* dhtMsg);
    void handleGetRequest(BeehiveDHTGetCall* dhtMsg);
    void handlePutResponse(BeehiveDHTPutResponse* dhtMsg, int rpcId);
    void handleGetResponse(BeehiveDHTGetResponse* dhtMsg, int rpcId);
    void handlePutCAPIRequest(DHTputCAPICall* capiPutMsg);
    void handleGetCAPIRequest(DHTgetCAPICall* capiPutMsg);
    void handleDumpDhtRequest(DHTdumpCall* call);
    void update(const NodeHandle& node, bool joined);
    void handleLookupResponse(LookupResponse* lookupMsg, int rpcId);
    void sendMaintenancePutCall(const TransportAddress& dest,
                                const OverlayKey& key,
                                const BeehiveDHTDataEntry& entry);

    void handleReplicateResponse(BeehiveReplicateResponse* replicateResponse, int rpcId);
    void handleReplicateRequest(BeehiveReplicateCall* replicateRequest);
    void handleUpdateRoutingResponse(BeehiveUpdateRoutingResponse* updateRoutingResponse, int rpcId);



    int resultValuesBitLength(BeehiveDHTGetResponse* msg);

    uint numReplica;
    int numGetRequests;
    double ratioIdentical;
    double maintenanceMessages;
    double normalMessages;
    double numBytesMaintenance;
    double numBytesNormal;
    double replicateDelay; /**< replicate interval (secs) */

    //stats
    int replicateCount; /**< */
    int replicateBytesSent; /**< */

    typedef std::map<uint32_t, PendingRpcsEntry> PendingRpcs;
    PendingRpcs pendingRpcs; /**< a map of all pending RPC operations */

    // module references
    BeehiveDHTDataStorage* dataStorage; /**< pointer to the BeehiveDHT data storage */

    // timers
    cMessage* replicate_timer; /**< */
};

#endif
