//
// Copyright (C) 2006 Institut fuer Telematik, Universitaet Karlsruhe (TH)
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
 * @file BeehiveTypes.h
 * @author Grigory Chirkov
 * Based on the Pastry overlay implementation
 * This file contains some structs and typedefs used internally by Beehive
 */

#ifndef __BEEHIVE_TYPES_H_
#define __BEEHIVE_TYPES_H_

#include <map>
#include <limits>

#include <OverlayKey.h>
#include <NodeHandle.h>

#include "BeehiveMessage_m.h"

/**
 * value for infinite proximity (ping timeout):
 */
#define BEEHIVE_PROX_INFINITE -1

/**
 * value for undefined proximity:
 */
#define BEEHIVE_PROX_UNDEF -2

/**
 * value for not yet determined proximity value:
 */
#define BEEHIVE_PROX_PENDING -3

/**
 * struct-type for temporary proximity metrics to a STATE message
 */
struct BeehiveStateMsgProximity
{
    std::vector<simtime_t> pr_rt;
    std::vector<simtime_t> pr_ls;
    std::vector<simtime_t> pr_ns;
};

/**
 * struct-type containing local info while processing a STATE message
 */
struct BeehiveStateMsgHandle
{
    BeehiveStateMessage* msg;
    BeehiveStateMsgProximity* prox;
    bool outdatedUpdate;
    uint32_t nonce;

    BeehiveStateMsgHandle() : msg(NULL), prox(NULL), outdatedUpdate(false) {};
    BeehiveStateMsgHandle(BeehiveStateMessage* msg)
    : msg(msg), prox(NULL), outdatedUpdate(false)
    {
        nonce = intuniform(0, std::numeric_limits<uint32_t>::max()); //0x7FFFFF ???
    };
};

/**
 * struct for storing a NodeHandle together with its proximity value and an
 * optional timestamp
 */
struct BeehiveExtendedNode
{
    NodeHandle node;
    simtime_t rtt;
    simtime_t timestamp;

    BeehiveExtendedNode() : node(), rtt(-2), timestamp(0) {};
    BeehiveExtendedNode(const NodeHandle& node, simtime_t rtt,
                       simtime_t timestamp = 0)
    : node(node), rtt(rtt), timestamp(timestamp) {};
};


#endif
