//
//  VersionVector.cc
//  Couchbase Lite Core
//
//  Created by Jens Alfke on 5/23/16.
//  Copyright (c) 2016 Couchbase. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//    http://www.apache.org/licenses/LICENSE-2.0
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.

#include "VersionVector.hh"
#include "SecureDigest.hh"
#include "Error.hh"
#include "StringUtil.hh"
#include "fleece/Fleece.hh"
#include "varint.hh"
#include <sstream>
#include <unordered_map>
#include <algorithm>


namespace litecore {
    using namespace std;
    using namespace fleece;


    // Utility that allocates a buffer, lets the callback write into it, then trims the buffer.
    static alloc_slice writeAlloced(size_t maxSize, function_ref<bool(slice*)> writer) {
        alloc_slice buf(maxSize);
        slice out = buf;
        Assert( writer(&out) );
        buf.shorten(buf.size - out.size);
        return buf;
    }


#pragma mark - VERSION:


    static void throwBadBinary() {
        error::_throw(error::BadRevisionID, "Invalid binary version ID");
    }

    static void throwBadASCII(slice string = nullslice) {
        if (string)
            error::_throw(error::BadRevisionID, "Invalid version string '%.*s'", SPLAT(string));
        else
            error::_throw(error::BadRevisionID, "Invalid version string");
    }


    Version::Version(slice string) {
        slice in = string;
        _gen = in.readHex();
        if (in.readByte() != '@' || _gen == 0)
            throwBadASCII(string);
        if (in == "*"_sl) {
            _author = kMePeerID;
        } else {
            _author.id = in.readHex();
            if (in.size > 0 || _author == kMePeerID)
                throwBadASCII(string);
        }
    }

    Version::Version(slice *dataP) {
        if (!ReadUVarInt(dataP, &_gen) || !ReadUVarInt(dataP, &_author.id))
            throwBadBinary();
        validate();
    }

    void Version::validate() const {
        if (_gen == 0)
            error::_throw(error::BadRevisionID);
    }

    bool Version::writeBinary(slice *out, peerID myID) const {
        uint64_t id = (_author == kMePeerID) ? myID.id : _author.id;
        return WriteUVarInt(out, _gen) && WriteUVarInt(out, id);
    }

    bool Version::writeASCII(slice *out, peerID myID) const {
        if (!out->writeHex(_gen) || !out->writeByte('@'))
            return false;
        auto author = (_author != kMePeerID) ? _author : myID;
        if (author != kMePeerID)
            return out->writeHex(author.id);
        else
            return out->writeByte('*');
    }

    alloc_slice Version::asASCII(peerID myID) const {
        return writeAlloced(kMaxASCIILength, [&](slice *out) {
            return writeASCII(out, myID);
        });
    }

    versionOrder Version::compareGen(generation a, generation b) {
        if (a > b)
            return kNewer;
        else if (a < b)
            return kOlder;
        return kSame;
    }

    versionOrder Version::compareTo(const VersionVector &vv) const {
        versionOrder o = vv.compareTo(*this);
        if (o == kOlder)
            return kNewer;
        else if (o == kNewer)
            return kOlder;
        else
            return o;
    }


#pragma mark - CONVERSION:


    void VersionVector::readBinary(slice data) {
        reset();
        if (data.size < 1 || data.readByte() != 0)
            throwBadBinary();
        while (data.size > 0)
            _vers.emplace_back(&data);
    }


    alloc_slice VersionVector::asBinary(peerID myID) const {
        return writeAlloced(1 + _vers.size() * 2 * kMaxVarintLen64, [&](slice *out) {
            if (!out->writeByte(0))           // leading 0 byte distinguishes it from a `revid`
                return false;
            for (auto &v : _vers)
                if (!v.writeBinary(out, myID))
                    return false;
            return true;
        });
    }


    size_t VersionVector::maxASCIILen() const {
        return _vers.size() * (Version::kMaxASCIILength + 1);
    }


    bool VersionVector::writeASCII(slice *out, peerID myID) const {
        int n = 0;
        for (auto &v : _vers) {
            if (n++ && !out->writeByte(','))
                return false;
            if (!v.writeASCII(out, myID))
                return false;
        }
        return true;
    }


    alloc_slice VersionVector::asASCII(peerID myID) const {
        if (_vers.empty())
            return nullslice;
        return writeAlloced(maxASCIILen(), [&](slice *out) {
            return writeASCII(out, myID);
        });
    }


    Version VersionVector::readCurrentVersionFromBinary(slice data) {
        if (data.size < 1 || data.readByte() != 0)
            throwBadBinary();
        return Version(&data);
    }


    void VersionVector::readASCII(slice str) {
        if (str.size == 0)
            throwBadASCII(str);
        reset();
        while (str.size > 0) {
            const void *comma = str.findByteOrEnd(',');
            _vers.emplace_back(str.upTo(comma));
            str = str.from(comma);
            if (str.size > 0)
                str.moveStart(1); // skip comma
        }
    }

    
#pragma mark - OPERATIONS:


    versionOrder VersionVector::compareTo(const Version& v) const {
        auto mine = const_cast<VersionVector*>(this)->findPeerIter(v.author());
        if (mine == _vers.end())
            return kOlder;
        else if (mine->gen() < v.gen())
            return kOlder;
        else if (mine->gen() == v.gen() && mine == _vers.begin())
            return kSame;
        else
            return kNewer;
    }


    versionOrder VersionVector::compareTo(const VersionVector &other) const {
        //OPT: This is O(n^2), since the `for` loop calls `other[ ]`, which is a linear search.
        versionOrder o = kSame;
        ssize_t countDiff = count() - other.count();
        if (countDiff < 0)
            o = kOlder;             // other must have versions from authors I don't have
        else if (countDiff > 0)
            o = kNewer;             // I must have versions from authors other doesn't have
        else if (count() > 0 && (*this)[0] == other[0])
            return kSame;           // first revs are identical so vectors are equal

        for (auto &v : _vers) {
            auto othergen = other[v.author()];
            if (v.gen() < othergen) {
                o = versionOrder(o | kOlder);
            } else if (v.gen() > othergen) {
                o = versionOrder(o | kNewer);
                if (othergen == 0) {
                    // other doesn't have this author, which makes its remaining entries more likely
                    // to have authors I don't have; when that becomes certainty, set 'older' flag:
                    if (--countDiff < 0)
                        o = versionOrder(o | kOlder);
                }
            }
            if (o == kConflicting)
                break;
        }
        return o;
    }

    vector<Version>::iterator VersionVector::findPeerIter(peerID author) {
        auto v = _vers.begin();
        for (; v != _vers.end(); ++v) {
            if (v->author() == author)
                break;
        }
        return v;
    }

    generation VersionVector::genOfAuthor(peerID author) const {
        auto v = const_cast<VersionVector*>(this)->findPeerIter(author);
        return (v != _vers.end()) ? v->gen() : 0;
    }

    void VersionVector::incrementGen(peerID author) {
        generation gen = 1;
        if (auto versP = findPeerIter(author); versP != _vers.end()) {
            gen += versP->gen();
            _vers.erase(versP);
        }
        _vers.insert(_vers.begin(), Version(gen, author));
    }

    void VersionVector::limitCount(size_t maxCount) {
        if (_vers.size() > maxCount)
            _vers.erase(_vers.begin() + maxCount, _vers.end());
    }

    

#pragma mark - MODIFICATION:


    void VersionVector::append(const Version &vers) {
        _vers.push_back(vers);
    }

    void VersionVector::compactMyPeerID(peerID myID) {
        auto versP = findPeerIter(myID);
        if (versP != _vers.end())
            *versP = Version(versP->gen(), kMePeerID);
    }

    void VersionVector::expandMyPeerID(peerID myID) {
        auto versP = findPeerIter(kMePeerID);
        if (versP != _vers.end())
            *versP = Version(versP->gen(), myID);
    }

    bool VersionVector::isExpanded() const {
        for (auto &vers : _vers)
            if (vers.author() == kMePeerID)
                return false;
        return true;
    }


#pragma mark - MERGING:


    // A hash table mapping peerID->generation, as an optimization for versionVector operations
    class versionMap {
    public:
        versionMap(const vector<Version> &vec) {
            _map.reserve(vec.size());
            for (auto &v : vec)
                add(v);
        }

        void add(const Version &vers) {
            _map[vers.author().id] = vers.gen();
        }

        generation operator[] (peerID author) {
            auto i = _map.find(author.id);
            return (i == _map.end()) ? 0 : i->second;
        }

    private:
        unordered_map<uint64_t, generation> _map;
    };
    
    
    VersionVector VersionVector::mergedWith(const VersionVector &other) const {
        // Walk through the two vectors in parallel, adding the current component from each if it's
        // newer than the corresponding component in the other. This isn't going to produce the
        // optimal ordering, but it should be pretty close.
        versionMap myMap(_vers), otherMap(other._vers);
        VersionVector result;
        size_t mySize = _vers.size(), itsSize = other._vers.size(), maxSize = max(mySize, itsSize);
        for (size_t i = 0; i < maxSize; ++i) {
            if (i < mySize) {
                if (auto &vers = _vers[i]; vers.gen() >= otherMap[vers.author()])
                    result.append(vers);
            }
            if (i < itsSize) {
                if (auto &vers = other._vers[i]; vers.gen() > myMap[vers.author()])
                    result.append(vers);
            }
        }
        return result;
    }



#if 0
    string VersionVector::canonicalString(peerID myPeerID) const {
        auto vec = *this; // copy before sorting
        vec.expandMyPeerID(myPeerID);
        sort(vec._vers.begin(), vec._vers.end(),
             [myPeerID](const Version &a, const Version &b) {
            return a.author().id < b.author().id;
        });
        return vec.asString();
    }


    void VersionVector::readFrom(Value val) {
        reset();
        Array arr = val.asArray();
        if (!arr)
            error::_throw(error::BadRevisionID);
        Array::iterator i(arr);
        if (i.count() % 2 != 0)
            error::_throw(error::BadRevisionID);
        _vers.reserve(i.count() / 2);
        for (; i; ++i,++i) {
            auto g = i[0], p = i[1];
            if (!g.isInteger() || !p.isInteger())
                error::_throw(error::BadRevisionID);
            _vers.emplace_back((generation)g.asUnsigned(), peerID{p.asUnsigned()});
        }
    }


    void VersionVector::writeTo(Encoder &encoder) const {
        encoder.beginArray(2*_vers.size());
        for (auto &v : _vers) {
            encoder << v.gen();
            encoder << v.author().id;
        }
        encoder.endArray();
    }
#endif

}
