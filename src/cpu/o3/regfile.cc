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
 *          Gabe Black
 *          Steve Reinhardt
 */

#include "cpu/o3/free_list.hh"
#include "cpu/o3/regfile.hh"


PhysRegFile::PhysRegFile(unsigned _numPhysicalIntRegs,
                         unsigned _numPhysicalFloatRegs,
                         unsigned _numPhysicalCCRegs)
    : intRegFile(_numPhysicalIntRegs),
      floatRegFile(_numPhysicalFloatRegs),
      ccRegFile(_numPhysicalCCRegs),
      baseFloatRegIndex(_numPhysicalIntRegs),
      baseCCRegIndex(_numPhysicalIntRegs + _numPhysicalFloatRegs),
      totalNumRegs(_numPhysicalIntRegs
                   + _numPhysicalFloatRegs
                   + _numPhysicalCCRegs)
{
    std::cout << "*****TRANSFORM totalNumRegs = " << _numPhysicalIntRegs << "+" << _numPhysicalFloatRegs << "+" << _numPhysicalCCRegs << std::endl;
	if (TheISA::NumCCRegs == 0 && _numPhysicalCCRegs != 0) {
        // Just make this a warning and go ahead and allocate them
        // anyway, to keep from having to add checks everywhere
        warn("Non-zero number of physical CC regs specified, even though\n"
             "    ISA does not use them.\n");
    }
    old_baseFloatRegIndex = -1;//lokeshjindal15
    old_baseCCRegIndex = -1;//lokeshjindal15
    old_totalNumRegs = -1;//lokeshjindal15
    scaled = false;
	
    init_misc_totalNumRegs = _numPhysicalIntRegs + _numPhysicalFloatRegs + _numPhysicalCCRegs;

}


void
PhysRegFile::initFreeList(UnifiedFreeList *freeList)
{
    // Initialize the free lists.
    PhysRegIndex reg_idx = 0;

    // The initial batch of registers are the integer ones
    while (reg_idx < baseFloatRegIndex) {
        freeList->addIntReg(reg_idx++);
    }

    // The next batch of the registers are the floating-point physical
    // registers; put them onto the floating-point free list.
    while (reg_idx < baseCCRegIndex) {
        freeList->addFloatReg(reg_idx++);
    }

    // The rest of the registers are the condition-code physical
    // registers; put them onto the condition-code free list.
    while (reg_idx < totalNumRegs) {
        freeList->addCCReg(reg_idx++);
    }

	std::cout << "*********TRANSFORM REG_IDX after init is " << reg_idx << std::endl;

}

/*Here we capture the old boundary markers of the regfile. 
 * Next we scan the freelist element by element, check that the phy reg is 
 * actually valid and if yes, then we store it back on the freelist else remove. 
 * Also we resize the actual vector (C++ memory in gem5) for the regfile after scaling. 
 * The new value pushed, needs to be shifted to the left since the number of regs in left partition has changed 
 * (intregs for float free regs and int+float regs for cc free regs).*/
void
PhysRegFile::scale_regfile (unsigned int_scale_factor, unsigned float_scale_factor, unsigned cc_scale_factor, UnifiedFreeList *freeList)
{
	old_baseFloatRegIndex = baseFloatRegIndex;
	baseFloatRegIndex /= int_scale_factor;
	old_baseCCRegIndex = baseCCRegIndex;
	baseCCRegIndex = baseFloatRegIndex + (old_baseCCRegIndex - old_baseFloatRegIndex)/float_scale_factor;
	old_totalNumRegs = totalNumRegs;	
	totalNumRegs = baseCCRegIndex + (old_totalNumRegs - old_baseCCRegIndex)/cc_scale_factor;

	int list_size = ((freeList->getIntList())->getfreeRegs())->size();
	for (int i =0; i < list_size; i++)
	{
		PhysRegIndex phy_reg = ((freeList->getIntList())->getfreeRegs())->front();
			((freeList->getIntList())->getfreeRegs())->pop();
		if (phy_reg < baseFloatRegIndex)//TODO FIXME check < or <=
		{
			((freeList->getIntList())->getfreeRegs())->push(phy_reg);
		}
	}
	//resizeintRegFile(baseFloatRegIndex);//TODO FIXME confirm that resize simply chopps off the nodes from the end while preserving values at the front
	
	list_size = ((freeList->getFloatList())->getfreeRegs())->size();
	for (int i =0; i < list_size; i++)
	{
		PhysRegIndex phy_reg = ((freeList->getFloatList())->getfreeRegs())->front();
			((freeList->getFloatList())->getfreeRegs())->pop();
		if (phy_reg < (old_baseFloatRegIndex + (old_baseCCRegIndex - old_baseFloatRegIndex)/float_scale_factor))//TODO FIXME check < or <=
		{
			PhysRegIndex newval = phy_reg - (old_baseFloatRegIndex - baseFloatRegIndex);//subtract the void created by change in registers to the left i.e. the int regs.
			assert(newval >= baseFloatRegIndex);
			((freeList->getFloatList())->getfreeRegs())->push(newval);
			
		}
	}
	//resizefloatRegFile((old_baseCCRegIndex - old_baseFloatRegIndex)/float_scale_factor);//TODO FIXME confirm that resize simply chopps off the nodes from the end while preserving values at the front
	
	list_size = ((freeList->getCCList())->getfreeRegs())->size();
	for (int i =0; i < list_size; i++)
	{
		PhysRegIndex phy_reg = ((freeList->getCCList())->getfreeRegs())->front();
			((freeList->getCCList())->getfreeRegs())->pop();
		if (phy_reg < (old_baseCCRegIndex + (totalNumRegs - old_baseCCRegIndex)/cc_scale_factor))//TODO FIXME check < or <=
		{
			PhysRegIndex newval = phy_reg - (old_baseCCRegIndex - baseCCRegIndex);//subtract the void created by change in registers to the left i.e. the int+float regs.;
			assert(newval >= baseCCRegIndex);
			((freeList->getCCList())->getfreeRegs())->push(newval);
			
		}
	}
}
 
/*Here we capture the old boundary markers of the regfile. 
 * Next we scan the freelist element by element, check that the phy reg is 
 * actually valid and if yes, then we store it back on the freelist else remove. 
 * Also we resize the actual vector (C++ memory in gem5) for the regfile after scaling. 
 * The new value pushed, needs to be shifted to the left since the number of regs in left partition has changed 
 * (intregs for float free regs and int+float regs for cc free regs).*/
void
PhysRegFile::scale_up_regfile (unsigned int_scale_factor, unsigned float_scale_factor, unsigned cc_scale_factor, UnifiedFreeList *freeList)
{
	old_baseFloatRegIndex = baseFloatRegIndex;
	baseFloatRegIndex *= int_scale_factor;
	old_baseCCRegIndex = baseCCRegIndex;
	baseCCRegIndex = baseFloatRegIndex + (old_baseCCRegIndex - old_baseFloatRegIndex)*float_scale_factor;
	old_totalNumRegs = totalNumRegs;	
	totalNumRegs = baseCCRegIndex + (old_totalNumRegs - old_baseCCRegIndex)*cc_scale_factor;

	int list_size = ((freeList->getIntList())->getfreeRegs())->size();
	for (int i =0; i < list_size; i++)
	{
		PhysRegIndex phy_reg = ((freeList->getIntList())->getfreeRegs())->front();
			((freeList->getIntList())->getfreeRegs())->pop();
		assert (phy_reg < old_baseFloatRegIndex);//TODO FIXME check < or <=
		//if (phy_reg < baseFloatRegIndex)//TODO FIXME check < or <=
		//{
			((freeList->getIntList())->getfreeRegs())->push(phy_reg);
		//}	
	}
	for (PhysRegIndex phy_reg = old_baseFloatRegIndex; phy_reg < baseFloatRegIndex; phy_reg++)
	{
		((freeList->getIntList())->getfreeRegs())->push(phy_reg);
	}
	//resizeintRegFile(baseFloatRegIndex);//TODO FIXME confirm that resize simply chopps off the nodes from the end while preserving values at the front
	
	list_size = ((freeList->getFloatList())->getfreeRegs())->size();
	for (int i =0; i < list_size; i++)
	{
		PhysRegIndex phy_reg = ((freeList->getFloatList())->getfreeRegs())->front();
			((freeList->getFloatList())->getfreeRegs())->pop();
		assert ((phy_reg < old_baseCCRegIndex) && (phy_reg >= old_baseFloatRegIndex));//TODO FIXME check < or <=
		//if (phy_reg < (old_baseFloatRegIndex + (old_baseCCRegIndex - old_baseFloatRegIndex)/float_scale_factor))//TODO FIXME check < or <=
		//{
			PhysRegIndex newval = phy_reg + (baseFloatRegIndex - old_baseFloatRegIndex);//add the void created by change in registers to the left i.e. the int regs.
			assert((newval >= baseFloatRegIndex) && (newval < baseCCRegIndex));
			((freeList->getFloatList())->getfreeRegs())->push(newval);
			
		//}
	}
	for (PhysRegIndex phy_reg = (baseFloatRegIndex + old_baseCCRegIndex - old_baseFloatRegIndex); phy_reg < baseCCRegIndex; phy_reg++)
	{
		((freeList->getFloatList())->getfreeRegs())->push(phy_reg);
	}
	//resizefloatRegFile((old_baseCCRegIndex - old_baseFloatRegIndex)/float_scale_factor);//TODO FIXME confirm that resize simply chopps off the nodes from the end while preserving values at the front
	
	list_size = ((freeList->getCCList())->getfreeRegs())->size();
	for (int i =0; i < list_size; i++)
	{
		PhysRegIndex phy_reg = ((freeList->getCCList())->getfreeRegs())->front();
			((freeList->getCCList())->getfreeRegs())->pop();
		assert ((phy_reg < old_totalNumRegs) && (phy_reg >= old_baseCCRegIndex));//TODO FIXME check < or <=
		//if (phy_reg < (old_baseCCRegIndex + (totalNumRegs - old_baseCCRegIndex)/cc_scale_factor))//TODO FIXME check < or <=
		//{
			PhysRegIndex newval = phy_reg + (baseCCRegIndex - old_baseCCRegIndex );//add the void created by change in registers to the left i.e. the int+float regs.;
			assert((newval >= baseCCRegIndex) && (phy_reg < totalNumRegs));
			((freeList->getCCList())->getfreeRegs())->push(newval);
			
		//}
	}
	for (PhysRegIndex phy_reg = (baseCCRegIndex + old_totalNumRegs - old_baseCCRegIndex) ; phy_reg < totalNumRegs; phy_reg++)
	{
		((freeList->getCCList())->getfreeRegs())->push(phy_reg);
	}
}
