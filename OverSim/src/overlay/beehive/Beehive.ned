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

package oversim.overlay.beehive;

import oversim.common.BaseOverlay;
import oversim.common.IOverlay;


simple BaseBeehive extends BaseOverlay
{
    parameters:

        bool enableNewLeafs;          // enable Beehive API call newLeafs()
        bool optimizeLookup;          // whether to search the closest node
                                      // in findCloserNode() calls
        int bitsPerDigit;             // bits per Beehive digit
        int numberOfLeaves;           // number of entries in leaf set
        int numberOfNeighbors;        // number of entries in neighborhoot set
        double joinTimeout @unit(s);  // seconds to wait for STATE message
                                      // from closest node
        double repairTimeout @unit(s);// how long to wait for repair messages
        bool useRegularNextHop;
        bool alwaysSendUpdate;
                                      // tables delayed (should be very small)
        double readyWait @unit(s);    // seconds to wait for missing state
                                      // messages in JOIN phase
        bool proximityNeighborSelection; // enable PNS ?
}
//
// The main module of the Beehive implementation
//
// @author Felix Palmen
//
simple Beehive extends BaseBeehive
{
    parameters:
        @class(Beehive);

        bool partialJoinPath;             // allow join even with missing state
                                          // message along the routing path
        bool useSecondStage;
        double secondStageWait @unit(s);  // how long to wait before starting
                                          // second stage of init phase
        bool pingBeforeSecondStage;
        // join at nearest node, otherwise use bootstrapnode
        bool useDiscovery;
        // use smaller join state msgs (as described in the 2nd Beehive paper)
        bool minimalJoinState;
        // use state messages for leafset repair, otherwise use leafset messages
        bool sendStateAtLeafsetRepair;
        // how long to wait for leafset pings in discovery stage
        double discoveryTimeoutAmount @unit(s);

        bool useRoutingTableMaintenance;
        // interval for periodic routing table maintenance
        double routingTableMaintenanceInterval @unit(s);
        // beehive configuration according to the original paper
        bool overrideOldBeehive;
        bool overrideNewBeehive;            // optimized beehive configuration
        @display("i=block/circle");
}

//
// This module contains the routing table of the Beehive implementation.
//
// @author Felix Palmen
//
simple BeehiveRoutingTable
{
    parameters:
        @display("i=block/table");
}

//
// This module contains the leaf set of the Beehive implementation.
//
// @author Felix Palmen
//
simple BeehiveLeafSet
{
    parameters:
        @display("i=block/table");
}

//
// This module contains the neighborhood set of the Beehive implementation.
//
// @author Felix Palmen
//
simple BeehiveNeighborhoodSet
{
    parameters:
        @display("i=block/table");
}

//
// Implementation of the Beehive KBR overlay as described in
// "Beehive: Scalable, Decentralized Object Location, and 
// Routing for Large-Scale Peer-to-Peer Systems" 
// by Antony Rowstron and Peter Druschel, published in 
// Lecture Notes in Computer Science, volume 2218.
//
// @author Felix Palmen
//
module BeehiveModules like IOverlay
{
    gates:
        input udpIn;   // gate from the UDP layer
        output udpOut;    // gate to the UDP layer
        input tcpIn;    // gate from the TCP layer
        output tcpOut;    // gate to the TCP layer
        input appIn;   // gate from the application
        output appOut;    // gate to the application

    submodules:
        beehive: Beehive {
            parameters:
                @display("p=60,52;i=block/circle");
        }
        beehiveRoutingTable: BeehiveRoutingTable {
            parameters:
                @display("p=140,68;i=block/table");
        }
        beehiveLeafSet: BeehiveLeafSet {
            parameters:
                @display("p=220,52;i=block/table");
        }
        beehiveNeighborhoodSet: BeehiveNeighborhoodSet {
            parameters:
                @display("p=300,68;i=block/table");

        }
    connections allowunconnected:
        udpIn --> beehive.udpIn;
        udpOut <-- beehive.udpOut;
        appIn --> beehive.appIn;
        appOut <-- beehive.appOut;

}
