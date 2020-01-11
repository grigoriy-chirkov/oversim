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
 * @file BeehiveDHT.cc
 * @author Gregoire Menuel, Ingmar Baumgart
 */

#include <IPAddressResolver.h>

#include "BeehiveDHT.h"
#include <Beehive.h>
#include <BeehiveSuccessorList.h>
#include <BeehiveFingerTable.h>

#include <RpcMacros.h>
#include <BaseRpc.h>
#include <GlobalStatistics.h>

#include <typeinfo>

Define_Module(BeehiveDHT);

using namespace std;

BeehiveDHT::BeehiveDHT()
{
    dataStorage = NULL;
    replicate_timer = NULL;
}

BeehiveDHT::~BeehiveDHT()
{
    PendingRpcs::iterator it;

    for (it = pendingRpcs.begin(); it != pendingRpcs.end(); it++) {
        delete(it->second.putCallMsg);
        delete(it->second.getCallMsg);
    }

    pendingRpcs.clear();

    if (dataStorage != NULL) {
        dataStorage->clear();
    }

    cancelAndDelete(replicate_timer);
}

void BeehiveDHT::initializeApp(int stage)
{
    if (stage != MIN_STAGE_APP)
        return;

    dataStorage = check_and_cast<BeehiveDHTDataStorage*>
                      (getParentModule()->getSubmodule("dhtDataStorage"));

    numReplica = par("numReplica");
    numGetRequests = par("numGetRequests");
    ratioIdentical = par("ratioIdentical");
    replicateDelay = par("replicateDelay");

    if ((int)numReplica > overlay->getMaxNumSiblings()) {
        opp_error("BeehiveDHT::initialize(): numReplica bigger than what this "
                  "overlay can handle (%d)", overlay->getMaxNumSiblings());
    }

    replicate_timer = new cMessage("replicate_timer");
    scheduleAt(simTime() + uniform(0, replicateDelay), replicate_timer);
//    uniform(0, replicateDelay);

    maintenanceMessages = 0;
    normalMessages = 0;
    numBytesMaintenance = 0;
    numBytesNormal = 0;
    WATCH(maintenanceMessages);
    WATCH(normalMessages);
    WATCH(numBytesNormal);
    WATCH(numBytesMaintenance);
    WATCH_MAP(pendingRpcs);
}

void BeehiveDHT::handleTimerEvent(cMessage* msg)
{

    if (msg == replicate_timer) {
        handleReplicateTimerExpired(msg);
        return;
    }

    BeehiveDHTTtlTimer* msg_timer = dynamic_cast<BeehiveDHTTtlTimer*> (msg);

    if (msg_timer) {
        EV << "[BeehiveDHT::handleTimerEvent()]\n"
           << "    received timer ttl, key: "
           << msg_timer->getKey().toString(16)
           << "\n (overlay->getThisNode().getKey() = "
           << overlay->getThisNode().getKey().toString(16) << ")"
           << endl;

        dataStorage->removeData(msg_timer->getKey(), msg_timer->getKind(),
                                msg_timer->getId());
    }
}

bool BeehiveDHT::handleRpcCall(BaseCallMessage* msg)
{
    RPC_SWITCH_START(msg)
        // RPCs between nodes
        RPC_DELEGATE(BeehiveDHTPut, handlePutRequest);
        RPC_DELEGATE(BeehiveDHTGet, handleGetRequest);
        RPC_DELEGATE(BeehiveReplicate, handleReplicateRequest);
        // internal RPCs
        RPC_DELEGATE(DHTputCAPI, handlePutCAPIRequest);
        RPC_DELEGATE(DHTgetCAPI, handleGetCAPIRequest);
        RPC_DELEGATE(DHTdump, handleDumpDhtRequest);
    RPC_SWITCH_END( )

    return RPC_HANDLED;
}

void BeehiveDHT::handleRpcResponse(BaseResponseMessage* msg, cPolymorphic* context,
                            int rpcId, simtime_t rtt)
{
    RPC_SWITCH_START(msg)
        RPC_ON_RESPONSE(BeehiveDHTPut){
        handlePutResponse(_BeehiveDHTPutResponse, rpcId);
        EV << "[BeehiveDHT::handleRpcResponse()]\n"
           << "    BeehiveDHT Put RPC Response received: id=" << rpcId
           << " msg=" << *_BeehiveDHTPutResponse << " rtt=" << rtt
           << endl;
        break;
    }
    RPC_ON_RESPONSE(BeehiveDHTGet) {
        handleGetResponse(_BeehiveDHTGetResponse, rpcId);
        EV << "[BeehiveDHT::handleRpcResponse()]\n"
           << "    BeehiveDHT Get RPC Response received: id=" << rpcId
           << " msg=" << *_BeehiveDHTGetResponse << " rtt=" << rtt
           << endl;
        break;
    }
    RPC_ON_RESPONSE(Lookup) {
        handleLookupResponse(_LookupResponse, rpcId);
        EV << "[BeehiveDHT::handleRpcResponse()]\n"
           << "    Lookup RPC Response received: id=" << rpcId
           << " msg=" << *_LookupResponse << " rtt=" << rtt
           << endl;
        break;
    }
    RPC_ON_RESPONSE(BeehiveUpdateRouting) {
        handleUpdateRoutingResponse(_BeehiveUpdateRoutingResponse, rpcId);
        EV << "[BeehiveDHT::handleRpcResponse()]\n"
           << "    BeehiveDHT UpdateRouting RPC Response received: id=" << rpcId
           << " msg=" << *_BeehiveUpdateRoutingResponse << " rtt=" << rtt
           << endl;
        break;
    }
    RPC_ON_RESPONSE(BeehiveReplicate) {
        handleReplicateResponse(_BeehiveReplicateResponse, rpcId);
        EV << "[BeehiveDHT::handleRpcResponse()]\n"
           << "    BeehiveDHT Replicate RPC Response received: id=" << rpcId
           << " msg=" << *_BeehiveReplicateResponse << " rtt=" << rtt
           << endl;
        break;
    }
    RPC_SWITCH_END()
}

void BeehiveDHT::handleRpcTimeout(BaseCallMessage* msg, const TransportAddress& dest,
                           cPolymorphic* context, int rpcId,
                           const OverlayKey& destKey)
{
    RPC_SWITCH_START(msg)
    RPC_ON_CALL(BeehiveDHTPut){
        EV << "[BeehiveDHT::handleRpcResponse()]\n"
           << "    BeehiveDHTPut Timeout"
           << endl;

        PendingRpcs::iterator it = pendingRpcs.find(rpcId);

        if (it == pendingRpcs.end()) // unknown request
            return;

        it->second.numFailed++;

        if (it->second.numFailed / (double)it->second.numSent >= 0.5) {
            DHTputCAPIResponse* capiPutRespMsg = new DHTputCAPIResponse();
            capiPutRespMsg->setIsSuccess(false);
            sendRpcResponse(it->second.putCallMsg, capiPutRespMsg);
            //cout << "timeout 1" << endl;
            pendingRpcs.erase(rpcId);
        }

        break;
    }
    RPC_ON_CALL(BeehiveDHTGet) {
        EV << "[BeehiveDHT::handleRpcResponse()]\n"
           << "    BeehiveDHTGet Timeout"
           << endl;

        PendingRpcs::iterator it = pendingRpcs.find(rpcId);

        if (it == pendingRpcs.end()) { // unknown request
            return;
        }

        if (it->second.state == GET_VALUE_SENT) {
            // we have sent a 'real' get request
            // ask anyone else, if possible
            if ((it->second.hashVector != NULL)
                && (it->second.hashVector->size() > 0)) {

                BeehiveDHTGetCall* getCall = new BeehiveDHTGetCall();
                getCall->setKey(_BeehiveDHTGetCall->getKey());
                getCall->setKind(_BeehiveDHTGetCall->getKind());
                getCall->setId(_BeehiveDHTGetCall->getId());
                getCall->setIsHash(false);
                getCall->setBitLength(GETCALL_L(getCall));
                RECORD_STATS(normalMessages++;
                             numBytesNormal += getCall->getByteLength());

                sendRouteRpcCall(TIER1_COMP, it->second.hashVector->back(),
                                 getCall, NULL, DEFAULT_ROUTING, -1, 0, rpcId);
                it->second.hashVector->pop_back();
            } else {
                // no one else
                DHTgetCAPIResponse* capiGetRespMsg = new DHTgetCAPIResponse();
                capiGetRespMsg->setIsSuccess(false);
                sendRpcResponse(it->second.getCallMsg,
                                capiGetRespMsg);
                //cout << "DHT: GET failed: timeout (no one else)" << endl;
                pendingRpcs.erase(rpcId);
                return;
            }
        } else {
            // timeout while waiting for hashes
            // try to ask another one of the replica list for the hash
            if (it->second.replica.size() > 0) {
                BeehiveDHTGetCall* getCall = new BeehiveDHTGetCall();
                getCall->setKey(_BeehiveDHTGetCall->getKey());
                getCall->setKind(_BeehiveDHTGetCall->getKind());
                getCall->setId(_BeehiveDHTGetCall->getId());
                getCall->setIsHash(true);
                getCall->setBitLength(GETCALL_L(getCall));

                RECORD_STATS(normalMessages++;
                             numBytesNormal += getCall->getByteLength());

                sendRouteRpcCall(TIER1_COMP, it->second.replica.back(),
                                 getCall, NULL, DEFAULT_ROUTING, -1, 0,
                                 rpcId);
                it->second.replica.pop_back();
            } else {
                // no one else to ask, see what we can do with what we have
                if (it->second.numResponses > 0) {
                    unsigned int maxCount = 0;
                    NodeVector* hashVector = NULL;
                    std::map<BinaryValue, NodeVector>::iterator itHashes;
                    for (itHashes = it->second.hashes.begin();
                         itHashes != it->second.hashes.end(); itHashes++) {

                        if (itHashes->second.size() > maxCount) {
                            maxCount = itHashes->second.size();
                            hashVector = &(itHashes->second);
                        }
                    }

                    // since it makes no difference for us, if we
                    // return a invalid result or return nothing,
                    // we simply return the value with the highest probability
                    it->second.hashVector = hashVector;

                }

                if ((it->second.hashVector != NULL)
                     && (it->second.hashVector->size() > 0)) {

                    BeehiveDHTGetCall* getCall = new BeehiveDHTGetCall();
                    getCall->setKey(_BeehiveDHTGetCall->getKey());
                    getCall->setKind(_BeehiveDHTGetCall->getKind());
                    getCall->setId(_BeehiveDHTGetCall->getId());
                    getCall->setIsHash(false);
                    getCall->setBitLength(GETCALL_L(getCall));
                    RECORD_STATS(normalMessages++;
                                 numBytesNormal += getCall->getByteLength());
                    sendRouteRpcCall(TIER1_COMP, it->second.hashVector->back(),
                                     getCall, NULL, DEFAULT_ROUTING, -1,
                                     0, rpcId);
                    it->second.hashVector->pop_back();
                } else {
                    // no more nodes to ask -> get failed
                    DHTgetCAPIResponse* capiGetRespMsg = new DHTgetCAPIResponse();
                    capiGetRespMsg->setIsSuccess(false);
                    sendRpcResponse(it->second.getCallMsg, capiGetRespMsg);
                    //cout << "DHT: GET failed: timeout2 (no one else)" << endl;
                    pendingRpcs.erase(rpcId);
                }
            }
        }
        break;
    }
    RPC_ON_CALL(BeehiveReplicate){
        EV << "[BeehiveDHT::handleRpcResponse()]\n"
           << "    BeehiveReplicate Timeout"
           << endl;

        PendingRpcs::iterator it = pendingRpcs.find(rpcId);

        if (it == pendingRpcs.end()) // unknown request
            return;

        // delete it, it's just replication call
        pendingRpcs.erase(rpcId);

        break;
    }
    RPC_SWITCH_END( )
}

void BeehiveDHT::handlePutRequest(BeehiveDHTPutCall* dhtMsg)
{
    std::string tempString = "PUT_REQUEST received: "
            + std::string(dhtMsg->getKey().toString(16));
    getParentModule()->getParentModule()->bubble(tempString.c_str());

    bool err;
    bool isSibling = overlay->isSiblingFor(overlay->getThisNode(),
                  dhtMsg->getKey(), 1, &err);
    if (err) {
        isSibling = true;
    }


    // remove data item from local data storage
    dataStorage->removeData(dhtMsg->getKey(), dhtMsg->getKind(),
                            dhtMsg->getId());

    if (dhtMsg->getValue().size() > 0) {
        // add ttl timer
        BeehiveDHTTtlTimer *timerMsg = new BeehiveDHTTtlTimer("ttl_timer");
        timerMsg->setKey(dhtMsg->getKey());
        timerMsg->setKind(dhtMsg->getKind());
        timerMsg->setId(dhtMsg->getId());
        scheduleAt(simTime() + dhtMsg->getTtl(), timerMsg);
        // storage data item in local data storage
        dataStorage->addData(dhtMsg->getKey(), dhtMsg->getKind(),
        		             dhtMsg->getId(), dhtMsg->getValue(), timerMsg,
                             dhtMsg->getIsModifiable(), dhtMsg->getSrcNode(),
                             isSibling);
    }

    // send back
    BeehiveDHTPutResponse* responseMsg = new BeehiveDHTPutResponse();
    responseMsg->setSuccess(true);
    responseMsg->setBitLength(PUTRESPONSE_L(responseMsg));
    RECORD_STATS(normalMessages++; numBytesNormal += responseMsg->getByteLength());

    sendRpcResponse(dhtMsg, responseMsg);
}

void BeehiveDHT::handleGetRequest(BeehiveDHTGetCall* dhtMsg)
{
    std::string tempString = "GET_REQUEST received: "
            + std::string(dhtMsg->getKey().toString(16));

    getParentModule()->getParentModule()->bubble(tempString.c_str());

    if (dhtMsg->getKey().isUnspecified()) {
        throw cRuntimeError("BeehiveDHT::handleGetRequest: Unspecified key!");
    }

    BeehiveDHTDumpVector* dataVect = dataStorage->dumpDht(dhtMsg->getKey(),
                                                   dhtMsg->getKind(),
                                                   dhtMsg->getId());

    // send back
    BeehiveDHTGetResponse* responseMsg = new BeehiveDHTGetResponse();
    responseMsg->setKey(dhtMsg->getKey());
    responseMsg->setIsHash(dhtMsg->getIsHash());

    if (dataVect->size() == 0) {
        responseMsg->setHashValue(BinaryValue::UNSPECIFIED_VALUE);
        responseMsg->setResultArraySize(0);
    } else {
        if (dhtMsg->getIsHash()) {
            // TODO: verify this
            BinaryValue resultValues;
            for (uint32_t i = 0; i < dataVect->size(); i++) {
                resultValues += (*dataVect)[i].getValue();
            }

            CSHA1 sha1;
            BinaryValue hashValue(20);
            sha1.Reset();
            sha1.Update((uint8_t*) (&(*resultValues.begin())),
                        resultValues.size());
            sha1.Final();
            sha1.GetHash((unsigned char*)&hashValue[0]);

            responseMsg->setHashValue(hashValue);
        } else {
            responseMsg->setResultArraySize(dataVect->size());

            for (uint32_t i = 0; i < dataVect->size(); i++) {
                responseMsg->setResult(i, (*dataVect)[i]);
            }

        }
    }
    delete dataVect;

    responseMsg->setBitLength(GETRESPONSE_L(responseMsg));
    RECORD_STATS(normalMessages++;
                 numBytesNormal += responseMsg->getByteLength());
    sendRpcResponse(dhtMsg, responseMsg);
}

void BeehiveDHT::handlePutCAPIRequest(DHTputCAPICall* capiPutMsg)
{
    // asks the replica list
    LookupCall* lookupCall = new LookupCall();
    lookupCall->setKey(capiPutMsg->getKey());
    lookupCall->setNumSiblings(numReplica);
    lookupCall->setReadReplicated(false);
    sendInternalRpcCall(OVERLAY_COMP, lookupCall, NULL, -1, 0,
                        capiPutMsg->getNonce());

    PendingRpcsEntry entry;
    entry.putCallMsg = capiPutMsg;
    entry.state = LOOKUP_STARTED;
    pendingRpcs.insert(make_pair(capiPutMsg->getNonce(), entry));
}

void BeehiveDHT::handleGetCAPIRequest(DHTgetCAPICall* capiGetMsg)
{
    LookupCall* lookupCall = new LookupCall();
    lookupCall->setKey(capiGetMsg->getKey());
    lookupCall->setNumSiblings(numReplica);
    lookupCall->setReadReplicated(true);
    sendInternalRpcCall(OVERLAY_COMP, lookupCall, NULL, -1, 0,
                        capiGetMsg->getNonce());

    PendingRpcsEntry entry;
    entry.getCallMsg = capiGetMsg;
    entry.state = LOOKUP_STARTED;
    pendingRpcs.insert(make_pair(capiGetMsg->getNonce(), entry));
}

void BeehiveDHT::handleDumpDhtRequest(DHTdumpCall* call)
{
    DHTdumpResponse* response = new DHTdumpResponse();
    BeehiveDHTDumpVector* dumpVector = dataStorage->dumpDht();

    response->setRecordArraySize(dumpVector->size());

    for (uint32_t i = 0; i < dumpVector->size(); i++) {
        response->setRecord(i, (*dumpVector)[i]);
    }

    delete dumpVector;

    sendRpcResponse(call, response);
}

void BeehiveDHT::handlePutResponse(BeehiveDHTPutResponse* dhtMsg, int rpcId)
{
    PendingRpcs::iterator it = pendingRpcs.find(rpcId);

    if (it == pendingRpcs.end()) // unknown request
        return;

    if (dhtMsg->getSuccess()) {
        it->second.numResponses++;
    } else {
        it->second.numFailed++;
    }


    if (it->second.numResponses / (double)it->second.numSent > 0.5) {

        DHTputCAPIResponse* capiPutRespMsg = new DHTputCAPIResponse();
        capiPutRespMsg->setIsSuccess(true);
        sendRpcResponse(it->second.putCallMsg, capiPutRespMsg);
        pendingRpcs.erase(rpcId);
    }
}

void BeehiveDHT::handleGetResponse(BeehiveDHTGetResponse* dhtMsg, int rpcId)
{
    NodeVector* hashVector = NULL;
    PendingRpcs::iterator it = pendingRpcs.find(rpcId);

    if (it == pendingRpcs.end()) // unknown request
        return;

    if (it->second.state == GET_VALUE_SENT) {
        // we have sent a 'real' get request
        if (!dhtMsg->getIsHash()) {
            // TODO verify hash
            DHTgetCAPIResponse* capiGetRespMsg = new DHTgetCAPIResponse();
            capiGetRespMsg->setResultArraySize(dhtMsg->getResultArraySize());
            for (uint i = 0; i < dhtMsg->getResultArraySize(); i++) {
                capiGetRespMsg->setResult(i, dhtMsg->getResult(i));
            }
            capiGetRespMsg->setIsSuccess(true);
            sendRpcResponse(it->second.getCallMsg, capiGetRespMsg);
            pendingRpcs.erase(rpcId);
            return;
        }
    }

    if (dhtMsg->getIsHash()) {
        std::map<BinaryValue, NodeVector>::iterator itHashes =
            it->second.hashes.find(dhtMsg->getHashValue());

        if (itHashes == it->second.hashes.end()) {
            // new hash
            NodeVector vect;
            vect.push_back(dhtMsg->getSrcNode());
            it->second.hashes.insert(make_pair(dhtMsg->getHashValue(),
                                               vect));
        } else {
            itHashes->second.push_back(dhtMsg->getSrcNode());
        }

        it->second.numResponses++;

        if (it->second.state == GET_VALUE_SENT) {
            // we have already sent a real get request
            return;
        }

        // count the maximum number of equal hash values received so far
        unsigned int maxCount = 0;


        for (itHashes = it->second.hashes.begin();
        itHashes != it->second.hashes.end(); itHashes++) {

            if (itHashes->second.size() > maxCount) {
                maxCount = itHashes->second.size();
                hashVector = &(itHashes->second);
            }
        }

        if ((double) maxCount / (double) it->second.numAvailableReplica
                >= ratioIdentical) {
            it->second.hashVector = hashVector;
        } else if (it->second.numResponses >= numGetRequests) {
            // we'll try to ask some other nodes
            if (it->second.replica.size() > 0) {
                BeehiveDHTGetCall* getCall = new BeehiveDHTGetCall();
                getCall->setKey(it->second.getCallMsg->getKey());
                getCall->setKind(it->second.getCallMsg->getKind());
                getCall->setId(it->second.getCallMsg->getId());
                getCall->setIsHash(true);
                getCall->setBitLength(GETCALL_L(getCall));
                RECORD_STATS(normalMessages++;
                numBytesNormal += getCall->getByteLength());
                sendRouteRpcCall(TIER1_COMP,
                                 it->second.replica.back(), getCall,
                                 NULL, DEFAULT_ROUTING, -1, 0, rpcId);
                it->second.replica.pop_back();
                it->second.state = GET_HASH_SENT;
            } else if (hashVector == NULL) {
                // we don't have anyone else to ask and no hash
                DHTgetCAPIResponse* capiGetRespMsg =
                    new DHTgetCAPIResponse();
                DhtDumpEntry result;
                result.setKey(dhtMsg->getKey());
                result.setValue(BinaryValue::UNSPECIFIED_VALUE);
                capiGetRespMsg->setResultArraySize(1);
                capiGetRespMsg->setResult(0, result);
                capiGetRespMsg->setIsSuccess(false);
                sendRpcResponse(it->second.getCallMsg, capiGetRespMsg);
                pendingRpcs.erase(rpcId);
                return;
            } else {
                // we don't have anyone else to ask => take what we've got
                it->second.hashVector = hashVector;
            }
        }
    }

    if ((it->second.state != GET_VALUE_SENT) &&
            (it->second.hashVector != NULL)) {
        // we have already received all the response and chosen a hash
        if (it->second.hashVector->size() > 0) {
            BeehiveDHTGetCall* getCall = new BeehiveDHTGetCall();
            getCall->setKey(it->second.getCallMsg->getKey());
            getCall->setKind(it->second.getCallMsg->getKind());
            getCall->setId(it->second.getCallMsg->getId());
            getCall->setIsHash(false);
            getCall->setBitLength(GETCALL_L(getCall));
            RECORD_STATS(normalMessages++;
                         numBytesNormal += getCall->getByteLength());
            sendRouteRpcCall(TIER1_COMP, it->second.hashVector->back(),
                             getCall, NULL, DEFAULT_ROUTING, -1, 0, rpcId);
            it->second.hashVector->pop_back();
            it->second.state = GET_VALUE_SENT;
        } else { // we don't have anyone else to ask
            DHTgetCAPIResponse* capiGetRespMsg = new DHTgetCAPIResponse();
            capiGetRespMsg->setResultArraySize(0);
            sendRpcResponse(it->second.getCallMsg, capiGetRespMsg);
            //cout << "DHT: GET failed: hash2 (no one else)" << endl;
            pendingRpcs.erase(rpcId);
        }
    }
}

void BeehiveDHT::update(const NodeHandle& node, bool joined)
{
    OverlayKey key;
    bool err = false;
    BeehiveDHTDataEntry entry;
    std::map<OverlayKey, BeehiveDHTDataEntry>::iterator it;

    EV << "[BeehiveDHT::update() @ " << overlay->getThisNode().getIp()
       << " (" << overlay->getThisNode().getKey().toString(16) << ")]\n"
       << "    Update called()"
       << endl;



    for (it = dataStorage->begin(); it != dataStorage->end(); it++) {
        key = it->first;
        entry = it->second;
        if (joined) {
            if (entry.responsible && (overlay->isSiblingFor(node, key,
                                                            numReplica, &err)
                    || err)) { // hack for Chord, if we've got a new predecessor

                if (err) {
                    EV << "[BeehiveDHT::update()]\n"
                       << "    Unable to know if key: " << key
                       << " is in range of node: " << node
                       << endl;
                    // For Chord: we've got a new predecessor
                    // TODO: only send record, if we are not responsible any more
                    // TODO: check all protocols to change routing table first,
                    //       and than call update.

                    //if (overlay->isSiblingFor(overlay->getThisNode(), key, 1, &err)) {
                    //    continue;
                    //}
                }

                sendMaintenancePutCall(node, key, entry);
            }
        }
        //TODO: move this to the inner block above?
        entry.responsible = overlay->isSiblingFor(overlay->getThisNode(),
                                                  key, 1, &err);
    }
}

void BeehiveDHT::sendMaintenancePutCall(const TransportAddress& node,
                                 const OverlayKey& key,
                                 const BeehiveDHTDataEntry& entry) {

    BeehiveDHTPutCall* dhtMsg = new BeehiveDHTPutCall();

    dhtMsg->setKey(key);
    dhtMsg->setKind(entry.kind);
    dhtMsg->setId(entry.id);
    dhtMsg->setValue(entry.value);


    dhtMsg->setTtl((int)SIMTIME_DBL(entry.ttlMessage->getArrivalTime()
                                    - simTime()));
    dhtMsg->setIsModifiable(entry.is_modifiable);
    dhtMsg->setMaintenance(true);
    dhtMsg->setBitLength(PUTCALL_L(dhtMsg));
    RECORD_STATS(maintenanceMessages++;
                 numBytesMaintenance += dhtMsg->getByteLength());

    sendRouteRpcCall(TIER1_COMP, node, dhtMsg);
}

void BeehiveDHT::handleLookupResponse(LookupResponse* lookupMsg, int rpcId)
{
    PendingRpcs::iterator it = pendingRpcs.find(rpcId);

    if (it == pendingRpcs.end()) {
        return;
    }

    if (it->second.putCallMsg != NULL) {


        if ((lookupMsg->getIsValid() == false)
                || (lookupMsg->getSiblingsArraySize() == 0)) {

            EV << "[BeehiveDHT::handleLookupResponse()]\n"
               << "    Unable to get replica list : invalid lookup"
               << endl;
            DHTputCAPIResponse* capiPutRespMsg = new DHTputCAPIResponse();
            capiPutRespMsg->setIsSuccess(false);
            //cout << "DHT::lookup failed" << endl;
            sendRpcResponse(it->second.putCallMsg, capiPutRespMsg);
            pendingRpcs.erase(rpcId);
            return;
        }

        if ((it->second.putCallMsg->getId() == 0) &&
                (it->second.putCallMsg->getValue().size() > 0)) {
            // pick a random id before replication of the data item
            // id 0 is kept for delete requests (i.e. a put with empty value)
            it->second.putCallMsg->setId(intuniform(1, 2147483647));
        }

        for (unsigned int i = 0; i < lookupMsg->getSiblingsArraySize(); i++) {
            BeehiveDHTPutCall* dhtMsg = new BeehiveDHTPutCall();
            dhtMsg->setKey(it->second.putCallMsg->getKey());
            dhtMsg->setKind(it->second.putCallMsg->getKind());
            dhtMsg->setId(it->second.putCallMsg->getId());
            dhtMsg->setValue(it->second.putCallMsg->getValue());
            dhtMsg->setTtl(it->second.putCallMsg->getTtl());
            dhtMsg->setIsModifiable(it->second.putCallMsg->getIsModifiable());
            dhtMsg->setMaintenance(false);
            dhtMsg->setBitLength(PUTCALL_L(dhtMsg));
            RECORD_STATS(normalMessages++;
                         numBytesNormal += dhtMsg->getByteLength());
            sendRouteRpcCall(TIER1_COMP, lookupMsg->getSiblings(i),
                             dhtMsg, NULL, DEFAULT_ROUTING, -1,
                             0, rpcId);
        }

        it->second.state = PUT_SENT;
        it->second.numResponses = 0;
        it->second.numFailed = 0;
        it->second.numSent = lookupMsg->getSiblingsArraySize();
    }
    else if (it->second.getCallMsg != NULL) {

        if ((lookupMsg->getIsValid() == false)
                || (lookupMsg->getSiblingsArraySize() == 0)) {

            EV << "[BeehiveDHT::handleLookupResponse()]\n"
               << "    Unable to get replica list : invalid lookup"
               << endl;
            DHTgetCAPIResponse* capiGetRespMsg = new DHTgetCAPIResponse();
            DhtDumpEntry result;
            result.setKey(lookupMsg->getKey());
            result.setValue(BinaryValue::UNSPECIFIED_VALUE);
            capiGetRespMsg->setResultArraySize(1);
            capiGetRespMsg->setResult(0, result);
            capiGetRespMsg->setIsSuccess(false);
            //cout << "DHT: lookup failed 2" << endl;
            sendRpcResponse(it->second.getCallMsg, capiGetRespMsg);
            pendingRpcs.erase(rpcId);
            return;
        }

        it->second.numSent = 0;

        for (unsigned int i = 0; i < lookupMsg->getSiblingsArraySize(); i++) {
            if (i < (unsigned int)numGetRequests) {
                BeehiveDHTGetCall* dhtMsg = new BeehiveDHTGetCall();
                dhtMsg->setKey(it->second.getCallMsg->getKey());
                dhtMsg->setKind(it->second.getCallMsg->getKind());
                dhtMsg->setId(it->second.getCallMsg->getId());
                dhtMsg->setIsHash(true);
                dhtMsg->setBitLength(GETCALL_L(dhtMsg));
                RECORD_STATS(normalMessages++;
                             numBytesNormal += dhtMsg->getByteLength());
                sendRouteRpcCall(TIER1_COMP, lookupMsg->getSiblings(i), dhtMsg,
                                 NULL, DEFAULT_ROUTING, -1, 0, rpcId);
                it->second.numSent++;
            } else {
                // we don't send, we just store the remaining keys
                it->second.replica.push_back(lookupMsg->getSiblings(i));
            }
        }

        it->second.numAvailableReplica = lookupMsg->getSiblingsArraySize();
        it->second.numResponses = 0;
        it->second.hashVector = NULL;
        it->second.state = GET_HASH_SENT;
    }
}

void BeehiveDHT::finishApp()
{
    simtime_t time = globalStatistics->calcMeasuredLifetime(creationTime);

    if (time >= GlobalStatistics::MIN_MEASURED) {
        globalStatistics->addStdDev("BeehiveDHT: Sent Maintenance Messages/s",
                                    maintenanceMessages / time);
        globalStatistics->addStdDev("BeehiveDHT: Sent Normal Messages/s",
                                    normalMessages / time);
        globalStatistics->addStdDev("BeehiveDHT: Sent Maintenance Bytes/s",
                                    numBytesMaintenance / time);
        globalStatistics->addStdDev("BeehiveDHT: Sent Normal Bytes/s",
                                    numBytesNormal / time);
    }
}

int BeehiveDHT::resultValuesBitLength(BeehiveDHTGetResponse* msg) {
    int bitSize = 0;
    for (uint i = 0; i < msg->getResultArraySize(); i++) {
        bitSize += msg->getResult(i).getValue().size();

    }
    return bitSize;
}

void BeehiveDHT::handleReplicateTimerExpired(cMessage* msg) 
{
    
    // reschedule replication timer
    // if (msg == replicate_timer) {
    //     scheduleAt(simTime() + replicateDelay, replicate_timer);
    // }
    // send replication request to all possible successors 
    // (technically, these requests should only be sent to nodes that are the "decidiing nodes" for a given object)

    // Get the list of current keys that live at this node
    BeehiveDHTDumpVector* dumpVector = dataStorage->dumpDht(); // Dumps out all data
    int numKeysStored = dumpVector->size(); // number of keys stored here
    vector<string> currData; // list to store keys
    for (uint32_t i = 0; i < numKeysStored; i++) { // fill up the list
        string currDataKey = (*dumpVector)[i].getKey().toString();
        currData.push_back(currDataKey);
    }
    // currData now holds all keys at this node. It should be included in the replication request sent to all successors.

    // get successor list and finger table
    oversim::Beehive* bOverlay = dynamic_cast<oversim::Beehive*>(overlay);
    oversim::BeehiveSuccessorList* succList = dynamic_cast<oversim::BeehiveSuccessorList*>(bOverlay->getParentModule()->getSubmodule("successorList"));
    oversim::BeehiveFingerTable* fingTable = dynamic_cast<oversim::BeehiveFingerTable*>(bOverlay->getParentModule()->getSubmodule("fingerTable"));

    int numFingers = fingTable->getSize();

    // for each key in successor list...
    for (uint i = 0; i < numFingers; i++) {

        // create message
        BeehiveReplicateCall *repmsg = new BeehiveReplicateCall();
        repmsg->setDestinationKey(fingTable->getFinger(i).getKey()); 

        if (dumpVector->size() > 0) {
            repmsg->setReplicatedKeysArraySize(dumpVector->size());
            for (uint32 j = 0; j < dumpVector->size(); j++) {
                repmsg->setReplicatedKeys(j, (*dumpVector)[j]);
            }
        } else {
            repmsg->setReplicatedKeysArraySize(0);
        }
        
        sendRouteRpcCall(TIER1_COMP, fingTable->getFinger(i).getKey(), repmsg);

	}

    // schedule next replication process
    cancelEvent(replicate_timer);
    scheduleAt(simTime() + replicateDelay, replicate_timer);
}

void BeehiveDHT::handleReplicateRequest(BeehiveReplicateCall* replicateRequest) 
{
    BeehiveReplicateCall* rrpc = (BeehiveReplicateCall*) replicateRequest;

    // get list of incoming data keys
    DhtDumpEntry incomingData[rrpc->getReplicatedKeysArraySize()];// = rrpc->getReplicatedKeys();
    for (uint i = 0; i < rrpc->getReplicatedKeysArraySize(); i++) {
        incomingData[i] = rrpc->getReplicatedKeys(i);
    }

    // Get the list of keys that live at this node
    BeehiveDHTDumpVector* dumpVector = dataStorage->dumpDht(); // Dumps out all data
    int numKeysStored = dumpVector->size(); // number of keys stored here
    DhtDumpEntry currData[numKeysStored]; // list to store keys
    for (uint32_t i = 0; i < numKeysStored; i++) { // fill up the list
        currData[i] = (*dumpVector)[i];
    }

    // compare incoming key list to the keys stored at this node, and store two list:
    //      1. list of keys that should be replicated at predecessor (these keys weren't in the incoming list)
    //      2. list of keys that should be deleted at predecessor (these keys were in the incoming list)
    vector<string> sharedKeys;
    set<string> unreplicatedKeys;
    for (uint i = 0; i < rrpc->getReplicatedKeysArraySize(); i++) {
        for (uint j = 0; j < numKeysStored; j++) {
            if (incomingData[i].getKey().toString() == currData[j].getKey().toString()) {
                sharedKeys.push_back(incomingData[i].getKey().toString());
            } else if (unreplicatedKeys.find(incomingData[i].getKey().toString()) == unreplicatedKeys.end()) {// && currData[j].getResponsible() == true) {
                unreplicatedKeys.insert(currData[j].getKey().toString());
            }
        }
    }

    // get the actual objects to replicate, not just their keys
    DhtDumpEntry objectsToReplicate[unreplicatedKeys.size()];
    int objCounter = 0;
    for (uint i = 0; i < rrpc->getReplicatedKeysArraySize(); i++) {
    	if (unreplicatedKeys.find(incomingData[i].getKey().toString()) != unreplicatedKeys.end()) {
    	    objectsToReplicate[objCounter] = incomingData[i];
    	    objCounter++;
    	}
    }

    // TODO: send back both of these lists in the response
    BeehiveReplicateResponse *resprpc = new BeehiveReplicateResponse();
    resprpc->setRespondingNode(thisNode);
    resprpc->setObjectsToReplicateArraySize(unreplicatedKeys.size());


    for (uint32 i = 0; i < unreplicatedKeys.size(); i++) {
    	resprpc->setObjectsToReplicate(i, objectsToReplicate[i]);
    	DhtDumpEntry currObject = objectsToReplicate[i];
    }
    sendRpcResponse(rrpc, resprpc);
}


void BeehiveDHT::handleReplicateResponse(BeehiveReplicateResponse* replicateResponse, int rpcId) 
{
    // we receive two lists in response:
    //      1. list of data to replicate here
    //      2. list of keys to delete here

    BeehiveReplicateResponse* rrpc = (BeehiveReplicateResponse*) replicateResponse;

    // TODO: store new data (first list)
    int numDataToStore = rrpc->getObjectsToReplicateArraySize();
    vector<DhtDumpEntry> keysActuallyReplicated;
    for (uint i = 0; i < numDataToStore; i++) {
	DhtDumpEntry currDatum = rrpc->getObjectsToReplicate(i);
        
	if (currDatum.getKind() != 0) {

	// create TTL message
	    BeehiveDHTTtlTimer *timerMsg = new BeehiveDHTTtlTimer("ttl_timer");
	    timerMsg->setKey(currDatum.getKey());
            timerMsg->setKind(currDatum.getKind());
            timerMsg->setId(currDatum.getId());
            scheduleAt(simTime() + currDatum.getTtl(), timerMsg);
	
	    dataStorage->addData(currDatum.getKey(), currDatum.getKind(), currDatum.getId(), currDatum.getValue(), timerMsg, currDatum.getIs_modifiable(), currDatum.getOwnerNode(), false);

	    keysActuallyReplicated.push_back(currDatum);
	}
    }

    // TODO: delete data that shouldn't be replicated anymore (second list)

    // TODO: update routing data
    BeehiveUpdateRoutingCall* beehiveUpdateRoutingCall = new BeehiveUpdateRoutingCall();
    //std::cout << keysActuallyReplicated[0];

    beehiveUpdateRoutingCall->setNewReplicatedKeysArraySize(keysActuallyReplicated.size());
    set<string> newReplicatedKeysSet;
    for (uint i = 0; i < keysActuallyReplicated.size(); i++) {
	//std::cout << keysActuallyReplicated[i].getKey().toString() + "\n";
	beehiveUpdateRoutingCall->setNewReplicatedKeys(i, keysActuallyReplicated[i]);
    }
    
    sendInternalRpcCall(OVERLAY_COMP, beehiveUpdateRoutingCall);
}

void BeehiveDHT::handleUpdateRoutingResponse(BeehiveUpdateRoutingResponse* updateRoutingResponse, int rpcId)
{
    // probably nothing...
}

std::ostream& operator<<(std::ostream& os, const BeehiveDHT::PendingRpcsEntry& entry)
{
    if (entry.getCallMsg) {
        os << "GET";
    } else if (entry.putCallMsg) {
        os << "PUT";
    }

    os << " state: " << entry.state
       << " numSent: " << entry.numSent
       << " numResponses: " << entry.numResponses
       << " numFailed: " << entry.numFailed
       << " numAvailableReplica: " << entry.numAvailableReplica;

    if (entry.replica.size() > 0) {
        os << " replicaSize: " << entry.replica.size();
    }

    if (entry.hashVector != NULL) {
        os << " hashVectorSize: " << entry.hashVector->size();
    }

    if (entry.hashes.size() > 0) {
        os << " hashes:";
        std::map<BinaryValue, NodeVector>::const_iterator it;

        int i = 0;
        for (it = entry.hashes.begin(); it != entry.hashes.end(); it++, i++) {
            os << " hash" << i << ":" << it->second.size();
        }
    }

    return os;
}
