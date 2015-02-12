/*
 * Copyright (c) 2004-2005 The Regents of The University of Michigan
 * Copyright (c) 2013 Advanced Micro Devices, Inc.
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
 * Authors: Kevin Lim
 */

#include <vector>

#include "cpu/o3/rename_map.hh"
#include "debug/Rename.hh"

using namespace std;

/**** SimpleRenameMap methods ****/

SimpleRenameMap::SimpleRenameMap()
    : freeList(NULL)
{
}


void
SimpleRenameMap::init(unsigned size, SimpleFreeList *_freeList,
                      RegIndex _zeroReg)
{
    assert(freeList == NULL);
    assert(map.empty());

    map.resize(size);
    freeList = _freeList;
    zeroReg = _zeroReg;
}

SimpleRenameMap::RenameInfo
SimpleRenameMap::rename(RegIndex arch_reg)
{
    PhysRegIndex renamed_reg;

    // Record the current physical register that is renamed to the
    // requested architected register.
    PhysRegIndex prev_reg = map[arch_reg];

    // If it's not referencing the zero register, then rename the
    // register.
    if (arch_reg != zeroReg) {
        renamed_reg = freeList->getReg();

        map[arch_reg] = renamed_reg;
    } else {
        // Otherwise return the zero register so nothing bad happens.
        assert(prev_reg == zeroReg);
        renamed_reg = zeroReg;
    }

    DPRINTF(Rename, "Renamed reg %d to physical reg %d old mapping was %d\n",
            arch_reg, renamed_reg, prev_reg);

    return RenameInfo(renamed_reg, prev_reg);
}


SimpleRenameMap::RenameInfo
SimpleRenameMap::restrict_rename(RegIndex arch_reg)//lokeshjindal15
{
    PhysRegIndex renamed_reg;

    // Record the current physical register that is renamed to the
    // requested architected register.
    PhysRegIndex prev_reg = map[arch_reg];

    // If it's not referencing the zero register, then rename the
    // register.
    if (arch_reg != zeroReg) {
    	std::cout << "before numFreeRegs:" << freeList->numFreeRegs() << std::endl;
        renamed_reg = freeList->getReg();
	
        map[arch_reg] = renamed_reg;
	freeList->addReg(prev_reg);
    	std::cout << "after numFreeRegs:" << freeList->numFreeRegs() << std::endl;
    } else {
        // Otherwise return the zero register so nothing bad happens.
        assert(prev_reg == zeroReg);
        renamed_reg = zeroReg;
    }
    DPRINTF(Rename, "Renamed reg %d to physical reg %d old mapping was %d\n",
            arch_reg, renamed_reg, prev_reg);

    return RenameInfo(renamed_reg, prev_reg);
}
void
SimpleRenameMap::simple_print_mapping(unsigned max_regs)
{
	std::cout << "*****TRANSFORM simple print mappings! max_regs:" << max_regs << std::endl;
	auto iter = map.begin();
	RegIndex i =0;
	while ((iter != map.end()) && (i < max_regs))
	{
		std::cout << "Archreg:" << i << " Physreg:" << map[i] << " lookup:" << lookup(i) << endl;
		iter++;
		i++;
	}
	assert(i == max_regs);
}

void
SimpleRenameMap::compact_regmapping(PhysRegIndex subvalue)
{
	assert(subvalue >= 0);
	auto iter = map.begin();
	RegIndex i =0;
	while (iter != map.end())
	{
		map[i] -= subvalue;
		iter++;
		i++;
	}
}
			
/**** UnifiedRenameMap methods ****/

void
UnifiedRenameMap::init(PhysRegFile *_regFile,
                       RegIndex _intZeroReg,
                       RegIndex _floatZeroReg,
                       UnifiedFreeList *freeList)
{
    regFile = _regFile;

    intMap.init(TheISA::NumIntRegs, &(freeList->intList), _intZeroReg);

    floatMap.init(TheISA::NumFloatRegs, &(freeList->floatList), _floatZeroReg);

    ccMap.init(TheISA::NumFloatRegs, &(freeList->ccList), (RegIndex)-1);
}


UnifiedRenameMap::RenameInfo
UnifiedRenameMap::rename(RegIndex arch_reg)
{
    RegIndex rel_arch_reg;

    switch (regIdxToClass(arch_reg, &rel_arch_reg)) {
      case IntRegClass:
        return renameInt(rel_arch_reg);

      case FloatRegClass:
        return renameFloat(rel_arch_reg);

      case CCRegClass:
        return renameCC(rel_arch_reg);

      case MiscRegClass:
        return renameMisc(rel_arch_reg);

      default:
        panic("rename rename(): unknown reg class %s\n",
              RegClassStrings[regIdxToClass(arch_reg)]);
    }
}


PhysRegIndex
UnifiedRenameMap::lookup(RegIndex arch_reg) const
{
    RegIndex rel_arch_reg;

    switch (regIdxToClass(arch_reg, &rel_arch_reg)) {
      case IntRegClass:
        return lookupInt(rel_arch_reg);

      case FloatRegClass:
        return lookupFloat(rel_arch_reg);

      case CCRegClass:
        return lookupCC(rel_arch_reg);

      case MiscRegClass:
        return lookupMisc(rel_arch_reg);

      default:
        panic("rename lookup(): unknown reg class %s\n",
              RegClassStrings[regIdxToClass(arch_reg)]);
    }
}

void
UnifiedRenameMap::setEntry(RegIndex arch_reg, PhysRegIndex phys_reg)
{
    RegIndex rel_arch_reg;

    switch (regIdxToClass(arch_reg, &rel_arch_reg)) {
      case IntRegClass:
        return setIntEntry(rel_arch_reg, phys_reg);

      case FloatRegClass:
        return setFloatEntry(rel_arch_reg, phys_reg);

      case CCRegClass:
        return setCCEntry(rel_arch_reg, phys_reg);

      case MiscRegClass:
        // Misc registers do not actually rename, so don't change
        // their mappings.  We end up here when a commit or squash
        // tries to update or undo a hardwired misc reg nmapping,
        // which should always be setting it to what it already is.
        assert(phys_reg == lookupMisc(rel_arch_reg));
        return;

      default:
        panic("rename setEntry(): unknown reg class %s\n",
              RegClassStrings[regIdxToClass(arch_reg)]);
    }
}


void
UnifiedRenameMap::unified_print_mapping()
{
	std::cout << "*****TRANSFORM going to print the reg mappings now!" << std::endl;
	std::cout << "*****INT REG mappings total TheISA::NumIntRegs:" << TheISA::NumIntRegs  << std::endl;
	//intMap.simple_print_mapping((unsigned) TheISA::NumIntRegs);

	for (RegIndex i = 0; i < TheISA::NumIntRegs; i++)
	{
		std::cout << "Archreg:" << i << " Phyreg lookup:" << lookupInt(i) << " value:" << regFile->readIntReg(lookupInt(i)) << endl;
	}
	//assert(i == max_regs);

	std::cout << "*****FLOAT REG mappings total TheISA::NumFloatRegs:" << TheISA::NumFloatRegs << std::endl;
	//floatMap.simple_print_mapping((unsigned) TheISA::NumFloatRegs);
	for (RegIndex i = 0; i < TheISA::NumFloatRegs; i++)
	{
		std::cout << "Archreg:" << i << " Phyreg lookup:" << lookupFloat(i) << " value:" << regFile->readFloatReg(lookupFloat(i)) << endl;
	}
	//assert(i == max_regs);
	
	std::cout << "*****CC REG mappings total TheISA::NumCCRegs:" << TheISA::NumCCRegs << std::endl;
	//ccMap.simple_print_mapping((unsigned) TheISA::NumCCRegs);
	for (RegIndex i = 0; i < TheISA::NumCCRegs; i++)
	{
		std::cout << "Archreg:" << i << " Phyreg lookup:" << lookupCC(i) << " value:" << regFile->readCCReg(lookupCC(i)) << endl;
	}
	//assert(i == max_regs);
}

/*for every arch reg, look up the phy reg and check if it will be "valid" for scaled regfile.
 * If not, get a new phy reg from free list and the same time,
 * copy the value from old phy reg to new phy reg.
 * Check if this new phy reg is valid , If not, get another free reg and repeat until you find one*/
void
UnifiedRenameMap::restrict_intreg_mapping(unsigned tf_regfile_scale_factor)
{
	std::cout << "restrict_intreg_mapping BEFORE numFreeEntries:" << numFreeEntries() << std::endl;
	for (RegIndex i = 0; i < TheISA::NumIntRegs; i++)
	{
		RegIndex phy = lookupInt(i);
		if (phy < (regFile->numIntPhysRegs()/tf_regfile_scale_factor))//TODO FIXME check < or <=
		{
			std::cout << "restrict_intreg_mapping: Skipping phy:" << phy << " against regfile->numIntPhysRegs():" << regFile->numIntPhysRegs() << " with tf_regfile_scale_factor:" << tf_regfile_scale_factor << endl;
		}
		else
		{
			std::cout << "restrict_intreg_mapping: REMAPPING phy:" << phy << " against regfile->numIntPhysRegs():" << regFile->numIntPhysRegs() << " with tf_regfile_scale_factor:" << tf_regfile_scale_factor << endl;
			while(1)
			{
			RenameInfo newpair = restrict_renameInt(i);
			uint64_t value = regFile->readIntReg(newpair.second);
			regFile->setIntReg(newpair.first,value);
			if (newpair.first < (regFile->numIntPhysRegs()/tf_regfile_scale_factor))//TODO FIXME check < or <=
			{
				std::cout << "ACCEPTING new phy:" << newpair.first << "old phy:" << newpair.second << std::endl;
				break;
			}
			else	
			{
				std::cout << "REJECTING new phy:" << newpair.first << "old phy:" << newpair.second << std::endl;
			}	
			}
		}
	}
	std::cout << "restrict_intreg_mapping AFTER numFreeEntries:" << numFreeEntries() << std::endl;
}

/*for every arch reg, look up the phy reg and check if it will be "valid" for scaled regfile.
 * If not, get a new phy reg from free list and the same time,
 * copy the value from old phy reg to new phy reg.
 * Check if this new phy reg is valid , If not, get another free reg and repeat until you find one*/
void
UnifiedRenameMap::restrict_floatreg_mapping(unsigned tf_regfile_scale_factor)
{
	std::cout << "restrict_floatreg_mapping BEFORE numFreeEntries:" << numFreeEntries() << std::endl;
	for (RegIndex i = 0; i < TheISA::NumFloatRegs; i++)
	{
		RegIndex phy = lookupFloat(i);
		if (phy < (regFile->numIntPhysRegs() + regFile->numFloatPhysRegs()/tf_regfile_scale_factor))//TODO FIXME check < or <=
		{
			std::cout << "restrict_floatreg_mapping: Skipping phy:" << phy << " against regFile->numIntPhysRegs() + regfile->numFloatPhysRegs():" << (regFile->numIntPhysRegs() +regFile->numFloatPhysRegs()) << " with tf_regfile_scale_factor:" << tf_regfile_scale_factor << endl;
		}
		else
		{
			std::cout << "restrict_floatreg_mapping: REMAPPING phy:" << phy << " against regFile->numIntPhysRegs() + regfile->numFloatPhysRegs():" << (regFile->numIntPhysRegs() + regFile->numFloatPhysRegs()) << " with tf_regfile_scale_factor:" << tf_regfile_scale_factor << endl;
			while(1)
			{
			RenameInfo newpair = restrict_renameFloat(i);
			FloatReg value = regFile->readFloatReg(newpair.second);
			regFile->setFloatReg(newpair.first,value);
			if (newpair.first < (regFile->numIntPhysRegs() + regFile->numFloatPhysRegs()/tf_regfile_scale_factor))//TODO FIXME check < or <=
			{
				std::cout << "ACCEPTING new phy:" << newpair.first << "old phy:" << newpair.second << std::endl;
				break;
			}
			else	
			{
				std::cout << "REJECTING new phy:" << newpair.first << "old phy:" << newpair.second << std::endl;
			}	
			}
		}
	}
	std::cout << "restrict_floatreg_mapping AFTER numFreeEntries:" << numFreeEntries() << std::endl;
}

/*for every arch reg, look up the phy reg and check if it will be "valid" for scaled regfile.
 * If not, get a new phy reg from free list and the same time,
 * copy the value from old phy reg to new phy reg.
 * Check if this new phy reg is valid , If not, get another free reg and repeat until you find one*/
void
UnifiedRenameMap::restrict_ccreg_mapping(unsigned tf_regfile_scale_factor)
{
	std::cout << "restrict_ccreg_mapping BEFORE numFreeEntries:" << numFreeEntries() << std::endl;
	for (RegIndex i = 0; i < TheISA::NumCCRegs; i++)
	{
		RegIndex phy = lookupCC(i);
		if (phy < (regFile->numIntPhysRegs() + regFile->numFloatPhysRegs()+ regFile->numCCPhysRegs()/tf_regfile_scale_factor))//TODO FIXME check < or <=
		{
			std::cout << "restrict_ccreg_mapping: Skipping phy:" << phy << " against regFile->numIntPhysRegs() + regFile->numFloatPhysRegs()+ regfile->numCCPhysRegs():" << (regFile->numIntPhysRegs() + regFile->numFloatPhysRegs()+ regFile->numCCPhysRegs()) << " with tf_regfile_scale_factor:" << tf_regfile_scale_factor << endl;
		}
		else
		{
			std::cout << "restrict_ccreg_mapping: REMAPPING phy:" << phy << " against regFile->numIntPhysRegs() + regFile->numFloatPhysRegs()+ regfile->numCCPhysRegs():" << (regFile->numIntPhysRegs() + regFile->numFloatPhysRegs()+  regFile->numCCPhysRegs()) << " with tf_regfile_scale_factor:" << tf_regfile_scale_factor << endl;
			while(1)
			{
			RenameInfo newpair = restrict_renameCC(i);
			CCReg value = regFile->readCCReg(newpair.second);
			regFile->setCCReg(newpair.first,value);
			if (newpair.first < (regFile->numIntPhysRegs() + regFile->numFloatPhysRegs()+ regFile->numCCPhysRegs()/tf_regfile_scale_factor))//TODO FIXME check < or <=
			{
				std::cout << "ACCEPTING new phy:" << newpair.first << "old phy:" << newpair.second << std::endl;
				break;
			}
			else	
			{
				std::cout << "REJECTING new phy:" << newpair.first << "old phy:" << newpair.second << std::endl;
			}	
			}
		}
	}
	std::cout << "restrict_ccreg_mapping AFTER numFreeEntries:" << numFreeEntries() << std::endl;
} 

void
UnifiedRenameMap::compact_regmapping()
{
	//call compact_regmapping of each map with correct parameter
	intMap.compact_regmapping(0);
	//(old_baseFloatRegIndex - baseFloatRegIndex)
	floatMap.compact_regmapping(regFile->old_numIntPhysRegs() - regFile->numIntPhysRegs());
	//(old_baseCCRegIndex - baseCCRegIndex)
	ccMap.compact_regmapping(regFile->old_numFloatPhysRegs() + regFile->old_numIntPhysRegs() - regFile->numFloatPhysRegs() - regFile->numIntPhysRegs());
}
