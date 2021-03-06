// addrspace.cc 
//	Routines to manage address spaces (executing user programs).
//
//	In order to run a user program, you must:
//
//	1. link with the -N -T 0 option 
//	2. run coff2noff to convert the object file to Nachos format
//		(Nachos object code format is essentially just a simpler
//		version of the UNIX executable object code format)
//	3. load the NOFF file into the Nachos file system
//		(if you haven't implemented the file system yet, you
//		don't need to do this last step)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "addrspace.h"
#include "machine.h"

//----------------------------------------------------------------------
// SwapHeader
// 	Do little endian to big endian conversion on the bytes in the 
//	object file header, in case the file was generated on a little
//	endian machine, and we're now running on a big endian machine.
//----------------------------------------------------------------------

extern void pt();
//extern int thPID[NumPhysPages];
//extern int thVPN[NumPhysPages];

static void 
SwapHeader (NoffHeader *noffH)
{
	noffH->noffMagic = WordToHost(noffH->noffMagic);
	noffH->code.size = WordToHost(noffH->code.size);
	noffH->code.virtualAddr = WordToHost(noffH->code.virtualAddr);
	noffH->code.inFileAddr = WordToHost(noffH->code.inFileAddr);
	noffH->initData.size = WordToHost(noffH->initData.size);
	noffH->initData.virtualAddr = WordToHost(noffH->initData.virtualAddr);
	noffH->initData.inFileAddr = WordToHost(noffH->initData.inFileAddr);
	noffH->uninitData.size = WordToHost(noffH->uninitData.size);
	noffH->uninitData.virtualAddr = WordToHost(noffH->uninitData.virtualAddr);
	noffH->uninitData.inFileAddr = WordToHost(noffH->uninitData.inFileAddr);
}

//----------------------------------------------------------------------
// ProcessAddressSpace::ProcessAddressSpace
// 	Create an address space to run a user program.
//	Load the program from a file "executable", and set everything
//	up so that we can start executing user instructions.
//
//	Assumes that the object code file is in NOFF format.
//
//	First, set up the translation from program memory to physical 
//	memory.  For now, this is really simple (1:1), since we are
//	only uniprogramming, and we have a single unsegmented page table
//
//	"executable" is the file containing the object code to load into memory
//----------------------------------------------------------------------

ProcessAddressSpace::ProcessAddressSpace(OpenFile *executable, char* f, int pd)
{
    // NoffHeader noffH;
    unsigned int i, size;
    unsigned vpn, offset;
    TranslationEntry *entry;
    unsigned int pageFrame;
    cpid = pd;
    filename = new char[1024];
    for (int i = 0; i < 1024; ++i)
    {
        filename[i] = f[i];
        if (f[i] == '\n' || f[i] == '\0')
            break;        
    }
    executable->ReadAt((char *)&noffH, sizeof(noffH), 0);
    if ((noffH.noffMagic != NOFFMAGIC) && 
		(WordToHost(noffH.noffMagic) == NOFFMAGIC))
    	SwapHeader(&noffH);
    ASSERT(noffH.noffMagic == NOFFMAGIC);

// how big is address space?
    size = noffH.code.size + noffH.initData.size + noffH.uninitData.size 
			+ UserStackSize;	// we need to increase the size
						// to leave room for the stack
    numVirtualPages = divRoundUp(size, PageSize);
    size = numVirtualPages * PageSize;

    //ASSERT(numVirtualPages+numPagesAllocated <= NumPhysPages);		// check we're not trying
										// to run anything too big --
										// at least until we have
										// virtual memory

    DEBUG('a', "Initializing address space, num pages %d, size %d\n", 
					numVirtualPages, size);
    backupArray = new char[size];
// first, set up the translation 
    KernelPageTable = new TranslationEntry[numVirtualPages];
    bzero(backupArray, size);
    for (i = 0; i < numVirtualPages; i++) {
   KernelPageTable[i].virtualPage = i;
    KernelPageTable[i].physicalPage = -1;
	KernelPageTable[i].valid = FALSE;
	KernelPageTable[i].use = FALSE;
	KernelPageTable[i].dirty = FALSE;
	KernelPageTable[i].readOnly = FALSE;  // if the code segment was entirely on 
					// a separate page, we could set its 
					// pages to be read-only
    KernelPageTable[i].shared = FALSE;
    KernelPageTable[i].loadFromSwap = FALSE;
    }
// zero out the entire address space, to zero the unitialized data segment 
// and the stack segment
    //bzero(&machine->mainMemory[numPagesAllocated*PageSize], size);
    //printf("sisze = %d\n", size);
 
 // We dont need this for demand paging
    // numPagesAllocated += numVirtualPages;

// // then, copy in the code and data segments into memory
//     if (noffH.code.size > 0) {
//         DEBUG('a', "Initializing code segment, at 0x%x, size %d\n",
// 			noffH.code.virtualAddr, noffH.code.size);
//         vpn = noffH.code.virtualAddr/PageSize;
//         offset = noffH.code.virtualAddr%PageSize;
//         entry = &KernelPageTable[vpn];
//         pageFrame = entry->physicalPage;
//         executable->ReadAt(&(machine->mainMemory[pageFrame * PageSize + offset]),
// 			noffH.code.size, noffH.code.inFileAddr);
//     }
//     if (noffH.initData.size > 0) {
//         DEBUG('a', "Initializing data segment, at 0x%x, size %d\n", 
// 			noffH.initData.virtualAddr, noffH.initData.size);
//         vpn = noffH.initData.virtualAddr/PageSize;
//         offset = noffH.initData.virtualAddr%PageSize;
//         entry = &KernelPageTable[vpn];
//         pageFrame = entry->physicalPage;
//         executable->ReadAt(&(machine->mainMemory[pageFrame * PageSize + offset]),
// 			noffH.initData.size, noffH.initData.inFileAddr);
//     }

}

//----------------------------------------------------------------------
// ProcessAddressSpace::ProcessAddressSpace (ProcessAddressSpace*) is called by a forked thread.
//      We need to duplicate the address space of the parent.
//----------------------------------------------------------------------

ProcessAddressSpace::ProcessAddressSpace(ProcessAddressSpace *parentSpace, int pd)
{
    numVirtualPages = parentSpace->GetNumPages();
    unsigned i, size = numVirtualPages * PageSize;
    cpid = pd;

    //ASSERT(numVirtualPages+numPagesAllocated <= NumPhysPages);                // check we're not trying
                                                                                // to run anything too big --
                                                                                // at least until we have
                                                                                // virtual memory

    DEBUG('a', "Initializing address space, num pages %d, size %d\n",
                                        numVirtualPages, size);
    // first, set up the translation
    TranslationEntry* parentPageTable = parentSpace->GetPageTable();
    KernelPageTable = new TranslationEntry[numVirtualPages];
    filename = new char[1024];
    noffH = parentSpace->noffH;
    for (int i = 0; i < 1024; ++i)
    {
        filename[i] = parentSpace->filename[i];
        if (filename[i] == '\0' || filename[i] == '\n')
            break;
    }
    int pagesAssigned = 0;

    for (i = 0; i < numVirtualPages; i++) {
        KernelPageTable[i].virtualPage = i;
        if (parentPageTable[i].shared) {
            KernelPageTable[i].physicalPage = parentPageTable[i].physicalPage;
        } else {
            if (parentPageTable[i].valid == TRUE){
                if(!replAlgo){
                    KernelPageTable[i].physicalPage = numPagesAllocated + pagesAssigned;
                    pagesAssigned += 1;
                }
                else {
                    KernelPageTable[i].physicalPage = -1;//GetPhysicalPage(i, parentPageTable[i].physicalPage);
                    /*int j = 0;
                    for (j = 0; j < PageSize; ++j)
                    {
                        machine->mainMemory[KernelPageTable[i].physicalPage*PageSize+j] = machine->mainMemory[parentPageTable[i].physicalPage*PageSize+j];
                    }*/
                }
            }
            else
                KernelPageTable[i].physicalPage = -1;
        }
        /*KernelPageTable[i].valid = parentPageTable[i].valid;
        KernelPageTable[i].use = parentPageTable[i].use;
        KernelPageTable[i].dirty = parentPageTable[i].dirty;
        KernelPageTable[i].readOnly = parentPageTable[i].readOnly;  	// if the code segment was entirely on
                                        			// a separate page, we could set its
                                        			// pages to be read-only
        KernelPageTable[i].shared = parentPageTable[i].shared;
        KernelPageTable[i].loadFromSwap = parentPageTable[i].loadFromSwap;*/
    }
    backupArray = new char[size];
    memcpy(backupArray, parentSpace->backupArray, size); // Copy parent swap to child

    // Copy the contents
    // unsigned startAddrParent = parentPageTable[0].physicalPage*PageSize;
    // unsigned startAddrChild = numPagesAllocated*PageSize;
    // unsigned copySize = numVirtualPages * pagesAssigned;
    // for (i=0; i<copySize; i++) {
    //    machine->mainMemory[startAddrChild+i] = machine->mainMemory[startAddrParent+i];
    // }
    int j;
    for (i = 0; i < numVirtualPages; i++)
    {
        if (parentPageTable[i].shared == FALSE && parentPageTable[i].valid == TRUE)
        {
            if(KernelPageTable[i].physicalPage == -1){ // If not allocated, then allocate a PPFN for the page
                //KernelPageTable[i].valid = parentPageTable[i].valid;
                KernelPageTable[i].physicalPage = GetPhysicalPage(i, parentPageTable[i].physicalPage);
            }
            for (j = 0; j < PageSize; ++j)
            {
                machine->mainMemory[KernelPageTable[i].physicalPage*PageSize+j] = machine->mainMemory[parentPageTable[i].physicalPage*PageSize+j];
            }
        }
        KernelPageTable[i].valid = parentPageTable[i].valid;
        KernelPageTable[i].use = parentPageTable[i].use;
        KernelPageTable[i].dirty = parentPageTable[i].dirty;
        KernelPageTable[i].readOnly = parentPageTable[i].readOnly;  	// if the code segment was entirely on
                                        			// a separate page, we could set its
                                        			// pages to be read-only
        KernelPageTable[i].shared = parentPageTable[i].shared;
        KernelPageTable[i].loadFromSwap = parentPageTable[i].loadFromSwap;

    }

    stats->totalPageFaults++;
    numPagesAllocated+=pagesAssigned;
    // TODO: Might have to do sorted insert in wait queue
}

//----------------------------------------------------------------------
// ProcessAddressSpace::~ProcessAddressSpace
// 	Dealloate an address space.  Nothing for now!
//----------------------------------------------------------------------

ProcessAddressSpace::~ProcessAddressSpace()
{
    for(int i = 0; i<numVirtualPages; i++){
        if(KernelPageTable[i].shared == FALSE && KernelPageTable[i].valid == TRUE){
            machine->threadPID[KernelPageTable[i].physicalPage] = -1;
            //thPID[KernelPageTable[i].physicalPage] = -1;
            machine->threadVPN[KernelPageTable[i].physicalPage] = -1;
            //thVPN[KernelPageTable[i].physicalPage] = -1;
            pagesAllocated--;
        }
    }
    //printf("#################################################################################lksjdfkljsdklfjsf");
    delete filename;
    delete KernelPageTable;
    delete backupArray;
}

//----------------------------------------------------------------------
// ProcessAddressSpace::InitUserModeCPURegisters
// 	Set the initial values for the user-level register set.
//
// 	We write these directly into the "machine" registers, so
//	that we can immediately jump to user code.  Note that these
//	will be saved/restored into the currentThread->userRegisters
//	when this thread is context switched out.
//----------------------------------------------------------------------

void
ProcessAddressSpace::InitUserModeCPURegisters()
{
    int i;

    for (i = 0; i < NumTotalRegs; i++)
	machine->WriteRegister(i, 0);

    // Initial program counter -- must be location of "Start"
    machine->WriteRegister(PCReg, 0);	

    // Need to also tell MIPS where next instruction is, because
    // of branch delay possibility
    machine->WriteRegister(NextPCReg, 4);

   // Set the stack register to the end of the address space, where we
   // allocated the stack; but subtract off a bit, to make sure we don't
   // accidentally reference off the end!
    machine->WriteRegister(StackReg, numVirtualPages * PageSize - 16);
    DEBUG('a', "Initializing stack register to %d\n", numVirtualPages * PageSize - 16);
}

//----------------------------------------------------------------------
// ProcessAddressSpace::SaveContextOnSwitch
// 	On a context switch, save any machine state, specificFork
//	to this address space, that needs saving.
//
//	For now, nothing!
//----------------------------------------------------------------------

void ProcessAddressSpace::SaveContextOnSwitch() 
{}

//----------------------------------------------------------------------
// ProcessAddressSpace::RestoreContextOnSwitch
// 	On a context switch, restore the machine state so that
//	this address space can run.
//
//      For now, tell the machine where to find the page table.
//----------------------------------------------------------------------

void ProcessAddressSpace::RestoreContextOnSwitch() 
{
    machine->KernelPageTable = KernelPageTable;
    machine->KernelPageTableSize = numVirtualPages;
}

unsigned
ProcessAddressSpace::GetNumPages()
{
   return numVirtualPages;
}

TranslationEntry*
ProcessAddressSpace::GetPageTable()
{
   return KernelPageTable;
}

unsigned
ProcessAddressSpace::sharedMemory(int numSharedPages)
{
    TranslationEntry* parentPageTable = GetPageTable();
    TranslationEntry* KernelPageTable1 = new TranslationEntry[numVirtualPages+numSharedPages];
    int i;
    for (i = 0; i < numVirtualPages; i++) {
        KernelPageTable1[i].virtualPage = i;
        KernelPageTable1[i].physicalPage = parentPageTable[i].physicalPage;
        KernelPageTable1[i].valid = parentPageTable[i].valid;
        KernelPageTable1[i].use = parentPageTable[i].use;
        KernelPageTable1[i].dirty = parentPageTable[i].dirty;
        KernelPageTable1[i].readOnly = parentPageTable[i].readOnly;  	// if the code segment was entirely on
                                        			// a separate page, we could set its
                                        			// pages to be read-only
        KernelPageTable1[i].shared = parentPageTable[i].shared;
    }
    for (i = numVirtualPages; i < numVirtualPages+numSharedPages; ++i)
    {
        KernelPageTable1[i].virtualPage = i;
        KernelPageTable1[i].physicalPage = GetPhysicalPage(i, -1); // TODO: Check
        KernelPageTable1[i].valid = TRUE;
	    KernelPageTable1[i].use = FALSE;
	    KernelPageTable1[i].dirty = FALSE;
	    KernelPageTable1[i].readOnly = FALSE;  // if the code segment was entirely on 
	    				// a separate page, we could set its 
	    				// pages to be read-only
        KernelPageTable1[i].shared = TRUE;
        machine->sharedPages[KernelPageTable[i].physicalPage] = TRUE;
        stats->totalPageFaults++;
    }
    numPagesAllocated += numSharedPages;
    pagesAllocated += numSharedPages;
    KernelPageTable = KernelPageTable1;
    unsigned virtualAddressStarting = numVirtualPages*PageSize;
    numVirtualPages += numSharedPages;
   RestoreContextOnSwitch();
    delete parentPageTable;
    return virtualAddressStarting;
}
unsigned
ProcessAddressSpace::GetPhysicalPage(unsigned vpn, int pageToIgnore)
{
      if(replAlgo == 0){
        printf("num alloc = %d\n", numPagesAllocated);
        ASSERT(numPagesAllocated < NumPhysPages);
        numPagesAllocated++;
        return numPagesAllocated-1;
    }

    //unsigned new_page = -1;
    for(int i = 0; i<NumPhysPages; i++){
        if(machine->threadPID[i] == -1 || machine->threadVPN[i] == -1){
            DEBUG('a', "Got empty page %d\n", i);
            machine->threadPID[i] = this->cpid;
            //thPID[i] = this->cpid;
            machine->threadVPN[i] = vpn;
            machine->referenceBit[i] = 1;
            if (replAlgo == 2) {
                        int *tmp = new int;
                        *tmp = i;
                        FIFOQueue->Append((void *)tmp);
                    }
            //thVPN[i] = vpn;
            pagesAllocated++;
            numPagesAllocated++;
            return i;
        }
    }

    DEBUG('a', "Going for page replacement\n");
    if(replAlgo == 1){
        return RandReplacement(vpn, pageToIgnore);
    }
    else if(replAlgo == 2){
           return FIFO_repl(vpn, pageToIgnore); 
         }
    else if(replAlgo == 3){
            return LRU_repl(vpn, pageToIgnore);
         }
    else if(replAlgo ==4){
          return LRU_Clock(vpn, pageToIgnore); 
       }
}

void
ProcessAddressSpace::CopyPageData(unsigned vpn, bool useNoffH)
{
    if (useNoffH)
    {
        unsigned startCopyAddr = vpn*PageSize, ppn = KernelPageTable[vpn].physicalPage;
        OpenFile *executable = fileSystem->Open(filename);
        //printf("noffH.code.virtualAddr: %d\n", noffH.code.virtualAddr);
        executable->ReadAt(&(machine->mainMemory[ppn * PageSize]), PageSize, noffH.code.inFileAddr + startCopyAddr - noffH.code.virtualAddr);
        delete executable;
        KernelPageTable[vpn].dirty = TRUE;
        memcpy(&(backupArray[vpn*PageSize]), &(machine->mainMemory[ppn*PageSize]), PageSize);
        KernelPageTable[vpn].loadFromSwap = TRUE;
        currentThread->SortedInsertInWaitQueue(1000+stats->totalTicks);
        stats->totalPageFaults++;
    }
    else {
        ASSERT(KernelPageTable[vpn].loadFromSwap == TRUE);

        //memcpy(&(machine->mainMemory[KernelPageTable[vpn].physicalPage * PageSize]), &(backupArray[vpn*PageSize]), PageSize); // Load data from backup
        for(int i = 0; i<PageSize; i++){
            machine->mainMemory[KernelPageTable[vpn].physicalPage*PageSize + i] = backupArray[vpn*PageSize + i];
        }
        KernelPageTable[vpn].dirty = TRUE;
        currentThread->SortedInsertInWaitQueue(1000+stats->totalTicks);
        stats->totalPageFaults++;
    }

}

void
ProcessAddressSpace::PageFaultHandler(unsigned vaddr)
{
    unsigned vpn = vaddr/PageSize;
    ASSERT(vpn <= numVirtualPages);
    unsigned ppn = GetPhysicalPage(vpn, -1);
    //printf("ppn:%d\n",ppn);

    KernelPageTable[vpn].virtualPage = vpn;
	KernelPageTable[vpn].physicalPage = ppn;
	KernelPageTable[vpn].valid = TRUE;
	KernelPageTable[vpn].use = FALSE;
	KernelPageTable[vpn].dirty = FALSE;
	KernelPageTable[vpn].readOnly = FALSE;  // if the code segment was entirely on 
					// a separate page, we could set its 
					// pages to be read-only
    KernelPageTable[vpn].shared = FALSE;
    DEBUG('a', "Page fault detected for %d virtual address vpn: %d. Loading Physical page into memory\n", vaddr, vpn);
    CopyPageData(vpn, !KernelPageTable[vpn].loadFromSwap);
    //RestoreContextOnSwitch();
    //pt();
}

void
ProcessAddressSpace::Backup(int vpn, int pd){
    //ASSERT(!exitThreadArray[pd]);
    ASSERT(this->KernelPageTable[vpn].valid == TRUE);

    if(this->KernelPageTable[vpn].dirty){
        memcpy(&(backupArray[vpn*PageSize]), &(machine->mainMemory[KernelPageTable[vpn].physicalPage*PageSize]), PageSize);
    }
    this->KernelPageTable[vpn].loadFromSwap = TRUE;
    this->KernelPageTable[vpn].valid = FALSE;
    this->KernelPageTable[vpn].dirty = FALSE;
    this->KernelPageTable[vpn].physicalPage = -1;
}


// Page Replacement Algorithms
unsigned
ProcessAddressSpace::RandReplacement(unsigned vpn, int pageToIgnore){
    unsigned new_ppn = Random()%(NumPhysPages);
    while(machine->sharedPages[new_ppn] == TRUE || new_ppn == pageToIgnore){
        new_ppn = Random()%(NumPhysPages);
    }
    //printf("New PPFN = %d\n", new_ppn);
    //printf("................\n");
    int pid = machine->threadPID[new_ppn]; // Part of inverse table
    /*if(threadArray[pid]->space->KernelPageTable[machine->threadVPN[new_ppn]].valid == FALSE){
        printf("########################## pid %d\n", machine->threadPID[new_ppn]);
        printf("mem dump\n");
        for(int i = 0; i<NumPhysPages; i++){
            printf("ppn=%d, pid=%d, vpn=%d\n", i, pid, machine->threadVPN[i]);
        }
    }*/
    //pt();
    if(threadArray[pid]->space != NULL)
        threadArray[pid]->space->Backup(machine->threadVPN[new_ppn], pid); // Save exiting page to backup
    else{
        this->Backup(machine->threadVPN[new_ppn], pid);
    }
    //KernelPageTable[vpn].loadFromSwap = TRUE;
    machine->threadPID[new_ppn] = cpid;
    //thPID[new_ppn] = cpid;
    machine->threadVPN[new_ppn] = vpn;
    //thVPN[new_ppn] = vpn;
    
    return new_ppn;
}

unsigned 
ProcessAddressSpace::FIFO_repl(int vpn, int pageToIgnore){
    int *tmp2=NULL,*tmp;
    int foundPage=-1;
    //printf("temp\n");
    tmp = (int *)FIFOQueue->Remove();
    foundPage = *tmp;
    while(machine->sharedPages[foundPage] == TRUE || foundPage == pageToIgnore){
      if (foundPage == pageToIgnore) {
                        tmp2 = tmp;
                        tmp = (int *)FIFOQueue->Remove();
                        foundPage = *tmp;
                    }
                    if(machine->sharedPages[foundPage]) {
                        delete tmp;
                        tmp = (int *)FIFOQueue->Remove();
                        foundPage = *tmp;
                    }
    }
    if(tmp2)
      FIFOQueue->Prepend((void *)tmp2);
    FIFOQueue->Append((void *)tmp);
    int pid = machine->threadPID[foundPage]; // Part of inverse table
    if(threadArray[pid]->space != NULL)
        threadArray[pid]->space->Backup(machine->threadVPN[foundPage], pid); // Save exiting page to backup
    else{
        this->Backup(machine->threadVPN[foundPage], pid);
    }
    machine->threadPID[foundPage] = cpid;
    machine->threadVPN[foundPage] = vpn;
    return foundPage; 
}

unsigned 
ProcessAddressSpace::LRU_repl(int vpn, int notToReplace){
     int foundPage=-1;
     long long int val = (1LL)<<60 -1;

      if(notToReplace!= -1) {
                    machine->LRUTimeStamp[notToReplace] = stats->totalTicks-1;
                }

                for(int i = 0; i<NumPhysPages; i++){
                    if( machine->LRUTimeStamp[i] < val  && machine->sharedPages[i] == FALSE  && i != notToReplace ){
                        foundPage = i;
                        val = machine->LRUTimeStamp[i];
                    }
                }
                ASSERT(foundPage != -1);
                machine->LRUTimeStamp[foundPage] = stats->totalTicks;
     int pid = machine->threadPID[foundPage]; // Part of inverse table
     if(threadArray[pid]->space != NULL)
         threadArray[pid]->space->Backup(machine->threadVPN[foundPage]    , pid); // Save exiting page to backup
     else{
         this->Backup(machine->threadVPN[foundPage], pid);
     }
     machine->threadPID[foundPage] = cpid;
     machine->threadVPN[foundPage] = vpn;
     return foundPage;
}

unsigned
ProcessAddressSpace::LRU_Clock(int vpn, int notToReplace){
  int foundPage=-1;          
  // printf("Entering clock lru replacement algorithm\n");
                while(machine->referenceBit[LRU_Clock_ptr] ||
                      machine->sharedPages[LRU_Clock_ptr] ||
                      LRU_Clock_ptr == notToReplace) {
                    machine->referenceBit[LRU_Clock_ptr] = 0;
                    LRU_Clock_ptr = (LRU_Clock_ptr+1)%NumPhysPages;
                }

                foundPage = LRU_Clock_ptr;
                machine->referenceBit[foundPage]=TRUE;
                LRU_Clock_ptr = (LRU_Clock_ptr+1)%NumPhysPages;
                int pid = machine->threadPID[foundPage]; // Part of inverse table
     if(threadArray[pid]->space != NULL)
         threadArray[pid]->space->Backup(machine->threadVPN[foundPage]    , pid); // Save exiting page to backup
     else{
         this->Backup(machine->threadVPN[foundPage], pid);
     }
     machine->threadPID[foundPage] = cpid;
     machine->threadVPN[foundPage] = vpn;
     return foundPage;
}
