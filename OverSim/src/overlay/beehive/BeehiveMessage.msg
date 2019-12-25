//
// Copyright (C) 2012 Institute of Telematics, Karlsruhe Institute of Technology (KIT)
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

//
// @author Grigory Chirkov
//

cplusplus {{
#include <IPvXAddress.h>
#include <NodeHandle.h>
#include <OverlayKey.h>
#include <CommonMessages_m.h>

// constants for message length in bit
static const int BEEHIVETYPE_L = 8;
static const int LASTHOPFLAG_L = 8;
static const int TIMESTAMP_L = 32;
static const int ROWNUMBER_L = 16;
static const int BEEHIVEFINDNODEEXTDATA_L = NODEHANDLE_L + HOPCOUNT_L;

#define BEEHIVE_L(msg) (BASEOVERLAY_L(msg) + BEEHIVETYPE_L)

#define BEEHIVESTATE_L(msg) (BEEHIVE_L(msg) + TRANSPORTADDRESS_L \
    + NODEHANDLE_L * \
    (msg->getRoutingTableArraySize() + msg->getLeafSetArraySize() \
    + msg->getNeighborhoodSetArraySize()) + 3 * ARRAYSIZE_L + HOPCOUNT_L + \
    LASTHOPFLAG_L + TIMESTAMP_L)

#define BEEHIVENEWLEAFS_L(msg) (msg->getLeafsArraySize() * NODEHANDLE_L + \
        ARRAYSIZE_L)

#define BEEHIVEJOINCALL_L(msg) BEEHIVE_L(msg)
#define BEEHIVEJOINRESPONSE_L(msg) BEEHIVE_L(msg) 

#define BEEHIVEREQUESTSTATECALL_L(msg) BEEHIVE_L(msg)
#define BEEHIVEREQUESTSTATERESPONSE_L(msg) BEEHIVE_L(msg)

#define BEEHIVEREQUESTREPAIRCALL_L(msg) BEEHIVE_L(msg)
#define BEEHIVEREQUESTREPAIRRESPONSE_L(msg) BEEHIVE_L(msg)

#define BEEHIVEREQUESTLEAFSETCALL_L(msg) BEEHIVE_L(msg)
#define BEEHIVEREQUESTLEAFSETRESPONSE_L(msg) BEEHIVE_L(msg)

#define BEEHIVEREQUESTROUTINGROWCALL_L(msg) (BEEHIVE_L(msg) + ROWNUMBER_L)
#define BEEHIVEREQUESTROUTINGROWRESPONSE_L(msg) BEEHIVE_L(msg)
}}


class noncobject IPvXAddress;
class noncobject TransportAddress;
class noncobject NodeHandle;
class noncobject OverlayKey;

class BaseOverlayMessage;
class BaseCallMessage;
class BaseResponseMessage;
class BaseAppDataMessage;


enum BeehiveStateMsgType
{
    BEEHIVE_STATE_STD        = 0x01;
    BEEHIVE_STATE_JOIN       = 0x02;
    BEEHIVE_STATE_MINJOIN    = 0x04;
    BEEHIVE_STATE_UPDATE     = 0x08;
    BEEHIVE_STATE_REPAIR     = 0x10;
    BEEHIVE_STATE_JOINUPDATE = 0x20;
    BEEHIVE_STATE_LEAFSET    = 0x40;
    BEEHIVE_STATE_ROUTINGROW = 0x80;
}


//
// Message used to send a BeehiveState
//
packet BeehiveStateMessage extends BaseOverlayMessage
{
    int beehiveStateMsgType @enum(BeehiveStateMsgType) = BEEHIVE_STATE_STD;    // the type of the BeehiveStateMessage
    NodeHandle sender = NodeHandle::UNSPECIFIED_NODE;    // NodeHandle of the node sending this message
    NodeHandle routingTable[];    // the routingTable of the sender
    NodeHandle leafSet[];         // the leafSet of the sender
    NodeHandle neighborhoodSet[]; // the neighborhoodSet of the sender
    int row = 0;                  // row or number of hops this message takes
    bool lastHop = false;         // is this node the destination node?
    simtime_t timestamp;          // simTime when sending this message
}


//
// Message used to find a Beehive node
//
packet BeehiveFindNodeExtData
{
    TransportAddress sendStateTo = TransportAddress::UNSPECIFIED_NODE;    // the sender of this message
    int joinHopCount = 0;    // counts the hops this message takes
}


//
// Message used to inform about new BeehiveLeafs
//
packet BeehiveNewLeafsMessage
{
    NodeHandle leafs[];    // the new BeehiveLeafs
}


message BeehiveSendState
{
    TransportAddress dest = TransportAddress::UNSPECIFIED_NODE;
}


packet BeehiveJoinCall extends BaseCallMessage
{
}


packet BeehiveJoinResponse extends BaseResponseMessage
{
}


packet BeehiveRequestStateCall extends BaseCallMessage
{
}


packet BeehiveRequestStateResponse extends BaseResponseMessage
{
}


packet BeehiveRequestRepairCall extends BaseCallMessage
{
}


packet BeehiveRequestRepairResponse extends BaseResponseMessage
{
}


packet BeehiveRequestLeafSetCall extends BaseCallMessage
{
}


packet BeehiveRequestLeafSetResponse extends BaseResponseMessage
{
}


packet BeehiveRequestRoutingRowCall extends BaseCallMessage
{
    int row;
}


packet BeehiveRequestRoutingRowResponse extends BaseResponseMessage
{
}
