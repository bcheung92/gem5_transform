/*
 * Copyright (c) 2012-2013 ARM Limited
 * All rights reserved.
 *
 * The license below extends only to copyright in the software and shall
 * not be construed as granting a license to any other intellectual
 * property including but not limited to intellectual property relating
 * to a hardware implementation of the functionality of the software
 * licensed hereunder.  You may use the software subject to the license
 * terms below provided that you ensure that this notice is replicated
 * unmodified and in its entirety in all distributions of the software,
 * modified or unmodified, in source code or in binary form.
 *
 * Copyright (c) 2003-2005,2014 The Regents of The University of Michigan
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors: Erik Hallnor
 */

/**
 * @file
 * Declaration of a base set associative tag store.
 */

#ifndef __MEM_CACHE_TAGS_BASESETASSOC_HH__
#define __MEM_CACHE_TAGS_BASESETASSOC_HH__

#include <cassert>
#include <cstring>
#include <list>

#include "mem/cache/tags/base.hh"
#include "mem/cache/tags/cacheset.hh"
#include "mem/cache/base.hh"
#include "mem/cache/blk.hh"
#include "mem/packet.hh"
#include "params/BaseSetAssoc.hh"

/*Notes by lokeshjindal15
 * Class BaseSetAssoc:
 * has a filed blklist which is a list of pointers of type CacheBlk.
 * has fields assoc (associativity) and numSets (# of sets in cache)
 * sets => pointer of type CacheSet<CacheBlk> * a linear array of all the sets in cache
 *          sets = new Cacheset<CacheBlk>[numSets]
 * blks => pointer of type CacheBlk * a linear array of all the blocks in the cache
 *          blks = new CacheBlk[numSets * assoc]
 * dataBlks => pointer fo type uint8_t * a linear array of actual storage of data
 *              dataBlks = new uint8_t[numSets * assoc * blkSize];
 * setShift => amount to shift the address to get the set
 * tagShift => amount to shift the address to get the tag
 * setMask => Mask out all bits that aren't part of the set index
 * blkMask => Mask out all bits that aren't part of the block offset
 * function invalidate => Invalidate the given block pass parameter of type CacheBlk
 *
 */

/* lokeshjindal15 changes for scaling associativity
 * like in Cache class ../cache_impl.hh
 * the function tags->forEachBlock is called inside block visitor function
 * for invalidate and writebacks
 * use exactly same architecture just change forEachBlock to
 * a new function like forEachScaledBlock...
 * and the functions writeback and invalidate can also be modified to their scaling versions 
 * in cahce_impl.hh and these scaling versions can be called inside one main function called 
 * scale_cache_down()
 */

/**
 * A BaseSetAssoc cache tag store.
 * @sa  \ref gem5MemorySystem "gem5 Memory System"
 *
 * The BaseSetAssoc tags provide a base, as well as the functionality
 * common to any set associative tags. Any derived class must implement
 * the methods related to the specifics of the actual replacment policy.
 * These are:
 *
 * BlkType* accessBlock();
 * BlkType* findVictim();
 * void insertBlock();
 * void invalidate();
 */
class BaseSetAssoc : public BaseTags
{
  public:
    /** Typedef the block type used in this tag store. */
    typedef CacheBlk BlkType;
    /** Typedef for a list of pointers to the local block class. */
    typedef std::list<BlkType*> BlkList;
    /** Typedef the set type used in this tag store. */
    typedef CacheSet<CacheBlk> SetType;


  protected:
    /** The associativity of the cache. */
    //const unsigned assoc;
    unsigned assoc;//lokeshjindal15 was originally defined as const as above
    unsigned orig_assoc;//lokeshjindal15 was originally defined as const as above
    /** The number of sets in the cache. */
    const unsigned numSets;
    /** Whether tags and data are accessed sequentially. */
    const bool sequentialAccess;

    /** The cache sets. */
    SetType *sets;

  protected:
  public://lokeshjindal15 TODO FIXME was protected originally
    /** The cache blocks. */
    BlkType *blks;
  protected:
    /** The data blocks, 1 per cache block. */
    uint8_t *dataBlks;

    /** The amount to shift the address to get the set. */
    int setShift;
    /** The amount to shift the address to get the tag. */
    int tagShift;
    /** Mask out all bits that aren't part of the set index. */
    unsigned setMask;
    /** Mask out all bits that aren't part of the block offset. */
    unsigned blkMask;

public:

    /** Convenience typedef. */
     typedef BaseSetAssocParams Params;

    /**
     * Construct and initialize this tag store.
     */
    BaseSetAssoc(const Params *p);

    /**
     * Destructor
     */
    virtual ~BaseSetAssoc();

    /**
     * Return the block size.
     * @return the block size.
     */
    unsigned
    getBlockSize() const
    {
        return blkSize;
    }

    /**
     * Return the subblock size. In the case of BaseSetAssoc it is always
     * the block size.
     * @return The block size.
     */
    unsigned
    getSubBlockSize() const
    {
        return blkSize;
    }

    /**
     * Invalidate the given block.
     * @param blk The block to invalidate.
     */
    void invalidate(BlkType *blk)
    {
        assert(blk);
        assert(blk->isValid());
        tagsInUse--;
        assert(blk->srcMasterId < cache->system->maxMasters());
        occupancies[blk->srcMasterId]--;
        blk->srcMasterId = Request::invldMasterId;
        blk->task_id = ContextSwitchTaskId::Unknown;
        blk->tickInserted = curTick();
    }

    /**
     * Access block and update replacement data. May not succeed, in which case
     * NULL pointer is returned. This has all the implications of a cache
     * access and should only be used as such. Returns the access latency as a
     * side effect.
     * @param addr The address to find.
     * @param is_secure True if the target memory space is secure.
     * @param asid The address space ID.
     * @param lat The access latency.
     * @return Pointer to the cache block if found.
     */
    BlkType* accessBlock(Addr addr, bool is_secure, Cycles &lat,
                                 int context_src)
    {
        Addr tag = extractTag(addr);
        int set = extractSet(addr);
        BlkType *blk = sets[set].findBlk(tag, is_secure);
        lat = hitLatency;

        // Access all tags in parallel, hence one in each way.  The data side
        // either accesses all blocks in parallel, or one block sequentially on
        // a hit.  Sequential access with a miss doesn't access data.
        tagAccesses += assoc;
        if (sequentialAccess) {
            if (blk != NULL) {
                dataAccesses += 1;
            }
        } else {
            dataAccesses += assoc;
        }

        if (blk != NULL) {
            if (blk->whenReady > curTick()
                && cache->ticksToCycles(blk->whenReady - curTick())
                > hitLatency) {
                lat = cache->ticksToCycles(blk->whenReady - curTick());
            }
            blk->refCount += 1;
        }

        return blk;
    }

    /**
     * Finds the given address in the cache, do not update replacement data.
     * i.e. This is a no-side-effect find of a block.
     * @param addr The address to find.
     * @param is_secure True if the target memory space is secure.
     * @param asid The address space ID.
     * @return Pointer to the cache block if found.
     */
    BlkType* findBlock(Addr addr, bool is_secure) const;

    /**
     * Find an invalid block to evict for the address provided.
     * If there are no invalid blocks, this will return the block
     * in the least-recently-used position.
     * @param addr The addr to a find a replacement candidate for.
     * @return The candidate block.
     */
    BlkType* findVictim(Addr addr) const
    {
        BlkType *blk = NULL;
        int set = extractSet(addr);

        // prefer to evict an invalid block
        for (int i = 0; i < assoc; ++i) {
            blk = sets[set].blks[i];
            if (!blk->isValid()) {
                break;
            }
        }

        return blk;
    }

    /**
     * Insert the new block into the cache.
     * @param pkt Packet holding the address to update
     * @param blk The block to update.
     */
     void insertBlock(PacketPtr pkt, BlkType *blk)
     {
         Addr addr = pkt->getAddr();
         MasterID master_id = pkt->req->masterId();
         uint32_t task_id = pkt->req->taskId();

         if (!blk->isTouched) {
             tagsInUse++;
             blk->isTouched = true;
             if (!warmedUp && tagsInUse.value() >= warmupBound) {
                 warmedUp = true;
                 warmupCycle = curTick();
             }
         }

         // If we're replacing a block that was previously valid update
         // stats for it. This can't be done in findBlock() because a
         // found block might not actually be replaced there if the
         // coherence protocol says it can't be.
         if (blk->isValid()) {
             replacements[0]++;
             totalRefs += blk->refCount;
             ++sampledRefs;
             blk->refCount = 0;

             // deal with evicted block
             assert(blk->srcMasterId < cache->system->maxMasters());
             occupancies[blk->srcMasterId]--;

             blk->invalidate();
         }

         blk->isTouched = true;

         // Set tag for new block.  Caller is responsible for setting status.
         blk->tag = extractTag(addr);

         // deal with what we are bringing in
         assert(master_id < cache->system->maxMasters());
         occupancies[master_id]++;
         blk->srcMasterId = master_id;
         blk->task_id = task_id;
         blk->tickInserted = curTick();

         // We only need to write into one tag and one data block.
         tagAccesses += 1;
         dataAccesses += 1;
     }

    /**
     * Generate the tag from the given address.
     * @param addr The address to get the tag from.
     * @return The tag of the address.
     */
    Addr extractTag(Addr addr) const
    {
        return (addr >> tagShift);
    }

    /**
     * Calculate the set index from the address.
     * @param addr The address to get the set from.
     * @return The set index of the address.
     */
    int extractSet(Addr addr) const
    {
        return ((addr >> setShift) & setMask);
    }

    /**
     * Get the block offset from an address.
     * @param addr The address to get the offset of.
     * @return The block offset.
     */
    int extractBlkOffset(Addr addr) const
    {
        return (addr & blkMask);
    }

    /**
     * Align an address to the block size.
     * @param addr the address to align.
     * @return The block address.
     */
    Addr blkAlign(Addr addr) const
    {
        return (addr & ~(Addr)blkMask);
    }

    /**
     * Regenerate the block address from the tag.
     * @param tag The tag of the block.
     * @param set The set of the block.
     * @return The block address.
     */
    Addr regenerateBlkAddr(Addr tag, unsigned set) const
    {
        return ((tag << tagShift) | ((Addr)set << setShift));
    }

    /**
     * Return the hit latency.
     * @return the hit latency.
     */
    Cycles getHitLatency() const
    {
        return hitLatency;
    }
    /**
     *iterated through all blocks and clear all locks
     *Needed to clear all lock tracking at once
     */
    virtual void clearLocks();

    /**
     * Called at end of simulation to complete average block reference stats.
     */
    virtual void cleanupRefs();

    /**
     * Print all tags used
     */
    virtual std::string print() const;

    /**
     * Called prior to dumping stats to compute task occupancy
     */
    virtual void computeStats();

    /**
     * Visit each block in the tag store and apply a visitor to the
     * block.
     *
     * The visitor should be a function (or object that behaves like a
     * function) that takes a cache block reference as its parameter
     * and returns a bool. A visitor can request the traversal to be
     * stopped by returning false, returning true causes it to be
     * called for the next block in the tag store.
     *
     * \param visitor Visitor to call on each block.
     */
    template <typename V>
    void forEachBlk(V &visitor) {
        //for (unsigned i = 0; i < numSets * assoc; ++i) {
        for (unsigned i = 0; i < numSets * orig_assoc; ++i) {
            if (!visitor(blks[i]))
                return;
        }
    }
    
    template <typename v>
    void forEachBlkScaleDown(v &visitor, unsigned tfscalefac) {//lokeshjindal15
        unsigned new_assoc = assoc/tfscalefac;
        //unsigned checkedinset = 0;
        //for (unsigned i = new_assoc; i < numSets * assoc; ++i) {
        //for (unsigned i = 0; i < numSets * assoc; ++i) {//TODO FIXME uncomment above
        //    std::cout << std::endl<< " foreachblkscaleDown: checking i:" << i << " addr:" << &blks[i] << " "  << blks[i].print() << std::endl;//TODO FIXME remove addr
           //assert(checkedinset <= (assoc - new_assoc));
           //if (checkedinset != (assoc - new_assoc))
           //{ 
           //    std::cout << "dispatching blk:" << i << std::endl;
           //    checkedinset++; 
           //    if (!visitor(blks[i]))
           //     {
           //         warn_once("base_set_assoc.hh: foreachblkscaleDown: stopped at block i:%d numSets:%d assoc:%di new_assoc:%d\n", i, numSets, assoc, new_assoc);
           //         return;
           //     }
           //}
           //else
           //{
           //    checkedinset = 0;
           //    i = i + new_assoc - 1;
           //}
        //}
        for (unsigned i = 0; i < numSets; i++)
        {
            for (unsigned j = new_assoc; j < assoc; j++)
            {
                if(!visitor(*(sets[i].blks[j])))
                    return;
            }
        }
    }

    template <typename v>
    void forEachBlkScaleUp(v &visitor, unsigned tfscalefac) {//lokeshjindal15
        unsigned new_assoc = assoc * tfscalefac;
        //unsigned checkedinset = 0;
        //for (unsigned i = assoc; i < numSets * new_assoc; ++i) {
        //   assert(checkedinset <= (new_assoc - assoc));
        //   if (checkedinset != (new_assoc - assoc))
        //   { 
        //       std::cout << "foreachblkscaleUp: dispatching blk:" << i << std::endl;
        //       checkedinset++; 
        //       visitor(blks[i]);
        //      if (!visitor(blks[i]))
        //        {
        //            warn_once("base_set_assoc.hh: foreachblkscaleDown: stopped at block i:%d numSets:%d assoc:%di new_assoc:%d\n", i, numSets, assoc, new_assoc);
        //            return;
        //        }  
        //   }
        //   else
        //   {
        //       checkedinset = 0;
        //       i = i + assoc - 1;
        //   }
        //}
        for (unsigned i = 0; i < numSets; i++)
        {
            for (unsigned j = assoc; j < new_assoc; j++)
            {
                if(!visitor(*(sets[i].blks[j])))
                    return;
            }
        }
    }

    void updateAssocDown(unsigned tfscalefac)
    {
        assoc /= tfscalefac;
    }
    
    void updateAssocUp(unsigned tfscalefac)
    {
        assoc *= tfscalefac;
    }
    
    void updateAssocCachesets()
    {
        for (unsigned i = 0; i < numSets; ++i)
        {
            sets[i].assoc = assoc;
        }
    }
};

#endif // __MEM_CACHE_TAGS_BASESETASSOC_HH__
