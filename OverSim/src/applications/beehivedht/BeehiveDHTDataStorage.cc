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
 * @file BeehiveDHTDataStorage.cc
 * @author Ingmar Baumgart
 */

#include <omnetpp.h>
#include <hashWatch.h>

#include "BeehiveDHTDataStorage.h"

Define_Module(BeehiveDHTDataStorage);

using namespace std;

std::ostream& operator<<(std::ostream& os, const BeehiveDHTDataEntry entry)
{
    os << "Value: " << entry.value
       << " Kind: " << entry.kind
       << " ID: " << entry.id
       << " Endtime: " << entry.ttlMessage->getArrivalTime()
       << " Responsible: " << entry.responsible
       << " SourceNode: " << entry.sourceNode;

    if (entry.siblingVote.size()) {
        os << " siblingVote:";

        for (SiblingVoteMap::const_iterator it = entry.siblingVote.begin();
             it != entry.siblingVote.end(); it++) {
            os << " " << it->first << " (" << it->second.size() << ")";
        }
    }
    return os;
}


void BeehiveDHTDataStorage::initialize(int stage)
{
    if (stage != MIN_STAGE_APP)
        return;

    WATCH_MULTIMAP(dataMap);
}

void BeehiveDHTDataStorage::handleMessage(cMessage* msg)
{
    error("This module doesn't handle messages!");
}

void BeehiveDHTDataStorage::clear()
{
    map<OverlayKey, BeehiveDHTDataEntry>::iterator iter;

    for( iter = dataMap.begin(); iter != dataMap.end(); iter++ ) {
        cancelAndDelete(iter->second.ttlMessage);
    }

    dataMap.clear();
}


uint32_t BeehiveDHTDataStorage::getSize()
{
    return dataMap.size();
}

BeehiveDHTDataEntry* BeehiveDHTDataStorage::getDataEntry(const OverlayKey& key,
                                           uint32_t kind, uint32_t id)
{
    pair<BeehiveDHTDataMap::iterator, BeehiveDHTDataMap::iterator> pos =
        dataMap.equal_range(key);

    while (pos.first != pos.second) {
        if ((pos.first->second.kind == kind) &&
                (pos.first->second.id == id)) {
            return &pos.first->second;
        }
        ++pos.first;
    }

    return NULL;
}



BeehiveDHTDataVector* BeehiveDHTDataStorage::getDataVector(const OverlayKey& key,
                                             uint32_t kind, uint32_t id)
{
    BeehiveDHTDataVector* vect = new BeehiveDHTDataVector();
    BeehiveDHTDataEntry entry;

    pair<BeehiveDHTDataMap::iterator, BeehiveDHTDataMap::iterator> pos =
        dataMap.equal_range(key);

    while (pos.first != pos.second) {
        entry = pos.first->second;
        vect->push_back(make_pair(key, entry));
        ++pos.first;
    }

    return vect;
}


const NodeHandle& BeehiveDHTDataStorage::getSourceNode(const OverlayKey& key,
                                                uint32_t kind, uint32_t id)
{
    BeehiveDHTDataEntry* entry = getDataEntry(key, kind, id);

    if (entry == NULL)
        return NodeHandle::UNSPECIFIED_NODE;
    else
        return entry->sourceNode;
}

const bool BeehiveDHTDataStorage::isModifiable(const OverlayKey& key,
                                        uint32_t kind, uint32_t id)
{
    BeehiveDHTDataEntry* entry = getDataEntry(key, kind, id);

    if (entry == NULL)
        return true;
    else
        return entry->is_modifiable;
}


const BeehiveDHTDataMap::iterator BeehiveDHTDataStorage::begin()
{
    return dataMap.begin();
}

const BeehiveDHTDataMap::iterator BeehiveDHTDataStorage::end()
{
    return dataMap.end();
}

BeehiveDHTDataEntry* BeehiveDHTDataStorage::addData(const OverlayKey& key, uint32_t kind,
                                      uint32_t id,
                                      BinaryValue value, cMessage* ttlMessage,
                                      bool is_modifiable, NodeHandle sourceNode,
                                      bool responsible)
{
    BeehiveDHTDataEntry entry;
    entry.kind = kind;
    entry.id = id;
    entry.value = value;
    entry.ttlMessage = ttlMessage;
    entry.sourceNode = sourceNode;
    entry.is_modifiable = is_modifiable;
    entry.responsible = responsible;

    if ((kind == 0) || (id == 0)) {
        throw cRuntimeError("BeehiveDHTDataStorage::addData(): "
                            "Not allowed to add data with kind = 0 or id = 0!");
    }

    pair<BeehiveDHTDataMap::iterator, BeehiveDHTDataMap::iterator> pos =
        dataMap.equal_range(key);

    // insert new record in sorted multimap (order: key, kind, id)
    while ((pos.first != pos.second) && (pos.first->second.kind < kind)) {
        ++pos.first;
    }

    while ((pos.first != pos.second) && (pos.first->second.kind == kind)
            && (pos.first->second.id < id)) {
        ++pos.first;
    }

    return &(dataMap.insert(pos.first, make_pair(key, entry))->second);
}

void BeehiveDHTDataStorage::removeData(const OverlayKey& key, uint32_t kind,
                                uint32_t id)
{
    pair<BeehiveDHTDataMap::iterator, BeehiveDHTDataMap::iterator> pos =
        dataMap.equal_range(key);

    while (pos.first != pos.second) {

        if (((kind == 0) || (pos.first->second.kind == kind)) &&
                ((id == 0) || (pos.first->second.id == id))) {
            cancelAndDelete(pos.first->second.ttlMessage);
            dataMap.erase(pos.first++);
        } else {
            ++pos.first;
        }
    }
}

BeehiveDHTDumpVector* BeehiveDHTDataStorage::dumpDht(const OverlayKey& key, uint32_t kind,
                                       uint32_t id)
{
    BeehiveDHTDumpVector* vect = new BeehiveDHTDumpVector();
    DhtDumpEntry entry;

    BeehiveDHTDataMap::iterator iter, end;

    if (key.isUnspecified()) {
        iter = dataMap.begin();
        end = dataMap.end();
    } else {
        iter = dataMap.lower_bound(key);
        end = dataMap.upper_bound(key);
    }

    for (; iter != end; iter++) {
        if (((kind == 0) || (iter->second.kind == kind)) &&
                ((id == 0) || (iter->second.id == id))) {

            entry.setKey(iter->first);
            entry.setKind(iter->second.kind);
            entry.setId(iter->second.id);
            entry.setValue(iter->second.value);
            entry.setTtl((int)SIMTIME_DBL(
                        iter->second.ttlMessage->getArrivalTime() - simTime()));
            entry.setOwnerNode(iter->second.sourceNode);
            entry.setIs_modifiable(iter->second.is_modifiable);
            entry.setResponsible(iter->second.responsible);
            vect->push_back(entry);
        }
    }

    return vect;
}


// TODO: not used ?
void BeehiveDHTDataStorage::updateDisplayString()
{
    if (ev.isGUI()) {
        char buf[80];

        if (dataMap.size() == 1) {
            sprintf(buf, "1 data item");
        } else {
            sprintf(buf, "%zi data items", dataMap.size());
        }

        getDisplayString().setTagArg("t", 0, buf);
        getDisplayString().setTagArg("t", 2, "blue");
    }

}

// TODO: not used ?
void BeehiveDHTDataStorage::updateTooltip()
{
    if (ev.isGUI()) {
        std::stringstream str;

        for (BeehiveDHTDataMap::iterator it = dataMap.begin();
             it != dataMap.end(); it++) {
            str << it->second.value;
        }

        str << endl;

        char buf[1024];
        sprintf(buf, "%s", str.str().c_str());
        getDisplayString().setTagArg("tt", 0, buf);
    }
}

// TODO: not used ?
void BeehiveDHTDataStorage::display()
{
    cout << "Content of BeehiveDHTDataStorage:" << endl;
    for (BeehiveDHTDataMap::iterator it = dataMap.begin();
         it != dataMap.end(); it++) {
        cout << "Key: " << it->first << " Kind: " << it->second.kind
             << " ID: " << it->second.id << " Value: "
             << it->second.value << "End-time: "
             << it->second.ttlMessage->getArrivalTime() << endl;
    }
}
