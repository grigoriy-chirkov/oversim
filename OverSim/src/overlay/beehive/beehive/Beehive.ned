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


@namespace(oversim);

//
// The main module of the Beehive implementation
//
// @author Ingmar Baumgart, Markus Mauch
//
simple Beehive extends BaseOverlay
{
    parameters:
        @class(Beehive);
        volatile int successorListSize; // max number of successors in the SuccessorList
        // after joinRetry unsuccessful join requests, we fetch
        // a need bootstrap node from the GlobalNodeList
        int joinRetry;
        int stabilizeRetry; // retries before a successor is considered failed
        double joinDelay @unit(s); // delay between join retries (sec)
        double stabilizeDelay @unit(s); // stabilize interval (sec)
        double fixfingersDelay @unit(s); // fix_fingers interval (sec)
        bool aggressiveJoinMode; // use modified (faster) JOIN protocol
        double checkPredecessorDelay @unit(s); // checkPredecessor interval (sec)
        bool extendedFingerTable; // use the extended finger table?        
        int numFingerCandidates; // number of fingers when using extended finger table
        bool proximityRouting;    // use proximity routing
        // do not use failed sucessor again and inform new successor
        bool memorizeFailedSuccessor;
        bool mergeOptimizationL1; // enable merge optimization OPL1
        bool mergeOptimizationL2; // activate merge optimization OPL2
        bool mergeOptimizationL3; // activate merge optimization OPL3
        bool mergeOptimizationL4; // activate merge optimization OPL4
}

//
// This modul contains the successor list of the Beehive implementation.
//
// @author Ingmar Baumgart, Markus Mauch
//
simple BeehiveSuccessorList
{
    parameters:
        @display("i=block/table");
}

//
// This modul contains the finger table of the Beehive implementation.
//
// @author Ingmar Baumgart, Markus Mauch
//
simple BeehiveFingerTable
{
    parameters:
        @display("i=block/table");
}

//
// Implementation of the Beehive KBR overlay as described in
// "Beehive: A Scalable Peer-to-Peer Lookup Protocol for Internet
// Applications" by I. Stoica et al. published in Transactions on Networking.
//
// @author Ingmar Baumgart, Markus Mauch
//
module BeehiveModules like IOverlay
{
    parameters:
        @display("i=block/network2");

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
                @display("p=60,60");

        }
        fingerTable: BeehiveFingerTable {
            parameters:
                @display("p=150,60");
        }
        successorList: BeehiveSuccessorList {
            parameters:
                @display("p=240,60");

        }
    connections allowunconnected:
        udpIn --> beehive.udpIn;
        udpOut <-- beehive.udpOut;
        appIn --> beehive.appIn;
        appOut <-- beehive.appOut;
}
