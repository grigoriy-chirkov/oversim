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
 * @file BeehiveNeighborhoodSet.h
 * @author Grigory Chirkov
 * Based on the Pastry overlay implementation
 */

#ifndef __BEEHIVENEIGHBORHOODSET_H
#define __BEEHIVENEIGHBORHOODSET_H


#include <vector>

#include <omnetpp.h>

#include <NodeHandle.h>

#include "BeehiveStateObject.h"
#include "BeehiveTypes.h"
#include "BeehiveMessage_m.h"

/**
 * BeehiveNeighborhoodSet module
 *
 * This module contains the NeighborhoodSet of the Beehive implementation.
 *
 * @author Felix Palmen
 * @see Beehive
 */
class BeehiveNeighborhoodSet : public BeehiveStateObject
{
  public:

    /**
     * Initializes the neighborhood set. This should be called on startup
     *
     * @param numberOfNeighbors Beehive configuration parameter
     * @param bitsPerDigit number of bits per digits
     * @param owner the node this table belongs to
     */
    void initializeSet(uint32_t numberOfNeighbors,
                       uint32_t bitsPerDigit,
                       const NodeHandle& owner);

    /**
     * dump content of the set to a BeehiveStateMessage
     *
     * @param msg the BeehiveStateMessage to be filled with entries
     */
    virtual void dumpToStateMessage(BeehiveStateMessage* msg) const;

    /**
     * try to find a node numerically closer to a given key with the same
     * shared prefix as the current node in the neighborhood set. this method
     * is to be called, when a regular next hop couldn't be found or wasn't
     * reachable.
     *
     * @param destination the destination key
     * @param optimize if set, check all nodes and return the best/closest one
     * @return a closer NodeHandle or NodeHandle::UNSPECIFIED_NODE if none was
     *	  found
     */
    virtual const NodeHandle& findCloserNode(const OverlayKey& destination,
	    bool optimize = false);

    void findCloserNodes(const OverlayKey& destination,
                         NodeVector* nodes);

    /**
     * merge a node into NeighborhoodSet
     *
     * @param node the node to merge
     * @param prox proximity value of the node
     * @return true if node was merged
     */
    virtual bool mergeNode(const NodeHandle& node, simtime_t prox);

    /**
     * appends all neighborhood set entries to a given vector of
     * TransportAddresses, needed to find all Nodes to be notified after
     * joining.
     *
     * @param affected the vector to fill with leaf set entries
     */
    virtual void dumpToVector(std::vector<TransportAddress>& affected) const;

    /**
     * tell the neighborhood set about a failed node
     *
     * @param failed the failed node
     */
    virtual const TransportAddress& failedNode(const TransportAddress& failed);

  private:

    uint32_t numberOfNeighbors;
    std::vector<BeehiveExtendedNode> neighbors;
    virtual void earlyInit(void);
};

/**
 * Stream output operator to make WATCH() do something useful with the
 * neighborhood set
 */
std::ostream& operator<<(std::ostream& os, const BeehiveExtendedNode& n);


#endif
